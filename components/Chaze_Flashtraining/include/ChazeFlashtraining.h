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
    uint32_t get_number_of_unsynched_trainings(void);
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
#endif


/*
//Working main.cpp file
#include "ChazeFlashtraining.h"
#include "Configuration.h"

extern "C" void app_main()
{

	config.turn_on_main_circuit();

	Flashtraining ft = Flashtraining();
	printf("Start new training successful?: %d\n",ft.start_new_training());

	int i =0;
	while(i < 100){
		ft.write_training_cycle_IMU(10000, 0.23, 0.55, 0.55, 0.55, 0.55, 0.55, 0.55);
		vTaskDelay(100 / portTICK_PERIOD_MS);
		//ft.please_call_every_loop();
		//printf("Iteration: is %d\n", i);
		i++;
	}
	ft.stop_training();

	ft.start_reading_data();

	uint8_t buf[512];
	ft.get_all_data_bufferwise(buf);

	for(int i=0; i<512;i=i+4){
		//printf("Data is %d\n", buf[i]);
		long l_regenerated = 16777216 * buf[i+0] + 65536 * buf[i+1] + 256 * buf[i+2] + buf[i+3];
  		float floaty_regenerated = *(float*) &l_regenerated;
		printf("Regen.: %f\n", floaty_regenerated);
	}

	while(1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

}

*/
