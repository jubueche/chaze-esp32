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
  public:
    //The following functions return true if they were executed successfully
    Flashtraining(void);

    //WRITE TRAINING
    bool start_new_training();
    bool stop_training();

    // Added by Julian Buechel; Writes n chars to the flash.
    bool initialize_flash();
    //! Needs to be tested and implemented
    bool write_compressed_chunk(uint8_t * data, uint32_t n);
    uint16_t get_number_of_unsynched_trainings(void);
    void set_number_of_unsynched_trainings(uint16_t);
    void add_unsynched_training();
    void remove_unsynched_training();
    int32_t get_next_buffer_of_training(uint8_t *); // Takes pointer to buffer. Returns -1 if wrote 512 bytes else the number of bytes written.
    void completed_synch_of_training(bool); // Passes a boolean indicating whether the training was successfully synched.
    const char * get_device_id(void); //Returns device ID
    const char * get_azure_connection_string(void); // Returns connection string for Azure.
    const char * get_wifi_ssid(void);
    const char * get_wifi_password(void);
    bool set_device_id(char *); //Sets device ID
    bool set_azure_connection_string(const char *); // Sets connection string for Azure.
    bool set_wifi_ssid(char *, uint8_t);
    bool set_wifi_password(char *, uint8_t);
    bool set_name(char *, uint8_t);
    const char * get_name(void);
    uint8_t get_version(char *);
    bool set_version(char *, uint8_t);



    //READING TRAINING DATA
    bool start_reading_data();
    //In the following function, buflenght MUST be 512. If the returned value is false, the reading process has finished (in this case STATE is automatically resetted to "ready")
    bool get_all_data_bufferwise(void *buf);
    //the following function has to be called only when reading process is aborted externally (if you don´t call it is not possible to start a new training until the next reset)
    bool abort_reading_data();

    //ERASE TRAINING DATA
    bool start_delete_all_trainings();

    //CALIBRATION STORAGE
    bool writeCalibration(float value, uint8_t storageaddress);
    float readCalibration(uint8_t storageaddress);	//default/error value is 0.0f

    //OTHER FUNCTIONS
    void please_call_every_loop();
    int get_STATE();
    //0: not initialized
    //1: ready
    //2: writing
    //3: reading
    //4: erasing
	//5: want to find new page start (after erasing) OCCURS SHORTLY AND ONLY PRIVATE

  private:
    const char * TAG = "Chaze-Flashtraining";

    bool _Check_or_Initialize();
    bool _Check_buffer_contains_only_ff(uint8_t  *buffer);
    //write
    uint8_t _pagewritebuffer[512];
    int _current_pagewritebuffer_position;	//Position, wo er noch nicht geschrieben hat
    int _current_page_writeposition;		//Position, wo er noch nicht geschrieben hat
    bool _write_bytebuffer_toflash(uint8_t *buffer, uint8_t  buflenght);
    //read
    uint8_t  _pagereadbuffer[512];
    int _current_page_readposition;
    //erase
    int _current_sector_eraseposition;

    int _STATE;
    //0: not initialized
    //1: ready
    //2: writing
    //3: reading
    //4: erasing
	//5: want to find new page start (after erasing) OCCURS SHORTLY AND ONLY PRIVATE

    // Disallow creating an instance:
    //Flashtraining() {}
};

extern Flashtraining *global_ft;

#endif

