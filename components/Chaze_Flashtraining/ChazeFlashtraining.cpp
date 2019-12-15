/*Flash_training.h - Library to organize Training Data on Flash memory
   Created by Constantin Koch, May 23, 2018
*/
#include "ChazeFlashtraining.h"
#include "string.h"

#define META_STARTADDR 33030144
#define CALIB_STARTADDR 33292288

Flashtraining *global_ft = new Flashtraining();

SPIFlash Flashtraining::myflash;
RV3028 Flashtraining::rtc;

//CONSTRUCTOR
Flashtraining::Flashtraining()	//TEST
{
	esp_err_t err = config.initialize_spi();

	if (!myflash.begin(SPIFlash_CS)) ESP_LOGI(TAG, " Error in SPIFlash.begin");
	if (!rtc.begin()) ESP_LOGI(TAG, " Error in rtc.begin");

	//Validation
	uint32_t JEDEC = myflash.getJEDECID();
	if (uint8_t(JEDEC >> 16) != 1 || uint8_t(JEDEC >> 8) != 2 || uint8_t(JEDEC >> 0) != 25) ESP_LOGI(TAG, " Error in JEDEC validation");

	_current_trainingdex = 0;
	_current_writeposition = 0;
	_current_readposition = 0;
	uint32_t _current_endposition; = 0;
	_current_sector_eraseposition = 0;

	_STATE = 1;

	ensure_metadata_validity();
}


//WRITING
bool Flashtraining::start_new_training()	//TEST
{
	wait_for_erasing();
	if (_STATE != 1)
		return false;

	//Find next free trainindex
	_current_trainingdex = meta_total_number_of_trainings();

	//Find next free startaddress
	uint32_t* endaddr = new uint32_t();
	_current_writeposition = 0;
	for (uint32_t i = 0; i < _current_trainingdex; i++)
	{
		myflash.readByteArray(META_STARTADDR + i * 512 + 4, (uint8_t*)endaddr, 4);
		_current_writeposition = max(_current_writeposition, (*endaddr));
	}
	//Start of next sector
	if (_current_writeposition % 262144 != 0)
		_current_writeposition = (_current_writeposition / 262144) * 262144 + 262144;
	if (_current_writeposition > META_STARTADDR) return false;

	//Write Metadata
	//Write Startaddress
	myflash.writeByteArray(META_STARTADDR + _current_trainindex * 512, (uint8_t*)&_current_writeposition, 4);

	//Write starttime
	rtc.updateTime();
	uint8_t* start_time = new uint8_t[5]{ (rtc.getYear() - 2000), rtc.getMonth(), rtc.getDate(), rtc.getHours(), rtc.getMinutes() };
	myflash.writeByteArray(META_STARTADDR + _current_trainindex * 512 + 8, start_time, 5);

	_STATE = 2;
	return true;
}

//! Use a temporary buffer of size 511 bytes that holds the bytes that were left over.
//! At the next call to write_compressed_chunk, preprend these bytes and repeat.
//! On stop_training, write the bytes in the temporary buffer to the flash.
bool Flashtraining::write_compressed_chunk(uint8_t * data, uint32_t n)	//TEST
{
	wait_for_erasing();
	if (_STATE != 2)
		return false;
	if (_current_writeposition + n > META_STARTADDR) {
		stop_training();
		return false;
	}

	if (!myflash.writeByteArray(_current_writeposition, data, n)) {
		ESP_LOGI(TAG, " Error writing at: %d:", _current_writeposition);
		return false;
	}
	_current_writeposition += n;

	return true;
}

bool Flashtraining::stop_training()	//TEST
{
	wait_for_erasing();
	if (_STATE != 2)
		return false;

	//Write Metadata
	//Write Endaddress
	myflash.writeByteArray(META_STARTADDR + _current_trainindex * 512 + 4, (uint8_t*)&_current_writeposition, 4);

	//Write Endtime
	rtc.updateTime();
	uint8_t* end_time = new uint8_t[5]{ (rtc.getYear() - 2000), rtc.getMonth(), rtc.getDate(), rtc.getHours(), rtc.getMinutes() };
	myflash.writeByteArray(META_STARTADDR + _current_trainindex * 512 + 13, end_time, 5);

	_STATE = 1;

	return true;
}


