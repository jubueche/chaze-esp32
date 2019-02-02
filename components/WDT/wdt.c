#include "wdt.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

const char * TAG = "Chaze-WDT";

#define DEFAULT_TIMEOUT 10
#define DEFAULT_PANIC true

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


esp_err_t get_wdt_status(void)
{
	return esp_task_wdt_status(NULL);
}
