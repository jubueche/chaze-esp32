/*Flash_training.h - Library to organize Training Data on Flash memory
   Created by Constantin Koch, May 23, 2018
*/
#include "ChazeFlashtraining.h"

SPIFlash Flashtraining::myflash;

//! Needs to be checked
Flashtraining::Flashtraining()
{
	_STATE = 0;
	_current_pagewritebuffer_position = 0;
	_current_page_writeposition = 0;
	_current_page_readposition = 0;
	_current_sector_eraseposition = 0;
}


bool Flashtraining::start_new_training()
{
	if (!_Check_or_Initialize()) return false;

	if (_STATE == 4) {
		for (int ii = 0; ii < 1000; ii += 10) {
			if (myflash.CheckErasing_inProgress() == 0) {
				//erste freie page wiederfinden
				_STATE = 5;
				_Check_or_Initialize();
				//Markierung setzen
				uint8_t bytes[1] = { 9 };
				_write_bytebuffer_toflash(bytes, sizeof(bytes));

				_STATE = 1;
				break;
			}
			delay(10);
		}
	}
	if (_STATE == 1) {
		uint8_t bytes[1] = { 1 };
		if (!_write_bytebuffer_toflash(bytes, sizeof(bytes)))return false;
		_STATE = 2;
		return true;
	}
	return false;
}

bool Flashtraining::stop_training()
{
	if (!_Check_or_Initialize()) return false;
	if (_STATE == 2) {
		uint8_t bytes[1] = { 2 };
		if (!_write_bytebuffer_toflash(bytes, sizeof(bytes)))return false;
		_STATE = 1;
		return true;
	}
	return false;
}



// TODO: Needs implementation

//! Use a temporary buffer of size 511 bytes that holds the bytes that were left over.
//! At the next call to write_compressed_chunk, preprend these bytes and repeat.
//! On stop_training, write the bytes in the temporary buffer to the flash.
bool Flashtraining::write_compressed_chunk(uint8_t * data, uint32_t n)
{
	// Abort if not initialized
	if (!_Check_or_Initialize()) return false;
	if(_STATE == 2)
	{
		// Can write since we started a training
		return _write_bytebuffer_toflash(data, n);
	}
	return false;

}

bool Flashtraining::set_name(char * name, uint8_t size)
{
	return true;
}

const char * Flashtraining::get_name()
{
	return "Julian'sChazeBand";
}

uint16_t Flashtraining::get_number_of_unsynched_trainings(){
	return 2;
}


int32_t Flashtraining::get_next_buffer_of_training(uint8_t * buff)
{
	if(config.random_between(0,100) < 75)
	{
		for(int i=0;i<UPLOAD_BLOCK_SIZE_BLE;i++){
			buff[i] = config.random_between(66,88);
		}
		ESP_LOGI(TAG, "Full chunk."); 
		return -1;
	} else {
		uint32_t n = config.random_between(20,100);
		for(int i =0; i<n; i++) {
			buff[i] = config.random_between(66,88);
		}
		ESP_LOGI(TAG, "Half full chunk of size %d.", n);
		return n;
	}
}


void Flashtraining::completed_synch_of_training(bool b)
{
	return;
}

const char * Flashtraining::get_device_id(void)
{
	return "chaze-3";
}
const char * Flashtraining::get_azure_connection_string(void)
{
	//return "HostName=chaze-iot-hub.azure-devices.net;DeviceId=device-2;SharedAccessKey=lMPIGSbbyrWlMUDDi0wPgOL+NaM8ATwB3rkUiQp4xH8=";
	return "HostName=chaze-iot-hub.azure-devices.net;DeviceId=chaze-3;SharedAccessKey=9vhufTtWrFy9vtucS1rzc4eKt9eIyfk6XnCoYIfI/7Y=";
}


const char * Flashtraining::get_wifi_ssid(void)
{
	return "Refhaus_Mitte/Zentral";
}


const char * Flashtraining::get_wifi_password(void)
{
	return "Amm+Tag+Vc+0"; 
}


bool Flashtraining::set_device_id(char * did)
{
	return true;
}

bool Flashtraining::set_azure_connection_string(const char * conn_string)
{
	return true;
}

bool Flashtraining::set_wifi_ssid(char * ssid, uint8_t size)
{
	return true;
}

bool Flashtraining::set_wifi_password(char * pass, uint8_t size)
{
	return true;
}

uint8_t Flashtraining::get_version(char * version_buffer)
{
	const char * v = "v1.3";
	for(int i=0;i<4; i++)
		version_buffer[i] = v[i];
	return 4;
}

bool Flashtraining::set_version(char * version, uint8_t size)
{
	return true;
}

// TODO: End Needs implementation

bool Flashtraining::start_delete_all_trainings()
{
	if (!_Check_or_Initialize()) return false;
	if (_STATE != 1)return false;

	_current_sector_eraseposition = 126;
	_STATE = 4;
	return true;
}

bool Flashtraining::start_reading_data() {
	if (!_Check_or_Initialize()) return false;
	if (_STATE != 1)return false;
	int _current_page_readposition = 0;
	_STATE = 3;
	return true;
}

