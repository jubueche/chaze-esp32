/*Flash_training.h - Library to organize Training Data on Flash memory
   Created by Constantin Koch, May 23, 2018
*/
#ifndef FLASHTRAINING_H
#define FLASHTRAINING_H

#include "SPIFlash.h"
#include "Configuration.h"
#include "nvs_flash.h"
#include "nvs.h"

//Necessary Defines:
//SPIFlash_SUPPLY
//SPIFlash_CS
//SPIFlash_RESET
//SPIFlash_WP
//SPIFlash_HOLD
/*#define SPIFlash_SUPPLY 7
#define SPIFlash_CS 14
#define SPIFlash_RESET 15
#define SPIFlash_WP 36
#define SPIFlash_HOLD 4*/

class Flashtraining
{ 
  static SPIFlash myflash; //Only declared, not initialized bc static
  static RV3028 rtc;

  public:
    Flashtraining();

	//The following functions return true if they were executed successfully

    //WRITE TRAINING
    bool start_new_training();
	bool write_compressed_chunk(uint8_t * data, uint32_t n);			//Write n chars to the flash (8192 bytes compressed to ~4000 bytes)
    bool stop_training();

	//TRAINING METADATA
	uint32_t meta_total_number_of_trainings();								//returns the total number of trainings (synced and nonsynced ones)									
	uint32_t meta_number_of_unsynced_trainings();							//trainindex: from 0 to meta_total_number_of_trainings() - 1
	uint32_t meta_get_training_size(uint8_t trainindex);					//returns the training size in bytes
	bool meta_get_training_starttime(uint8_t trainindex, uint8_t * buf);	//writes 5 bytes to buf: year (2019=19), month, date, hours, minutes
	bool meta_get_training_endtime(uint8_t trainindex, uint8_t * buf);		//writes 5 bytes to buf: year (2019=19), month, date, hours, minutes
	void meta_set_synced(uint8_t trainindex);
	bool meta_is_synced(uint8_t trainindex);


    //READING TRAINING DATA
    bool start_reading_data(uint8_t trainindex);
	//Read the next page (512 bytes) from current training. If the returned value is false, the reading process has finished (in this case STATE is automatically resetted to "ready")
	bool get_next_buffer_of_training(uint8_t * buf);
	//the following function has to be called only when reading process is aborted externally (if you don´t call it is not possible to start a new training until the next reset)
	bool abort_reading_data();

    //ERASE TRAINING DATA
	bool delete_training(uint8_t trainindex);
    bool start_delete_all_trainings();

    //CALIBRATION STORAGE
    bool writeCalibration(float value, uint8_t storageaddress);
    float readCalibration(uint8_t storageaddress);	//default/error value is 0.0f

    //OTHER FUNCTIONS
	char * get_device_ID(); //Returns device ID (OTP)
    int get_STATE();
    //1: ready
    //2: writing
    //3: reading
    //4: erasing
	//5: want to find new page start (after erasing) OCCURS SHORTLY AND ONLY PRIVATE

  private:
    const char * TAG = "Chaze-Flashtraining";

    bool _Check_buffer_contains_only_ff(uint8_t  *buffer);

	//meta data variables
	void ensure_metadata_validity();
	uint32_t _current_startaddress, _current_endaddress;
	uint8_t _current_startyear, _current_startmonth, _current_startdate, _current_starthour, _current_startminute;
	uint8_t _current_endyear, _current_endmonth, _current_enddate, _current_endhour, _current_endminute;

	//write
	uint8_t _pagewritebuffer[512];//TODO: dummes kopieren?
	int _current_page_writeposition;		//Position, wo er noch nicht geschrieben hat
	bool _write_bytebuffer_toflash(uint8_t *buffer, uint8_t  buflenght);
	//read
	uint8_t  _pagereadbuffer[512];//TODO: dummes kopieren?
	int _current_page_readposition;
	//erase
	void erase_trainings_to_erase();
	int _current_sector_eraseposition;

    int _STATE;
    //1: ready
    //2: writing
    //3: reading
    //4: erasing
	//5: want to find new page start (after erasing) OCCURS SHORTLY AND ONLY PRIVATE
};

extern Flashtraining *global_ft;

#endif