//METADATA
uint32_t Flashtraining::meta_total_number_of_trainings() //TEST
{
	wait_for_erasing();
	for (uint32_t i = 0; i < 512; i++)
	{
		myflash.readByteArray(META_STARTADDR + i * 512, _pagereadbuffer, 25);
		if (_Check_buffer_contains_only_ff(_pagereadbuffer, 25))
			return i;
	}
	return 512;
}
uint32_t Flashtraining::meta_number_of_unsynced_trainings() //TEST
{
	wait_for_erasing();
	uint32_t total = meta_total_number_of_trainings();
	uint32_t count = 0;

	for (uint32_t i = 0; i < total; i++) {
		//Check synced flag
		if (myflash.readByte(META_STARTADDR + i * 512 + 18) == 255)
			count++;
	}

	return count;
}
uint32_t Flashtraining::meta_get_training_size(uint8_t trainindex)	//TEST
{
	wait_for_erasing();
	uint32_t* start = new uint32_t();
	uint32_t* end = new uint32_t();
	myflash.readByteArray(META_STARTADDR + trainindex * 512 + 0, (uint8_t*)start, 4);
	myflash.readByteArray(META_STARTADDR + trainindex * 512 + 4, (uint8_t*)end, 4);

	return (*end) - (*start);
}
bool Flashtraining::meta_get_training_starttime(uint8_t trainindex, uint8_t * buf)	//TEST
{
	wait_for_erasing();
	return myflash.readByteArray(META_STARTADDR + trainindex * 512 + 8, buf, 5);
}
bool Flashtraining::meta_get_training_endtime(uint8_t trainindex, uint8_t * buf)	//TEST
{
	wait_for_erasing();
	return myflash.readByteArray(META_STARTADDR + trainindex * 512 + 13, buf, 5);
}
void Flashtraining::meta_set_synced(uint8_t trainindex) 	//TEST
{
	wait_for_erasing();
	myflash.writeByte(META_STARTADDR + trainindex * 512 + 18, 1);
}
bool Flashtraining::meta_is_synced(uint8_t trainindex) 	//TEST
{
	wait_for_erasing();
	return (myflash.readByte(META_STARTADDR + trainindex * 512 + 18) == 1);
}
bool Flashtraining::meta_is_deleted(uint8_t trainindex) 	//TEST
{
	wait_for_erasing();
	return (myflash.readByte(META_STARTADDR + trainindex * 512 + 19) == 1);
}


