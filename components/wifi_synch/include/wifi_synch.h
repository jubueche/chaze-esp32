#ifndef WIFI_SYNCH_H
#define WIFI_SYNCH_H


#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include "string.h"

extern "C" {
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothub.h"
#include "iothub_device_client_ll.h"
#include "iothub_message.h"
#include "iothubtransporthttp.h"
#include "azure_c_shared_utility/agenttime.h"
}


#include "certs.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "Configuration.h"
#include "ChazeFlashtraining.h"

void synch_via_wifi(void *);
void synch_with_azure(void *);
bool poll_wifi(void);

static IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle = NULL;
static TaskHandle_t synch_with_azure_task_handle = NULL;
static Flashtraining ft;

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
