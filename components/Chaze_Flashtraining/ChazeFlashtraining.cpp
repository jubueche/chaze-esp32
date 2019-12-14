/*Flash_training.h - Library to organize Training Data on Flash memory
   Created by Constantin Koch, May 23, 2018
*/
#include "ChazeFlashtraining.h"
#include "string.h"

Flashtraining *global_ft = new Flashtraining();

SPIFlash Flashtraining::myflash;
RV3028 Flashtraining::rtc;

//CONSTRUCTOR
Flashtraining::Flashtraining()	//TODO: validate metadata
{
	esp_err_t err = config.initialize_spi();

	if (!myflash.begin(SPIFlash_CS)) ESP_LOGI(TAG, " Error in SPIFlash.begin");
	if (!rtc.begin()) ESP_LOGI(TAG, " Error in rtc.begin");

	//Validation
	uint32_t JEDEC = myflash.getJEDECID();
	if (uint8_t(JEDEC >> 16) != 1 || uint8_t(JEDEC >> 8) != 2 || uint8_t(JEDEC >> 0) != 25) ESP_LOGI(TAG, " Error in JEDEC validation");

	_current_pagewritebuffer_position = 0;
	_current_page_writeposition = 0;
	_current_page_readposition = 0;
	_current_sector_eraseposition = 0;
	
	_STATE = 1;
}


//WRITING
bool Flashtraining::start_new_training()	//ALMOST OLDGOOD (außer Startposition finden)
{
	if (_STATE == 1) {
		//TODO: _current_page_writeposition ...
		_current_startaddress = _current_page_writeposition;

		rtc.updateTime();
		_current_startyear = (rtc.getYear() - 2000);
		_current_startmonth = rtc.getMonth();
		_current_startdate = rtc.getDate();
		_current_starthour = rtc.getHours();
		_current_startminute = rtc.getMinutes();

		_STATE = 2;
		return true;
	}
	return false;
}

//! Use a temporary buffer of size 511 bytes that holds the bytes that were left over.
//! At the next call to write_compressed_chunk, preprend these bytes and repeat.
//! On stop_training, write the bytes in the temporary buffer to the flash.
bool Flashtraining::write_compressed_chunk(uint8_t * data, uint32_t n)
{
	if (_STATE == 2)
	{
		// Can write since we started a training
		return _write_bytebuffer_toflash(data, n);
	}
	return false;
}

bool Flashtraining::stop_training()
{
	if (_STATE == 2) {
		_current_endaddress = _current_page_writeposition;
		rtc.updateTime();
		_current_endyear = (rtc.getYear() - 2000);
		_current_endmonth = rtc.getMonth();
		_current_enddate = rtc.getDate();
		_current_endhour = rtc.getHours();
		_current_endminute = rtc.getMinutes();

		//private metadata variables are now filled. Let's pack them to a buffer
		_pagewritebuffer[0] = _current_startaddress >> 24;
		_pagewritebuffer[1] = current_startaddress >> 16;
		_pagewritebuffer[2] = current_startaddress >> 8;
		_pagewritebuffer[3] = current_startaddress;
		_pagewritebuffer[4] = _current_endaddress >> 24;
		_pagewritebuffer[5] = _current_endaddress >> 16;
		_pagewritebuffer[6] = _current_endaddress >> 8;
		_pagewritebuffer[7] = _current_endaddress;
		_pagewritebuffer[8] = _current_startyear;
		_pagewritebuffer[9] = _current_startmonth;
		_pagewritebuffer[10] = _current_startdate;
		_pagewritebuffer[11] = _current_starthour;
		_pagewritebuffer[12] = _current_startminute;
		_pagewritebuffer[13] = _current_endyear;
		_pagewritebuffer[14] = _current_endmonth;
		_pagewritebuffer[15] = _current_enddate;
		_pagewritebuffer[16] = _current_endhour;
		_pagewritebuffer[17] = _current_endminute;

		//find a free page in metadata sector to write the metadata of the finishing training
		//address of the first byte in metadata sector: 0 + 262144 * 126 = 33030144
		for (int metaaddress = 33030144; metaaddress < 33030144 + 512 * 512; metaaddress += 512) {
			if (!myflash.readByteArray(metaaddress, _pagereadbuffer, 512)) return false;
			if (_Check_buffer_contains_only_ff(_pagereadbuffer, 512)) {
				if (!(myflash.writeByteArray(metaaddress, &_pagewritebuffer[0], 512))) return false;
				_STATE = 1;
				return true;
			}
		}
	}
	return false;
}


//METADATA
uint32_t Flashtraining::meta_total_number_of_trainings() {

}
uint32_t Flashtraining::meta_number_of_unsynced_trainings() {

}
uint32_t Flashtraining::meta_get_training_size(uint8_t trainindex) {

}
bool Flashtraining::meta_get_training_starttime(uint8_t trainindex, uint8_t * buf) {

}
bool Flashtraining::meta_get_training_endtime(uint8_t trainindex, uint8_t * buf) {

}
void Flashtraining::meta_set_synced(uint8_t trainindex) {

}
bool Flashtraining::meta_is_synced(uint8_t trainindex) {

}


