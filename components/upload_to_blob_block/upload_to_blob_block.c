// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// CAVEAT: This sample is to demonstrate azure IoT client concepts only and is not a guide design principles or style
// Checking of return codes and error values shall be omitted for brevity.  Please practice sound engineering practices
// when writing production code.

#ifndef DONT_USE_UPLOADTOBLOB

#include <stdio.h>
#include <stdlib.h>

/* This sample uses the _LL APIs of iothub_client for example purposes.
That does not mean that HTTP only works with the _LL APIs.
Simply changing the using the convenience layer (functions not having _LL)
and removing calls to _DoWork will yield the same results. */

#include "azure_c_shared_utility/shared_util_options.h"
#include "iothub.h"
#include "iothub_device_client_ll.h"
#include "iothub_message.h"
#include "iothubtransporthttp.h"


#include "certs.h"

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
static const char* connectionString = "HostName=chaze-iot-hub.azure-devices.net;DeviceId=chaze1;SharedAccessKey=aFyZrV362aMj/hEaxz0Wlq2eg6UMWMmjU3hwrDxzy1M=";

/*Optional string with http proxy host and integer for http proxy port (Linux only)         */
static const char* proxyHost = NULL;
static int proxyPort = 0;
static const char* data_to_upload = "Block Upload\n";
static int block_count = 0;

static IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_RESULT getDataCallback(IOTHUB_CLIENT_FILE_UPLOAD_RESULT result, unsigned char const ** data, size_t* size, void* context)
{
    (void)context;
    if (result == FILE_UPLOAD_OK)
    {
        if (data != NULL && size != NULL)
        {
            // "block_count" is used to simulate reading chunks from a larger data content, like a large file.

            if (block_count < 100)
            {
                *data = (const unsigned char*)data_to_upload;
                *size = strlen(data_to_upload);
                block_count++;
            }
            else
            {
                // This simulates reaching the end of the file. At this point all the data content has been uploaded to blob.
                // Setting data to NULL and/or passing size as zero indicates the upload is completed.

                *data = NULL;
                *size = 0;

                (void)printf("Indicating upload is complete (%d blocks uploaded)\r\n", block_count);
            }
        }
        else
        {
            // The last call to this callback is to indicate the result of uploading the previous data block provided.
            // Note: In this last call, data and size pointers are NULL.

            (void)printf("Last call to getDataCallback (result for %dth block uploaded: %s)\r\n", block_count, ENUM_TO_STRING(IOTHUB_CLIENT_FILE_UPLOAD_RESULT, result));
        }
    }
    else
    {
        (void)printf("Received unexpected result %s\r\n", ENUM_TO_STRING(IOTHUB_CLIENT_FILE_UPLOAD_RESULT, result));
    }

    // This callback returns IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_OK to indicate that the upload shall continue.
    // To abort the upload, it should return IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_ABORT
    return IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_OK;
}

//Set time in agenttime_esp.c @ esp-azure/port/src/
int upload_to_blob_block(void)
{
    IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;

    (void)IoTHub_Init();
    (void)printf("Starting the IoTHub client sample upload to blob with multiple blocks...\r\n");



    device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(connectionString, HTTP_Protocol);


    if (device_ll_handle == NULL)
    {
        (void)printf("Failure createing Iothub device.  Hint: Check you connection string.\r\n");
    }
    else
    {
        IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);

        HTTP_PROXY_OPTIONS http_proxy_options = { 0 };
        http_proxy_options.host_address = proxyHost;
        http_proxy_options.port = proxyPort;

        if (proxyHost != NULL && IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_HTTP_PROXY, &http_proxy_options) != IOTHUB_CLIENT_OK)
        {
            (void)printf("failure to set proxy\n");
        }
        else
        {
            if (IoTHubDeviceClient_LL_UploadMultipleBlocksToBlob(device_ll_handle, "test.txt", getDataCallback, NULL) != IOTHUB_CLIENT_OK)
            {
                printf("hello world failed to upload\n");
            }
            else
            {
                printf("hello world has been created\n");
            }
        }

        printf("Press any key to continue");
        (void)getchar();

        // Clean up the iothub sdk handle
        IoTHubDeviceClient_LL_Destroy(device_ll_handle);
    }
    IoTHub_Deinit();
    return 0;
}
#endif /*DONT_USE_UPLOADTOBLOB*/



//Working main.cpp file, important: Change CMakeLists.txt to

/*
# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.c"
					"iothub_client_sample_mqtt.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
component_compile_definitions(SET_TRUSTED_CERT_IN_SAMPLES)
 */

/*
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

 */
