
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdlib.h"
#include "stdio.h"

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


	config.turn_on_main_circuit();
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

