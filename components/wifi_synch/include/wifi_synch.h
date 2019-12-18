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
#include "ChazeMeta.h"

void synch_via_wifi(void *);
void poll_wifi(void);
bool synch_with_azure(void);

static IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle = NULL;

#endif
