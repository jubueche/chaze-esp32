#ifndef WIFI_SYNCH_H
#define WIFI_SYNCH_H


void synch_via_wifi(void *);
void synch_with_azure(void *);
bool poll_wifi(void);
char * get_ssid(void);
char * get_password(void);


#endif

/*
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C"{
#include "wifi_synch.h"
}

extern "C" void app_main()
{
	if (xTaskCreate(&synch_via_wifi, "synch_via_wifi", 1024 * 8, NULL, 5, NULL) != pdPASS )
	{
		printf("Synch task create failed.\n");
	}
	while(1){
		vTaskDelay(100);
	}
}
//
 * Typical working scenario:
 * while state = advertising
 * 		spawn task azure sync and get handle
 * 		spawn task BLE synch
 * 		[Main thread: Try to aquire semaphore, which must be given by both tasks]
 * 		[BLE task] and [WiFi task]: If phone is connected, aquire lock. If WiFi is connected, aquire lock.
 * 		(This way we make it impossible to synch via BLE and WiFi at the same time)
 //
*/