//READING
bool Flashtraining::start_reading_data(uint8_t trainindex) {
	if (_STATE != 1)return false;
	int _current_page_readposition = 0;
	_STATE = 3;
	return true;
}

bool Flashtraining::get_next_buffer_of_training(uint8_t * buf)
{
	if (_STATE != 3)return false;

	if (!myflash.readByteArray(_current_page_readposition * 512, _pagereadbuffer, 512)) return false;
	_current_page_readposition++;
	memcpy((char *)buf, _pagereadbuffer, 512);

	if (Flashtraining::_Check_buffer_contains_only_ff(_pagereadbuffer, 512)) {
		_STATE = 1;
		return false;
	}
	else return true;
}

bool Flashtraining::abort_reading_data() {
	if (_STATE != 3)return false;
	_STATE = 1;
	return true;
}


//ERASING
bool Flashtraining::delete_training(uint8_t trainindex) {

}

bool Flashtraining::start_delete_all_trainings()
{
	if (_STATE != 1)return false;

	_current_sector_eraseposition = 126;
	_STATE = 4;
	return true;
}


//CALIBRATION STORAGE
/*
 * @brief Possible adresses for storing a float: [0,256)
 */
bool Flashtraining::writeCalibration(float value, uint8_t storageaddress) {	//OLDGOOD
	//Adresse des ersten Bytes des letzten Sektors: 0 + 262144 * 127 = 33292288

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
	if (!myflash.eraseBlock256K(33292288)) fehlerfrei = false;
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
		if (!(myflash.writeByteArray(33292288 + zwii * 512, &barr[0], 4)))fehlerfrei = false;
	}

	//Sector protecten
	if (!myflash.ASP_PPB_protect_sector(33292288))fehlerfrei = false;

	//Protection überprüfen
	if (myflash.ASP_PPB_read_Access_Register(0) != 255)fehlerfrei = false;
	if (myflash.ASP_PPB_read_Access_Register(33292288) != 0)fehlerfrei = false;


	return fehlerfrei;
}

float Flashtraining::readCalibration(uint8_t storageaddress) {	//OLDGOOD
	//Adresse des ersten Bytes des letzten Sektors: 0 + 262144 * 127 = 33292288

	if (!myflash.readByteArray(33292288 + storageaddress * 512, _pagereadbuffer, 512)) return 0;

	long l_regenerated = 16777216 * _pagereadbuffer[0] + 65536 * _pagereadbuffer[1] + 256 * _pagereadbuffer[2] + _pagereadbuffer[3];
	if (_pagereadbuffer[0] == 255 && _pagereadbuffer[1] == 255 && _pagereadbuffer[2] == 255 && _pagereadbuffer[3] == 255) return 0;
	float float_regenerated = *(float*)&l_regenerated;
	return float_regenerated;
}


//OTHER FUNCTIONS
char * Flashtraining::get_device_ID() {	//TEST
	return "Prototype v3.0";
}

int Flashtraining::get_STATE() {	//GOOD
	return _STATE;
}




//PRIVATE ######################################################################

bool Flashtraining::_Check_buffer_contains_only_ff(uint8_t *buffer) {
	bool only_ff = true;
	for (int i = 0; i < 512; i++) {
		if (buffer[i] != 255)only_ff = false;
	}
	return only_ff;
}

void Flashtraining::ensure_metadata_validity() {

}

bool Flashtraining::_write_bytebuffer_toflash(uint8_t *buffer, uint8_t buflenght) {
	//uint8_t _pagewritebuffer[512];
	//int _current_pagewritebuffer_position = 0;  //Position, wo er noch nicht geschrieben hat
	//int _current_page_writeposition = 0;      //Position, wo er noch nicht geschrieben hat

	//pagewritebuffer einfach weiter schreiben
	if (_current_pagewritebuffer_position + buflenght <= 512) {
		memcpy(&_pagewritebuffer[_current_pagewritebuffer_position], buffer, buflenght);
		_current_pagewritebuffer_position += buflenght;
		//für wenn der buffer für "STOP TRAINING" steht
		if (buflenght == 1) {
			if (buffer[0] == 2) {
				if (!myflash.writeByteArray(_current_page_writeposition * 512, &_pagewritebuffer[0], 512))return false;
				_current_page_writeposition++;
				for (int i = 0; i < 512; i++) {
					_pagewritebuffer[i] = 255;
				}
				_current_pagewritebuffer_position = 0;
			}
		}
	}
	//pagewritebuffer in Page schreiben und buffer zurücksetzen
	else {
		if (!myflash.writeByteArray(_current_page_writeposition * 512, &_pagewritebuffer[0], 512)) {
			ESP_LOGI(TAG, " Error writing at: %d:", _current_page_writeposition);
			return false;
		}
		_current_page_writeposition++;
		for (int i = 0; i < 512; i++) {
			_pagewritebuffer[i] = 255;
		}
		_current_pagewritebuffer_position = 0;
		//pagewritebuffer jetzt weiter schreiben
		memcpy(&_pagewritebuffer[_current_pagewritebuffer_position], buffer, buflenght);
		_current_pagewritebuffer_position += buflenght;
	}
	return true;
}

void Flashtraining::erase_trainings_to_erase() {

}
