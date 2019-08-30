

/*
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdlib.h"
#include "stdio.h"

#include <stdio.h>
#include <string.h>

extern "C" {
#include "compression.h"
}

#include "ChazeFlashtrainingWrapper.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "Configuration.h"

SemaphoreHandle_t xSemaphore = NULL;
volatile uint8_t buff_idx = 0;
static const char * TAG = "Chaze-Compression-Main";

void sample_hr(void * pvParams)
{
	FlashtrainingWrapper_t *ft = (FlashtrainingWrapper_t *) pvParams; 
	uint8_t dummy[] = "123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789123456789";



	for(;;){

		//Try to aquire the mutex for a given amount of time then back-off
		//For real life, need priorites in low sample rate sensors, random back-off time etc.

		buffer_t * curr_buff = buffers[buff_idx];

		if(xSemaphore != NULL)
		{
			if(xSemaphoreTake(xSemaphore,(TickType_t) 100) == pdTRUE)
			{
				ESP_LOGI(TAG, "Acquired the lock");
				if(BUFFER_SIZE-curr_buff->counter >= sizeof(dummy)){
					ESP_LOGI(TAG, "Enough space to write");
					//Enough space, we can write
					memcpy(curr_buff->data+(curr_buff->counter), dummy, sizeof(dummy));
					curr_buff->counter += sizeof(dummy);
				} else{
					//Fill up the buffer with 'f' and set the buff_idx to buff_idx XOR 1
					ESP_LOGI(TAG, "Buffer almost full, fill up");
					for(int i=curr_buff->counter;i<BUFFER_SIZE-curr_buff->counter;i++)
						curr_buff->data[i] = 'f';
					curr_buff->counter += BUFFER_SIZE-curr_buff->counter;
					ESP_LOGI(TAG, "Counter is %d", curr_buff->counter);

					if(buff_idx==0)
						buff_idx=1;
					else buff_idx=0;

					ESP_LOGI(TAG, "Call compress");
					//Call compress. Switch the bit back since we want to compress the buffer that is full
					if(ft == NULL){
						ESP_LOGE(TAG, "FlashtrainingWrapper is NULL");
						xSemaphoreGive(xSemaphore);
					} else {
						printf("State is %d\n", FlashtrainingWrapper_get_STATE(ft));
						compress_and_save(ft, !buff_idx);
						curr_buff->counter = 0; //Reset the compressed buffer
					}
				}


				ESP_LOGI(TAG, "Release mutex");
				//Release mutex
				xSemaphoreGive(xSemaphore);

			}
			vTaskDelay(100 / portTICK_PERIOD_MS); //Back off a little longer
		}
	}
}

extern "C" void app_main()
{


	turn_on_main_circuit();
	xSemaphore = xSemaphoreCreateMutex();

	//Create a training struct of type wrapper
	FlashtrainingWrapper_t *ft = FlashtrainingWrapper_create();
	bool ret = FlashtrainingWrapper_start_new_training(ft);

	printf("Ret is %d", ret);


	//Initialize buffers 1 and 2
	buffer_t buffer0 = {};
	buffer0.data = (uint8_t *) malloc(BUFFER_SIZE);
	buffer0.counter = 0;
	buffer_t buffer1 = {};
	buffer1.data = (uint8_t *) malloc(BUFFER_SIZE);
	buffer1.counter = 0;

	buffers[0] = &buffer0;
	buffers[1] = &buffer1;

	if (xTaskCreate(&sample_hr, "sample_hr", 1024 * 8, (void *) ft, 5, NULL) != pdPASS )
	{
		printf("Synch task create failed.\n");
	} else ESP_LOGI(TAG, "Created task");

	while(1){
		vTaskDelay(10000);
	}

	FlashtrainingWrapper_stop_training(ft);
	FlashtrainingWrapper_destroy(ft);

}

*/

/*
#include "ChazeFlashtraining.h"
#include "Configuration.h"

extern "C" void app_main()
{

	turn_on_main_circuit();

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

}*/