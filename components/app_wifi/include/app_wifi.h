#ifndef _WIFI_FUNCTIONS_H_
#define _WIFI_FUNCTIONS_H_

#include "esp_wifi.h"
#include "freertos/event_groups.h"


#define WIFI_SSID			"EatOrBeEaten"
#define WIFI_PASS			"Fussball08"

EventGroupHandle_t wifi_event_group;

void wifi_initialise(void);
void wifi_wait_connected();

#endif

/*

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

 *
 */
