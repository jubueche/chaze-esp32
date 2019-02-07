#ifndef WIFI_SYNCH_H
#define WIFI_SYNCH_H


void synch_via_wifi(void *);
void synch_with_azure(void *);
bool poll_wifi(void);
char * get_ssid(void);
char * get_password(void);


#endif

/*#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

extern "C"{
#include "wifi_synch.h"
}

extern "C" void app_main()
{

	synch_via_wifi();
	while(1){
		vTaskDelay(100);
	}


}
*/
