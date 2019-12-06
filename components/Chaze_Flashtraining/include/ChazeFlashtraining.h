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
    bool start_new_training(); //! Consti
    bool stop_training(); //! Consti

    // Helper functions for reading and writing to NVS flash memory
    char * get_string_pointer_from_memory(const char *, size_t , char *); //! Done
    bool set_string_in_memory(char *, char *, uint8_t); //! Done

    bool initialize_flash(); //! Done
    //! Needs to be tested and implemented
    bool write_compressed_chunk(uint8_t * data, uint32_t n); //! Consti
    uint16_t get_number_of_unsynched_trainings(void); //! Done
    void set_number_of_unsynched_trainings(uint16_t); //! Done
    void add_unsynched_training(); //! Done
    void remove_unsynched_training(); //! Done
    int32_t get_next_buffer_of_training(uint8_t *);  //! Consti // Takes pointer to buffer. Returns -1 if wrote 512 bytes else the number of bytes written.
    void completed_synch_of_training(bool); // Passes a boolean indicating whether the training was successfully synched. //! Consti
    char * get_device_name(void); //Returns device ID //! Consti
    char * get_azure_connection_string(void); // Returns connection string for Azure. //! Done
    char * get_wifi_ssid(void); //! Done
    char * get_wifi_password(void); //! Done
    bool set_device_name(char *, uint8_t); //Sets device name //! Done
    bool set_azure_connection_string(char *, uint8_t); // Sets connection string for Azure. //! Done
    bool set_wifi_ssid(char *, uint8_t); //! Done
    bool set_wifi_password(char *, uint8_t); //! Done
    bool set_name(char *, uint8_t); //! Done
    char * get_name(void); //! Done
    bool set_container_name(char *, uint8_t); //! Done
    char * get_container_name(void); //! Done
    uint8_t get_version(char *); //! Done Maybe have #define in main.cpp or Configuration.c?
    bool set_version(char *, uint8_t); //! Done


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

