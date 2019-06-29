#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include "string.h"

#include "azure_c_shared_utility/shared_util_options.h"
#include "iothub.h"
#include "iothub_device_client_ll.h"
#include "iothub_message.h"
#include "iothubtransporthttp.h"
#include "azure_c_shared_utility/agenttime.h"


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


//After we scanned and saw that the SSID was available, connect and timeout connection try after 9s.
#define WAIT_TIME 9000
#define MAX_APs 20
#define SCAN_INTERVAL 20 //Scan every X seconds, then go to light sleep and resume
#define LONG_TIME 0xffff

/**
 * TODO: Must be saved in non-erasable memory
 */
static char ssid[32] = "EatOrBeEaten";
static char password[32] = "Fussball08";
static char deviceId[32] = "chaze-2";


const char * TAG = "Chaze-WIFI-Synch";

/**
 * TODO: Must be saved in non-erasable memory
 */
static const char* connectionString = "HostName=chaze-iot-hub.azure-devices.net;DeviceId=device-2;SharedAccessKey=lMPIGSbbyrWlMUDDi0wPgOL+NaM8ATwB3rkUiQp4xH8=";

//This semaphore is used so that the synch_via_wifi task waits for the azure task to end.
SemaphoreHandle_t xSemaphore = NULL;

static int block_count = 0;

//Used for the interrupt generated when we have connected
const int CONNECTED_BIT = BIT0;

EventGroupHandle_t wifi_event_group;
EventBits_t uxBits;

static bool allow_to_connect = false;

/**
 * @brief Event handler for WiFi. If disconnected -> connect again
 * @param ctx
 * @param event
 * @return
 */
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
switch(event->event_id) {
case SYSTEM_EVENT_STA_START:
ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
if(allow_to_connect){
	ESP_ERROR_CHECK(esp_wifi_connect());
}
break;
case SYSTEM_EVENT_STA_GOT_IP:
ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
ESP_LOGI(TAG, "got ip:%s\n",
	ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	break;
case SYSTEM_EVENT_STA_DISCONNECTED:
ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
//ESP_ERROR_CHECK(esp_wifi_connect());
xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
break;
default:
	break;
}
return ESP_OK;
}

/**
 * @brief Task to synch the data via WiFi. This task iteratively checks if the SSID router is available. If not, goes to light sleep
 * else connects to WiFi and synchs all the trainings in flash with Azure.
 * TODO: Maybe disable event handler
 * @param pvParameter
 */
void synch_via_wifi(void *pvParameter)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		if(nvs_flash_erase() == ESP_OK){
			if(nvs_flash_init() != ESP_OK){
				ESP_LOGE(TAG, "Failed to init NVS flash.");
				vTaskDelete(NULL);
			}
		} else{
			ESP_LOGE(TAG, "Failed to erase flash.");
			vTaskDelete(NULL);
		}

	}

	bool connected = false;
	/**
	 * TODO: Change this to while(STATE == ADVERTISING)
	 */
	while(1){
		connected = poll_wifi();
		if(connected){
			ESP_LOGI(TAG, "Connected to WiFi, syncing data now...");
			xSemaphore = xSemaphoreCreateBinary();

			if (xTaskCreate(&synch_with_azure, "synch_with_azure", 1024 * 8, NULL, 5, NULL) != pdPASS )
			{
				ESP_LOGE(TAG, "Creating synch_azure task failed.");
				break; //Shall exit the while loop and clean up resources
			}

			for(;;){
				 if( xSemaphoreTake( xSemaphore, LONG_TIME ) == pdTRUE )
				 {
					ESP_LOGI(TAG, "Azure is done and acquired the semaphore. Returning...");
					break; //Shall exit for(;;) loop
				 }
			}
			break; //Shall exit while loop

		} else{
			ESP_LOGI(TAG, "Not connected. Going back to sleep...");
			const int wakeup_time_sec = SCAN_INTERVAL; // in sec
			ESP_LOGI(TAG, "Enabling timer wakeup, %ds", wakeup_time_sec);
			if(esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000) == ESP_OK){
				if(esp_light_sleep_start() != ESP_OK){
					ESP_LOGE(TAG, "Failed to enter light sleep.");
					break;
				}
			} else{
				ESP_LOGE(TAG, "Failed to enable on timer out wake up.");
				break;
			}
			ESP_LOGI(TAG,"Woke up.");
		}
	}
	ESP_LOGI(TAG, "Done. Stopping WiFi");
	allow_to_connect = false;
	if(esp_wifi_stop() == ESP_OK){
		if(esp_wifi_deinit() != ESP_OK){
			ESP_LOGE(TAG, "Failed to deinit WiFi");
		}
	} else {
		ESP_LOGE(TAG, "Could not stop WiFi");
	}
	vTaskDelete(NULL);
}

