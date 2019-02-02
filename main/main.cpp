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
