/*Flash_training.h - Library to organize Training Data on Flash memory
   Created by Constantin Koch, May 23, 2018
*/
#ifndef Flashtraining_h
#define Flashtraining_h

#include "SPIFlash.h"
#include "Configuration.h"

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
    bool write_training_cycle_pressure(long Time, float Pressure);
    bool write_training_cycle_IMU(long Time, float ax, float ay, float az, float q1, float q2, float q3, float q4);
    //bool write_training_cycle_O2Sensor(long Time, ...);	//TODO

    // Added by Julian Buechel; Writes n chars to the flash.
    //! Needs to be tested
    bool write_compressed_chunk(uint8_t * data, uint32_t n);

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
#endif