/**
 * @brief Fills up char array data with n bytes. For now just dummy data.
 * TODO: Connect to flash and call function for passing the data.
 * @param data
 * @param n
 */
void get_data(unsigned char * data, int n)
{
	float * nums = (float *) malloc(13*sizeof(float));
	for(int i= 0;i< 13;i++){
		nums[i] = (float)rand()/(float)(RAND_MAX/1);
	}
	char fl[4*13+13+2];
	snprintf(fl, sizeof fl, "%.02f %.02f %.02f %.02f %.02f %.02f %.02f %.02f %.02f %.02f %.02f %.02f %.02f\n",
			nums[0], nums[1], nums[2], nums[3], nums[4], nums[5], nums[6], nums[7], nums[8], nums[9], nums[10], nums[11],nums[12]);
	free(nums);
	if(n < 4*13+13+2){
		ESP_LOGE(TAG, "Need more memory.");
	}
	memcpy(data, fl, strlen(fl));

}

/**
 * @brief Callback used to upload blocks to the blob storage. *data will be passed to a function filling it with data from the flash.
 * @param result
 * @param data
 * @param size
 * @param context
 * @return
 */
static IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_RESULT getDataCallback(IOTHUB_CLIENT_FILE_UPLOAD_RESULT result, unsigned char const ** data, size_t* size, void* context)
{
    (void)context;
    if (result == FILE_UPLOAD_OK)
    {
        if (data != NULL && size != NULL)
        {
            if (block_count < 100)
            {
            	unsigned char * tmp = (unsigned char *) malloc(4*13+13+2*sizeof(char));
                get_data(tmp,4*13+13+2);
                *data = (const unsigned char*) tmp;
                *size = 4*13+13+2;
                block_count++;
            }
            else
            {
            	//This indicates the last block has been written.
                *data = NULL;
                *size = 0;
                ESP_LOGI(TAG, "Indicating upload is complete (%d blocks uploaded)", block_count);
            }
        }
        else
        {
            // The last call to this callback is to indicate the result of uploading the previous data block provided.
            // Note: In this last call, data and size pointers are NULL.
            ESP_LOGI(TAG, "Last call to getDataCallback (result for %dth block uploaded: %s)", block_count, ENUM_TO_STRING(IOTHUB_CLIENT_FILE_UPLOAD_RESULT, result));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Received unexpected result %s", ENUM_TO_STRING(IOTHUB_CLIENT_FILE_UPLOAD_RESULT, result));
    }

    // This callback returns IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_OK to indicate that the upload shall continue.
    // To abort the upload, it should return IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_ABORT
    return IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_OK;
}

/**
 * @brief Initializes the IoTHub client, sets options, sets filename, uploads data to blob.
 * @param pvParameter
 */
void synch_with_azure(void *pvParameter)
{
	IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;
	(void)IoTHub_Init();
	device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(connectionString, HTTP_Protocol);

	if (device_ll_handle == NULL)
	{
		ESP_LOGE(TAG, "Failure creating Iothub device. Check conn string.");
	}
	else
	{
		IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);

		time_t now;
		time_t tmp;
		now = get_time(&tmp);

		char strftime_buf[64];
		struct tm timeinfo;

		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

		for(int i=0;i<strlen(strftime_buf);i++){
			if(strftime_buf[i] == ' '){
				strftime_buf[i] = '_';
			}
		}
		char file_name[128]; //64+32+puffer
		strcpy(file_name, deviceId);
		strcat(file_name, "_");
		strcat(file_name, strftime_buf);
		strcat(file_name, ".txt");

		if (IoTHubDeviceClient_LL_UploadMultipleBlocksToBlob(device_ll_handle, file_name, getDataCallback, NULL) != IOTHUB_CLIENT_OK)
		{
			ESP_LOGE(TAG, "Failed to upload");
		}
		else
		{
			ESP_LOGI(TAG, "Successful upload.");
		}
		IoTHubDeviceClient_LL_Destroy(device_ll_handle);
	}
	IoTHub_Deinit();
	if(xSemaphoreGive(xSemaphore) != pdTRUE)
	{
		ESP_LOGE(TAG, "Failed to release the semaphore.");
	}
	vTaskDelete(NULL);
}

/**
 * @brief Scans the available networks for a matching one. Does not connect.
 * @return True if SSID was found.
 */
