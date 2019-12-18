/*Flash_training.h - Library to organize Training Data on Flash memory
   Created by Constantin Koch, May 23, 2018
*/
#ifndef FLASHTRAINING_H
#define FLASHTRAINING_H

#include "SPIFlash.h"
#include "Chaze_Realtime.h"
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
	bool meta_is_deleted(uint8_t trainindex);


    //READING TRAINING DATA
    bool start_reading_data(uint8_t trainindex);
	//Read the next page (512 bytes) from current training. If the returned value is false, the reading process has finished (in this case STATE is automatically resetted to "ready")
	bool get_next_buffer_of_training(uint8_t * buf);
	//the following function has to be called only when reading process is aborted externally (if you don´t call it is not possible to start a new training until the next reset)
	bool abort_reading_data();

    //ERASE TRAINING DATA
	bool init_delete_training(uint8_t trainindex);
    bool init_delete_all_trainings();
	//! Please call this method in an extra task, when there is "freetime" for the flash. You don't have to care about terminating it
	void erase_trainings_to_erase();

    //CALIBRATION STORAGE
    bool writeCalibration(uint8_t storageaddress, float value);
    float readCalibration(uint8_t storageaddress);	//default/error value is 0.0f

    //OTHER FUNCTIONS
	const char * get_device_ID(); //Returns device ID (OTP)
    int get_STATE();
    //1: ready
    //2: writing
    //3: reading
    //4: erasing
	//5: want to stop erasing OCCURS SHORTLY AND ONLY PRIVATE

  private:
    SPIFlash myflash;
    RV3028 rtc;
  
    const char * TAG = "Chaze-Flashtraining";

    bool _Check_buffer_contains_only_ff(uint8_t *buffer, uint32_t length);

	//meta data variables
	void ensure_metadata_validity();

	//write
	uint32_t _current_trainingindex;
	uint32_t _current_writeposition;		//Position, wo er noch nicht geschrieben hat
	//read
	uint8_t*  _pagereadbuffer;	//Brauche ich manchmal
	uint32_t _current_readposition;
	uint32_t _current_endposition;
	//erase
	uint32_t _current_eraseposition;
	void wait_for_erasing();

    int _STATE;
    //1: ready
    //2: writing
    //3: reading
    //4: erasing
	//5: want to stop erasing OCCURS SHORTLY AND ONLY PRIVATE
};

extern Flashtraining *global_ft;

#endif
