#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

extern "C" {
#include "WiFiOTA.h"
}

extern "C" void app_main()
{
	check_and_update();

	while(1){
		vTaskDelay(1000);
	}
}