bool Flashtraining::get_all_data_bufferwise(void *buf)
{
	if (!_Check_or_Initialize()) return false;
	if (_STATE != 3)return false;

	if (!myflash.readByteArray(_current_page_readposition * 512, _pagereadbuffer, 512)) return false;
	_current_page_readposition++;
	memcpy((char *)buf, _pagereadbuffer, 512);

	if (Flashtraining::_Check_buffer_contains_only_ff(_pagereadbuffer)) {
		_STATE = 1;
		return false;
	}
	else return true;
}

bool Flashtraining::abort_reading_data() {
	if (!_Check_or_Initialize()) return false;
	if (_STATE != 3)return false;
	_STATE = 1;
	return true;
}

void Flashtraining::please_call_every_loop() {
	_Check_or_Initialize();
	if (_STATE == 4) {
#ifdef DEBUG
		Serial.print("_current_sector_eraseposition: "); Serial.println(_current_sector_eraseposition);
#endif
		if (myflash.CheckErasing_inProgress() == 0) {
			myflash.readByteArray(_current_sector_eraseposition * 262144, _pagereadbuffer, 512);
			if (Flashtraining::_Check_buffer_contains_only_ff(_pagereadbuffer)) {
				//Endbedingung
				if (_current_sector_eraseposition == 0) {
					_STATE = 1;
					return;
				}
				_current_sector_eraseposition--;
			}
			else {
				myflash.eraseBlock256K(_current_sector_eraseposition * 262144);
			}
		}
	}
}

int Flashtraining::get_STATE() {
	return _STATE;
}

/*
 * @brief Possible adresses for storing a float: [0,256)
 */
bool Flashtraining::writeCalibration(float value, uint8_t storageaddress) {
	if (!_Check_or_Initialize()) return false;
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
		if (millis() > mm + 2000) {
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

float Flashtraining::readCalibration(uint8_t storageaddress) {
	if (!_Check_or_Initialize()) return false;
	//Adresse des ersten Bytes des letzten Sektors: 0 + 262144 * 127 = 33292288

	if (!myflash.readByteArray(33292288 + storageaddress * 512, _pagereadbuffer, 512)) return 0;

	long l_regenerated = 16777216 * _pagereadbuffer[0] + 65536 * _pagereadbuffer[1] + 256 * _pagereadbuffer[2] + _pagereadbuffer[3];
	if (_pagereadbuffer[0] == 255 && _pagereadbuffer[1] == 255 && _pagereadbuffer[2] == 255 && _pagereadbuffer[3] == 255) return 0;
	float float_regenerated = *(float*)&l_regenerated;
	return float_regenerated;
}

//PRIVATE ######################################################################

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
#ifdef DEBUG 
			Serial.print("ERROR "); Serial.println(_current_page_writeposition);
#endif
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

bool Flashtraining::_Check_or_Initialize() {
	if (_STATE == 0 || _STATE == 5) {
		if (_STATE == 0) {
			
			esp_err_t err = config.initialize_spi();

			if (!myflash.begin(SPIFlash_CS)) return false;

			//Validation
			uint32_t JEDEC = myflash.getJEDECID();
			if (uint8_t(JEDEC >> 16) != 1 || uint8_t(JEDEC >> 8) != 2 || uint8_t(JEDEC >> 0) != 25) return false;

			_STATE = 1;
		}
		//Suchen der ersten freien Page (zum Beschreiben)
		//zu suchen: Pages 0 bis 126 * 512 - 1 = 64511 
		_current_page_writeposition = 0;
		for (uint8_t i = 0; i <= 6; i++) {
			myflash.readByteArray(_current_page_writeposition * 512, _pagereadbuffer, 512);
			if (Flashtraining::_Check_buffer_contains_only_ff(_pagereadbuffer)) {
				if (_current_page_writeposition == 0) return true;
				_current_page_writeposition -= 10000;
				for (uint8_t ii = 0; ii < 10; ii++) {
					myflash.readByteArray(_current_page_writeposition * 512, _pagereadbuffer, 512);
					if (Flashtraining::_Check_buffer_contains_only_ff(_pagereadbuffer)) {
						_current_page_writeposition -= 1000;
						for (uint8_t iii = 0; iii < 10; iii++) {
							myflash.readByteArray(_current_page_writeposition * 512, _pagereadbuffer, 512);
							if (Flashtraining::_Check_buffer_contains_only_ff(_pagereadbuffer)) {
								_current_page_writeposition -= 100;
								for (uint8_t iiii = 0; iiii < 10; iiii++) {
									myflash.readByteArray(_current_page_writeposition * 512, _pagereadbuffer, 512);
									if (Flashtraining::_Check_buffer_contains_only_ff(_pagereadbuffer)) {
										_current_page_writeposition -= 10;
										for (uint8_t iiiii = 0; iiiii < 10; iiiii++) {
											myflash.readByteArray(_current_page_writeposition * 512, _pagereadbuffer, 512);
											if (Flashtraining::_Check_buffer_contains_only_ff(_pagereadbuffer)) {
												return true;
											}
											_current_page_writeposition++;
										}
									}
									_current_page_writeposition += 10;
								}
							}
							_current_page_writeposition += 100;
						}
					}
					_current_page_writeposition += 1000;
				}
			}
			_current_page_writeposition += 10000;
		}
		_current_page_writeposition = 130048;
	}
	return true;
}

bool Flashtraining::_Check_buffer_contains_only_ff(uint8_t *buffer) {
	bool only_ff = true;
	for (int i = 0; i < 512; i++) {
		if (buffer[i] != 255)only_ff = false;
	}
	return only_ff;
}