//READING
bool Flashtraining::start_reading_data(uint8_t trainindex) 	//TEST
{
	wait_for_erasing();
	if (_STATE != 1) return false;

	//get startaddress
	if (!myflash.readByteArray(META_STARTADDR + trainindex * 512 + 0, (uint8_t*)&_current_readposition, 4)) return false;
	if (_Check_buffer_contains_only_ff(&_current_readposition, 4) return false;

	//get current endaddress
	if (!myflash.readByteArray(META_STARTADDR + trainindex * 512 + 4, (uint8_t*)&_current_endposition, 4)) return false;
	if (_Check_buffer_contains_only_ff(&_current_endposition, 4) return false;

	_STATE = 3;
	return true;
}

bool Flashtraining::get_next_buffer_of_training(uint8_t * buf)	//TEST
{
	wait_for_erasing();
	if (_STATE != 3) return false;

	if (!myflash.readByteArray(_current_readposition, buf, 512)) return false;
	_current_readposition += 512;

	if (_current_readposition >= _current_endposition) {
		_STATE = 1;
		return false;
	}

	return true;
}

bool Flashtraining::abort_reading_data() 	//TEST
{
	wait_for_erasing();
	if (_STATE != 3) return false;
	_STATE = 1;
	return true;
}


//ERASING
bool Flashtraining::init_delete_training(uint8_t trainindex) 	//TEST
{
	wait_for_erasing();
	if (_STATE != 1) return false;
	myflash.writeByte(META_STARTADDR + trainindex * 512 + 18, 1);
	myflash.writeByte(META_STARTADDR + trainindex * 512 + 19, 1);

	return true;
}

bool Flashtraining::init_delete_all_trainings()	//TEST
{
	wait_for_erasing();
	if (_STATE != 1) return false;

	uint32_t total = meta_total_number_of_trainings();

	for (uint32_t i = 0; i < total; i++)
	{
		init_delete_training(i);
	}

	return true;
}

void Flashtraining::erase_trainings_to_erase() //TEST
{
	if (_STATE != 1) return;

	uint32_t total = meta_total_number_of_trainings();

	for (uint32_t i = 0; i < total; i++) {
		//		should delete this training?							//	is it not yet deleted?
		if ((myflash.readByte(META_STARTADDR + i * 512 + 19) == 1) && (myflash.readByte(META_STARTADDR + i * 512 + 20) != 1)) {
			//Delete all Sectors of this training
			//Get start- and end address
			uint32_t start = 0;
			uint32_t end = 0;
			myflash.readByteArray(META_STARTADDR + i * 512 + 0, (uint8_t*)&start, 4);
			myflash.readByteArray(META_STARTADDR + i * 512 + 4, (uint8_t*)&end, 4);
			while (start < end) {
				//Terminate prematurely?
				if (_STATE == 5) { _STATE = 1; return; }
				//Erase Sector
				myflash.eraseBlock256K(start);
				int mm = millis();
				while (myflash.CheckErasing_inProgress()) { delay(10);  if (millis() > mm + 4000) { _STATE = 4; return; } }
			}
			//Set "deleted" flag
			myflash.writeByte(META_STARTADDR + i * 512 + 20, 1);

		}
	}

	//If we get here and all meta-trainings are "deleted", we can erase the meta data sector
	bool all_deleted = true;
	for (uint32_t i = 0; i < total; i++) {
		if (myflash.readByte(META_STARTADDR + i * 512 + 20) != 1) all_deleted = false;
	}
	if(all_deleted)
		myflash.eraseBlock256K(META_STARTADDR);
	while (myflash.CheckErasing_inProgress()) { delay(10); if (millis() > mm + 4000) break; }

	_STATE = 1;
}

//CALIBRATION STORAGE
/*
 * @brief Possible adresses for storing a float: [0,256)
 */
bool Flashtraining::writeCalibration(float value, uint8_t storageaddress)	//TEST
{
	wait_for_erasing();
	//Adresse des ersten Bytes des letzten Sektors: 0 + 262144 * 127 = 33292288	(CALIB_STARTADDR)

	//selbst im Fehlerfalle soll erst ganz am Ende returnt werden, damit die Calibration noch gerettet werden kann
	bool fehlerfrei = true;

	//Zwischenspeicher
	float zwischenfloat[256];
	for (int zwi = 0; zwi < 256; zwi++) {
		zwischenfloat[zwi] = readCalibration(zwi);
	}
	//Wert verändern
	zwischenfloat[storageaddress] = value;

	//Sector Protection freigeben und Erasen
	if (!myflash.ASP_release_all_sectors()) fehlerfrei = false;
	delayMicroseconds(100);
	if (!myflash.eraseBlock256K(CALIB_STARTADDR)) fehlerfrei = false;
	int mm = millis();
	while (myflash.CheckErasing_inProgress()) {
		if (millis() > mm + 4000) {
			fehlerfrei = false; break;
		}
	}

	//alle Werte reinschreiben
	for (int zwii = 0; zwii < 256; zwii++) {
		long l = *(long*)&zwischenfloat[zwii];
		uint8_t barr[4];
		barr[0] = l >> 24;             //linkeste Bits von l
		barr[1] = l >> 16;
		barr[2] = l >> 8;
		barr[3] = l;                   //rechteste Bits von l
		if (!(myflash.writeByteArray(CALIB_STARTADDR + zwii * 512, &barr[0], 4)))fehlerfrei = false;
	}

	//Sector protecten
	if (!myflash.ASP_PPB_protect_sector(CALIB_STARTADDR))fehlerfrei = false;

	//Protection überprüfen
	if (myflash.ASP_PPB_read_Access_Register(0) != 255)fehlerfrei = false;
	if (myflash.ASP_PPB_read_Access_Register(CALIB_STARTADDR) != 0)fehlerfrei = false;

	delete[] zwischenfloat;

	return fehlerfrei;
}

float Flashtraining::readCalibration(uint8_t storageaddress)	//TEST
{
	wait_for_erasing();
	//Adresse des ersten Bytes des letzten Sektors: 0 + 262144 * 127 = 33292288		(CALIB_STARTADDR)

	if (!myflash.readByteArray(CALIB_STARTADDR + storageaddress * 512, _pagereadbuffer, 4)) return 0;

	long l_regenerated = 16777216 * _pagereadbuffer[0] + 65536 * _pagereadbuffer[1] + 256 * _pagereadbuffer[2] + _pagereadbuffer[3];	//TODO: geht besser
	if (_pagereadbuffer[0] == 255 && _pagereadbuffer[1] == 255 && _pagereadbuffer[2] == 255 && _pagereadbuffer[3] == 255) return 0;
	float float_regenerated = *(float*)&l_regenerated;
	return float_regenerated;
}


//OTHER FUNCTIONS
const char * Flashtraining::get_device_ID() {	//TEST
	return "Prototype v3.0";
}

int Flashtraining::get_STATE() {	//GOOD
	return _STATE;
}




//PRIVATE ######################################################################

bool Flashtraining::_Check_buffer_contains_only_ff(uint8_t *buffer, uint32_t length) 	//TEST
{
	bool only_ff = true;
	for (int i = 0; i < length; i++) {
		if (buffer[i] != 255) only_ff = false;
	}
	return only_ff;
}

void Flashtraining::ensure_metadata_validity()  //TEST
{
	wait_for_erasing();
	uint32_t total = meta_total_number_of_trainings();

	bool errorfree = true;

	for (uint32_t i = 0; i < total; i++)
	{
		if (meta_get_training_size(i) > META_STARTADDR) errorfree = false;
		if (!myflash.readByteArray(META_STARTADDR + i * 512 + 4, (uint8_t*)&_current_endposition, 4)) errorfree = false;
		if (_current_endposition == UINT32_MAX) errorfree = false;
	}
	_current_endposition = 0;

	if (errorfree) return;

	//FIND LOWEST FREE SECTOR
	uint32_t lowest_free = META_STARTADDR;
	for (uint32_t i = META_STARTADDR; 0 < i; i -= 262144) {
		lowest_free = i;
		myflash.ReadByteArray(i - 262144, _pagereadbuffer, 512);
		if (!_Check_buffer_contains_only_ff(&_pagereadbuffer, 512))
			break;
	}

	//Write this lowest free sector as end adress for incomplete trainings
	for (uint32_t i = 0; i < total; i++)
	{
		myflash.readByteArray(META_STARTADDR + i * 512 + 4, (uint8_t*)&_current_endposition, 4);
		if (_current_endposition == UINT32_MAX)
			myflash.writeByteArray(META_STARTADDR + i * 512 + 4, (uint8_t*)&lowest_free, 4);
	}
	_current_endposition = 0;
}

void Flashtraining::wait_for_erasing()	//TEST
{
	if (_STATE != 4 && _STATE != 5) return;

	_STATE = 5;
	while (_STATE != 1) delay(10);
	return;
}

int max(const int& a, const int& b) {
	return (a < b) ? b : a;
}
