#include "wdt.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

const char * TAG = "Chaze-WDT";

#define DEFAULT_TIMEOUT 10
#define DEFAULT_PANIC true

/**
 * @brief Initialize and attach the watchdog to the current task.
 * @param timeout in seconds. Watchdog gets triggered after timeout seconds without being reset.
 * @param panic Whether to panic if the WDT is not reset or not.
 * @return
 */
esp_err_t start_wdt(uint32_t timeout, bool panic)
{
	esp_err_t ret = esp_task_wdt_init(timeout, panic);
	if(ret == ESP_ERR_NO_MEM) return ret;
	ret = esp_task_wdt_add(NULL);
	if(ret == ESP_ERR_NO_MEM){
		ESP_LOGE(TAG, "No memory");
		return ret;
	} else if(ret == ESP_ERR_INVALID_STATE){ //Was not initialized
		esp_task_wdt_init(timeout, panic);
		ret = esp_task_wdt_add(NULL);
		return ret;
	} else return ESP_OK;

}

/**
 * Detach and deinitialize the interrupt. Must be called from the task where the start_wdt() was called from.
 * @return esp_err_t
 */
esp_err_t stop_wdt(void)
{
	esp_err_t ret = esp_task_wdt_delete(NULL);
	if(ret == ESP_ERR_INVALID_STATE){
		return ret; //No need to stop, brecause it wasn't inited in the first place
	}
	ret = esp_task_wdt_deinit();
	if(ret == ESP_ERR_INVALID_STATE){
		ESP_LOGE(TAG, "Tasks are still subscribed to WDT.");
		return ret;
	} else return ESP_OK;
}

/**
 * @brief Resets the WDT. Must be called within timeout period specified in start_wdt()
 * @return
 */
esp_err_t feed_wdt(void)
{
	esp_err_t ret  = esp_task_wdt_reset();
	if(ret == ESP_ERR_INVALID_STATE){
		ret = start_wdt(DEFAULT_TIMEOUT, DEFAULT_PANIC);
		return ret;
	} else if(ret == ESP_ERR_NOT_FOUND){
		esp_task_wdt_add(NULL);
		return esp_task_wdt_reset();
	}
	else return ESP_OK;
}

/**
 * @brief Get the watchdog status for the current task.
 * @return ESP_OK if task is subscribed, ESP_ERR_NOT_FOUND if task is not subscribed
 * ESP_ERR_INVALID_STATE if the wdt is not even initialized. It should never come to that.
 */
esp_err_t get_wdt_status(void)
{
	return esp_task_wdt_status(NULL);
}


/*
 //Working main.cpp example
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"


extern "C" {
#include "WiFiOTA.h"
#include "wdt.h"
}

extern "C" void app_main()
{
	start_wdt(5, true);
	vTaskDelay(4900 / portTICK_RATE_MS);
	feed_wdt();
	vTaskDelay(5100 / portTICK_RATE_MS);


	while(1){
		vTaskDelay(1000);
	}
}
*/