bool scan_for_ssid(void)
{
	// No need to clean up. wifi_stop() does that.
	tcpip_adapter_init();

	wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
	if(esp_wifi_init(&wifi_config) == ESP_OK){
		if(esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK){
			if(esp_wifi_start() != ESP_OK){
				//No need to clean up
				ESP_LOGE(TAG, "Failed to start WiFi");
				return false;
			}
		} else{
			ESP_LOGE(TAG, "Failed to set WiFi mode.");
			return false;
		}
	} else{
		ESP_LOGE(TAG, "Failed to initialize WiFi");
		return false;
	}

	// configure and run the scan process in blocking mode
	wifi_scan_config_t scan_config = {
		.ssid = (uint8_t *) ssid,
		.bssid = 0,
		.channel = 0,
		.show_hidden = true
	};
	if(esp_wifi_scan_start(&scan_config, true) != ESP_OK){
		ESP_LOGE(TAG, "Failed to start scanning.");
		return false;
	}

	uint16_t ap_num = MAX_APs;
	wifi_ap_record_t ap_records[MAX_APs];
	if(esp_wifi_scan_get_ap_records(&ap_num, ap_records) != ESP_OK){
		ESP_LOGE(TAG, "Failed to get AP records.");
		return false;
	}

	for(int i = 0; i < ap_num; i++){
		ESP_LOGI(TAG, "Network found: %s\n", (char *)ap_records[i].ssid);
		if(strcmp((char *)ap_records[i].ssid, ssid) == 0){
			if(esp_wifi_stop() == ESP_OK){
				if(esp_wifi_deinit() != ESP_OK){
					ESP_LOGE(TAG, "Failed to deinit WiFi.");
					return false;
				} else return true;
			} else{
				ESP_LOGE(TAG, "Failed to stop WiFi");
				return false;
			}
		}
	}
	esp_wifi_scan_stop();
	if(esp_wifi_stop() == ESP_OK){
		if(esp_wifi_deinit() != ESP_OK){
			ESP_LOGE(TAG, "Failed to deinit WiFi.");
		}
	} else{
			ESP_LOGE(TAG, "Failed to stop WiFi");
	}
	return false;
}

/**
 * @brief This function is periodically called and firstly checks if the given SSID can be found on the WiFi.
 * If the SSID can be found, we establish a WiFi connection.
 * @return True on successful connection.
 */
bool poll_wifi(void){

	//First, scan to see if there is one ssid that matches ours.
	bool tmp;
	uint64_t start = (uint64_t) esp_timer_get_time();
	tmp = scan_for_ssid();
	printf("%lld\n", (uint64_t) esp_timer_get_time()-start);

	allow_to_connect = true; //Enable connect on event STA_READY
	ESP_LOGI(TAG, "Enabled allow_to_connect");

	if(!tmp){
		ESP_LOGI(TAG, "WiFi SSID not found.");
		allow_to_connect = false;
		return false;
	}


	wifi_event_group = xEventGroupCreate();

	tcpip_adapter_init();
	esp_err_t ret = esp_event_loop_init(event_handler, NULL);
	if(ret == ESP_FAIL){
		ESP_LOGI(TAG, "Event handler already initialized. That is ok. Continue.\n");
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	if(esp_wifi_init(&cfg) != ESP_OK){
		ESP_LOGE(TAG, "Could not initialize WiFi.");
		allow_to_connect = false;
		return false;
	}

	wifi_sta_config_t sta_cnfg = {};
	strcpy((char *) sta_cnfg.ssid, ssid);
	strcpy((char *) sta_cnfg.password, password);

	wifi_config_t wifi_config = { .sta = sta_cnfg };
	if(esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK){
		ESP_LOGE(TAG, "Could not set WiFi mode.");
		allow_to_connect = false;
		return false;
	}
	if(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK){
		ESP_LOGE(TAG, "Could not set WiFi config.");
		allow_to_connect = false;
		return false;
	}

	if(esp_wifi_start() != ESP_OK){
		ESP_LOGE(TAG, "WiFi start error.");
		allow_to_connect = false;
		return false;
	}

	esp_wifi_set_ps(/*WIFI_PS_MAX_MODEM*/WIFI_PS_NONE); //Give 100% power for short amount of time

	//Wait for connection. If not connected after WAIT_TIME many ms, continue as usual.
	uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, WAIT_TIME / portTICK_PERIOD_MS);

	if(CONNECTED_BIT & uxBits){
		return true;
	} else{
		if(esp_wifi_stop() == ESP_OK){
			if(esp_wifi_deinit() != ESP_OK){
				ESP_LOGE(TAG, "Failed to deinit WiFi");
			}
		} else {
			ESP_LOGE(TAG, "Could not stop WiFi");
		}
		ESP_LOGE(TAG, "Experienced timeout when trying to connect to WiFi. Maybe password wrong.");
		allow_to_connect = false;
		ESP_LOGI(TAG, "Disabled connect on STA mode.");
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
