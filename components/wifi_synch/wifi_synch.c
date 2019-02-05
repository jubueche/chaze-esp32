#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include "string.h"

#include "azure_c_shared_utility/shared_util_options.h"
#include "iothub.h"
#include "iothub_device_client_ll.h"
#include "iothub_message.h"
#include "iothubtransporthttp.h"


#include "certs.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi_synch.h"

#define WAIT_TIME 9000
#define MAX_APs 20
#define SCAN_INTERVAL 20
#define LONG_TIME 0xffff

static char ssid[32] = "EatOrBeEaten";
static char password[32] = "Fussball08";

const char * TAG = "Chaze-WIFI-Synch";

static const char* connectionString = "HostName=chaze-iot-hub.azure-devices.net;DeviceId=chaze-1;SharedAccessKey=RKzQxxE6WZRRn4jzmEB155l/jGioRBnziQHvQvfZzZ8=";

SemaphoreHandle_t xSemaphore = NULL;

/*Optional string with http proxy host and integer for http proxy port (Linux only)         */
static const char* proxyHost = NULL;
static int proxyPort = 0;
static const char* data_to_upload = "Block Upload\n";
static int block_count = 0;

//Used for the interrupt generated when we have connected
const int CONNECTED_BIT = BIT0;

EventGroupHandle_t wifi_event_group;
EventBits_t uxBits;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
switch(event->event_id) {
case SYSTEM_EVENT_STA_START:
ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
ESP_ERROR_CHECK(esp_wifi_connect());
break;
case SYSTEM_EVENT_STA_GOT_IP:
ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
ESP_LOGI(TAG, "got ip:%s\n",
	ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	break;
case SYSTEM_EVENT_STA_DISCONNECTED:
ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
ESP_ERROR_CHECK(esp_wifi_connect());
xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
break;
default:
	break;
}
return ESP_OK;
}


void synch_via_wifi(void)
{
	 // Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK( ret );

	bool connected = false;
	//Change this to while state is advertising
	while(1){
		connected = poll_wifi();
		if(connected){
			printf("Connected. Syncing data now.\n");
			xSemaphore = xSemaphoreCreateBinary();

			if (xTaskCreate(&synch_with_azure, "synch_with_azure", 1024 * 8, NULL, 5, NULL) != pdPASS )
			{
				printf("create azure task failed\r\n");
			}

			for(;;){
				 if( xSemaphoreTake( xSemaphore, LONG_TIME ) == pdTRUE )
				 {
					 printf("Azure is done. Acquired the semaphore. Returning.\n");
					 return;
				 }
			}

		} else{
			printf("Not connected. Going back to sleep.\n");
			const int wakeup_time_sec = SCAN_INTERVAL; // in sec
			printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
			esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);
			//rtc_gpio_isolate(GPIO_NUM_12);
			printf("Entering deep sleep\n");
			esp_light_sleep_start();
			printf("Woke up.\n");
		}
	}
}

void get_data(unsigned char * data, int n)
{
	for(int i=0;i<n-1;i++){
		data[i] = 'h';
	}
	data[n-1] = '\n';
}


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

void synch_with_azure(void *pvParameter)
{
	IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;

	(void)IoTHub_Init();
	(void)printf("Starting the IoTHub client sample upload to blob with multiple blocks...\r\n");

	device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(connectionString, HTTP_Protocol);

	if (device_ll_handle == NULL)
	{
		(void)printf("Failure creating Iothub device.  Hint: Check you connection string.\r\n");
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
			if (IoTHubDeviceClient_LL_UploadMultipleBlocksToBlob(device_ll_handle, "new.txt", getDataCallback, NULL) != IOTHUB_CLIENT_OK)
			{
				ESP_LOGE(TAG, "Failed to upload");
			}
			else
			{
				printf("hello world has been created\n");
			}
		}
		// Clean up the iothub sdk handle
		IoTHubDeviceClient_LL_Destroy(device_ll_handle);
	}
	IoTHub_Deinit();
	printf("Releasing the semaphore\n");
	if( xSemaphoreGive( xSemaphore ) != pdTRUE )
	{
		 printf("Failed to release the semaphore.\n");
	}
	vTaskDelete(NULL);
}

bool scan_for_ssid(void)
{
	// initialize the tcp stack
	tcpip_adapter_init();

	wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	// configure and run the scan process in blocking mode
	wifi_scan_config_t scan_config = {
		.ssid = (uint8_t *) ssid,
		.bssid = 0,
		.channel = 0,
		.show_hidden = true
	};
	printf("Start scanning...");
	ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
	printf(" completed!\n");

	// get the list of APs found in the last scan
	uint16_t ap_num = MAX_APs;
	wifi_ap_record_t ap_records[MAX_APs];
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

	for(int i = 0; i < ap_num; i++){
		printf("%s\n", (char *)ap_records[i].ssid);
		if(strcmp((char *)ap_records[i].ssid, ssid) == 0){
			ESP_ERROR_CHECK(esp_wifi_stop());
			ESP_ERROR_CHECK(esp_wifi_deinit());
			return true;
		}
	}

	ESP_ERROR_CHECK(esp_wifi_stop());
	ESP_ERROR_CHECK(esp_wifi_deinit());
	return false;

}

bool poll_wifi(void){

	//First, scan to see if there is one ssid that matches ours.
	bool tmp;
	uint64_t start = (uint64_t) esp_timer_get_time();
	tmp = scan_for_ssid();
	printf("%lld\n", (uint64_t) esp_timer_get_time()-start);
	if(!tmp) return false;

#if CONFIG_PM_ENABLE
	// Configure dynamic frequency scaling:
	// maximum and minimum frequencies are set in sdkconfig,
	// automatic light sleep is enabled if tickless idle support is enabled.
	esp_pm_config_esp32_t pm_config = {
			.max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
			.min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
			.light_sleep_enable = true
#endif
	};
	ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
#endif // CONFIG_PM_ENABLE

	wifi_event_group = xEventGroupCreate();

	tcpip_adapter_init();
	esp_err_t ret = esp_event_loop_init(event_handler, NULL);
	if(ret == ESP_FAIL){
		ESP_LOGI(TAG, "Event handler already initialized. That is ok. Continue.\n");
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));


	wifi_config_t wifi_config = {
		.sta = {.ssid = "EatOrBeEaten", .password = "Fussball08",},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "esp_wifi_set_ps().");
	esp_wifi_set_ps(/*WIFI_PS_MAX_MODEM*/WIFI_PS_NONE);

	//Wait for connection. If not connected after WAIT_TIME many ms, continue as usual.
	uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, WAIT_TIME / portTICK_PERIOD_MS);

	if(CONNECTED_BIT & uxBits){
		printf("Connected\n");
		return true;
	} else{
		printf("Timer...\n");
		return false;
	}


}

char * get_ssid(void){
	return ssid;
}

char * get_password(void)
{
	return password;
}
