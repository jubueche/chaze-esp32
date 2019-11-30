#ifndef CHAZE_OTA_H
#define CHAZE_OTA_H

#include "ChazeFlashtraining.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "wifi_synch.h"
#include "esp_https_ota.h"

#define OTA_WAIT_TIME 9000

void perform_OTA(void);
void check_for_update(void);

#endif
