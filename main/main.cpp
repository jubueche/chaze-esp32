//   This example code is in the Public Domain (or CC0 licensed, at your option.)

//   Unless required by applicable law or agreed to in writing, this
//   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//   CONDITIONS OF ANY KIND, either express or implied.

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

//Include time setting lib
#include "lwip/err.h"
#include "lwip/apps/sntp.h"

#include "nvs_flash.h"

#include "upload_to_blob.h"
#include "upload_to_blob_block.h"

#include "azure_c_shared_utility/xlogging.h"

extern "C"{
#include "app_wifi.h"
}

#define EXAMPLE_WIFI_SSID "EatOrBeEaten"
#define EXAMPLE_WIFI_PASS "Fussball08"


#ifndef BIT0
#define BIT0 (0x1 << 0)
#endif


static const char *TAG = "azure";


void azure_task(void *pvParameter)
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP success!");

    int res = upload_to_blob_block();
    //upload_to_blob();

    vTaskDelete(NULL);
}

extern "C" void app_main()
{

    wifi_initialise();

    if ( xTaskCreate(&azure_task, "azure_task", 1024 * 8, NULL, 5, NULL) != pdPASS ) {
        printf("create azure task failed\r\n");
    }

}
