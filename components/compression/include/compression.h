#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "stdint.h"
#include "ChazeFlashtrainingWrapper.h"

#define BUFFER_SIZE /*16384*/ 8192
#define CHUNK 8192 /*Must be same size as BUFFER_SIZE*/
#define DEFAULT_COMPRESSION_LEVEL 6
#define WINDOW_SIZE 9
#define MEM_LEVEL 1
#define GZIP_ENCODING 16


typedef struct {
	int32_t counter;
	uint8_t * data;
}buffer_t;

buffer_t * buffers[2];

void compress_and_save(FlashtrainingWrapper_t *, uint8_t);
void write_data_to_flash(FlashtrainingWrapper_t *, uint8_t *, uint32_t);
uint32_t def(uint8_t *, uint8_t *, uint8_t);
void zerr(int32_t);


#endif


//Working main.cpp file.
//Creates task simulating sensor readings with mutex, compresses if buffer full, switches buffer and writes to next one
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
#include "freertos/semphr.h"
#include "esp_log.h"

SemaphoreHandle_t xSemaphore = NULL;
volatile uint8_t buff_idx = 0;
static const char * TAG = "Chaze-Compression-Main";

void sample_hr(void * pvParams)
{
	char dummy[] = "Sorem ipsum lorem ich habe hier einen schönen\nAusblich wäre ich hier irgendwas rein schreibe. Ich sitze an meinem Schreibtisch\nund drehe ein bisschen Daeumchen und so weiter und so fort.\n";
	for(;;){

		//Try to aquire the mutex for a given amount of time then back-off
		//For real life, need priorites in low sample rate sensors, random back-off time etc.

		buffer_t * curr_buff = buffers[buff_idx];

		if(xSemaphore != NULL)
		{
			if(xSemaphoreTake(xSemaphore,(TickType_t) 100) == pdTRUE)
			{
				ESP_LOGI(TAG, "Acquired the lock");
				if(BUFFER_SIZE-curr_buff->counter >= strlen(dummy)){
					ESP_LOGI(TAG, "Enough space to write");
					//Enough space, we can write
					memcpy(curr_buff->data+curr_buff->counter, dummy, strlen(dummy));
					curr_buff->counter += strlen(dummy);
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
					compress_and_save(!buff_idx);
					curr_buff->counter = 0; //Reset the compressed buffer
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
	 xSemaphore = xSemaphoreCreateMutex();

	//Initialize buffers 1 and 2
	buffer_t buffer0 = {};
	buffer0.data = (char *) malloc(BUFFER_SIZE);
	buffer0.counter = 0;
	buffer_t buffer1 = {};
	buffer1.data = (char *) malloc(BUFFER_SIZE);
	buffer1.counter = 0;

	buffers[0] = &buffer0;
	buffers[1] = &buffer1;

	if (xTaskCreate(&sample_hr, "sample_hr", 1024 * 8, NULL, 5, NULL) != pdPASS )
	{
		printf("Synch task create failed.\n");
	} else ESP_LOGI(TAG, "Created task");

	while(1){
		vTaskDelay(10000);
	}

}


//Second working main.cpp example compressing one large file. Will more likely use glog for the
 * case we have a FatFS.
 */
/*
extern "C" void app_main()
{

	esp_vfs_fat_mount_config_t mount_config = {};
	mount_config.max_files = 4;
	mount_config.format_if_mount_failed = true;
	mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
		return;
	}

	print_dir();

	remove_folder();

	print_dir();


	ESP_LOGI(TAG, "Opening file");
	foo = fopen("/spiflash/foo.bin", "wb");
	if (foo == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	//int ret = fprintf(foo, "written using ESP-IDF\n");
	const char * tmp = esp_get_idf_version();
	int ret = fwrite(tmp,1,strlen(tmp),foo);
	if(ret < strlen(tmp)){
		ESP_LOGE(TAG, "Error writing file");
	}
	if(fclose(foo) == EOF){
		ESP_LOGE(TAG, "Error closing the file.");
	}
	else ESP_LOGI(TAG, "File written");

	comp = fopen("/spiflash/c.bin", "wb");
	if (comp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}

	foo = fopen("/spiflash/foo.bin", "rb");
	if(foo == NULL){
		ESP_LOGE(TAG, "Error opening the file.");
	}

	print_dir();

	uint8_t level = 6;
	zerr(def(foo, comp, level));

	ESP_LOGI(TAG, "Compressed.");

	if(fclose(foo) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	else ESP_LOGI(TAG, "Closed foo.");

	if(fclose(comp) == EOF){
		ESP_LOGE(TAG, "Error closing file: %s", strerror(errno));
	}
	else ESP_LOGI(TAG, "Closed comp.");


	//Read comp
	char line[128];
	char *pos;
	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	comp = fopen("/spiflash/c.bin", "rb");
	if (comp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
	}
	fgets(line, sizeof(line), foo);
	fclose(comp);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);


	//Inflate
	recon = fopen("/spiflash/rec.bin", "wb");
	comp = fopen("/spiflash/c.bin", "rb");
	if(recon == NULL || comp == NULL){
		ESP_LOGE(TAG, "Error opening file before inflating.");
	}

	ESP_LOGI(TAG, "Start inflating");

	zerr(inf(comp, recon));

	ESP_LOGI(TAG, "Stopped inflating");

	if(fclose(comp) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	ESP_LOGI(TAG, "Closed comp");

	if(fclose(recon) == EOF){
		ESP_LOGE(TAG, "Error closing file");
	}
	ESP_LOGI(TAG, "Closed recon");


	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	recon = fopen("/spiflash/rec.bin", "rb");
	if (recon == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
	}
	fgets(line, sizeof(line), recon);
	fclose(recon);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);


	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	foo = fopen("/spiflash/foo.bin", "rb");
	if (foo == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return;
	}
	fgets(line, sizeof(line), foo);
	fclose(foo);
	// strip newline
	pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);

	// Unmount FATFS
	ESP_LOGI(TAG, "Unmounting FAT filesystem");
	ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle));

	ESP_LOGI(TAG, "Done");

	while(1){
		vTaskDelay(1000);
	}
}
*/

