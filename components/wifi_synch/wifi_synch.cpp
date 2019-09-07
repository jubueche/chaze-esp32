#include "wifi_synch.h"


//After we scanned and saw that the SSID was available, connect and timeout connection try after 9s.
#define WAIT_TIME 9000
#define MAX_APs 20
#define SCAN_INTERVAL 20 //Scan every X seconds, then go to light sleep and resume
#define LONG_TIME 0xffff


const char * TAG_WiFi = "Chaze-WIFI-Synch";


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
ESP_LOGI(TAG_WiFi, "SYSTEM_EVENT_STA_START");
if(allow_to_connect){
	ESP_ERROR_CHECK(esp_wifi_connect());
}
break;
case SYSTEM_EVENT_STA_GOT_IP:
ESP_LOGI(TAG_WiFi, "SYSTEM_EVENT_STA_GOT_IP");
ESP_LOGI(TAG_WiFi, "got ip:%s\n",
	ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	break;
case SYSTEM_EVENT_STA_DISCONNECTED:
ESP_LOGI(TAG_WiFi, "SYSTEM_EVENT_STA_DISCONNECTED");
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
 * Must be first task to be spawned in Advertising to initialize object. 
 * TODO: Maybe disable event handler
 * @param pvParameter
 */
void synch_via_wifi(void *pvParameter)
{
	ft = Flashtraining();
	config.wifi_connected = false;
	config.wifi_synch_semaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(config.wifi_synch_semaphore);

	for(;;)
	{
		if(xSemaphoreTake(config.wifi_synch_semaphore, LONG_TIME) == pdTRUE){
			ESP_LOGI(TAG_WiFi, "Acquired wifi_synch_semaphore.");
			break;
		}
		vTaskDelay(80 / portTICK_PERIOD_MS);
	}
	
	xSemaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(xSemaphore);

	// Continue to try and synch even if we are connected with the phone.
	// We will stay in Adv. mode max. 10min. This should be more than enough time to try & synch. We continue to
	// Synch in connected mode.
	while(config.STATE == ADVERTISING || config.STATE == CONNECTED ){

		uint16_t number_of_unsynched_trainings = ft.get_number_of_unsynched_trainings();
		// TODO Test that case
		if(number_of_unsynched_trainings == 0)
		{
			xSemaphoreGive(config.wifi_synch_semaphore);
			ft.~Flashtraining();
			vSemaphoreDelete(xSemaphore);
			vSemaphoreDelete(config.wifi_synch_semaphore);
			vTaskDelete(NULL);
		}

		config.wifi_connected = poll_wifi();
		if(config.wifi_connected){
			ESP_LOGI(TAG_WiFi, "Connected to WiFi, syncing data now...");

			if (xTaskCreate(&synch_with_azure, "synch_with_azure", 1024 * 8, NULL, 5, &synch_with_azure_task_handle) != pdPASS )
			{
				ESP_LOGE(TAG_WiFi, "Creating synch_azure task failed.");
				break; //Shall exit the while loop and clean up resources
			}

			// Let the azure-task acquire the lock first.
			vTaskDelay(1000 / portTICK_PERIOD_MS);

			for(;;){
				 if( xSemaphoreTake( xSemaphore, LONG_TIME ) == pdTRUE )
				 {
					ESP_LOGI(TAG_WiFi, "Azure is done and acquired the semaphore. Returning...");
					break; //Shall exit for(;;) loop
				 }
				 vTaskDelay(200 / portTICK_PERIOD_MS);
				 ESP_LOGI(TAG_WiFi, "Waiting for lock...");
			}
			break; //Shall exit while loop

		} else{
			unsigned long tmp_timer = millis();
			bool tmp_state_switched = false;
			while(millis() - tmp_timer < SCAN_INTERVAL*1000)
			{
				if(config.STATE != ADVERTISING && config.STATE != CONNECTED)
				{
					tmp_state_switched = true;
					break;
				}
				vTaskDelay(100 / portTICK_PERIOD_MS);
			}
			if(tmp_state_switched)
				break;
		}
	}
	ESP_LOGI(TAG_WiFi, "Done. Stopping WiFi");
	allow_to_connect = false;
	if(esp_wifi_stop() == ESP_OK){
		if(esp_wifi_deinit() != ESP_OK){
			ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi");
		}
	} else {
		ESP_LOGE(TAG_WiFi, "Could not stop WiFi");
	}
	xSemaphoreGive(config.wifi_synch_semaphore);
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
		ESP_LOGE(TAG_WiFi, "Need more memory.");
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
                ESP_LOGI(TAG_WiFi, "Indicating upload is complete (%d blocks uploaded)", block_count);
            }
        }
        else
        {
            // The last call to this callback is to indicate the result of uploading the previous data block provided.
            // Note: In this last call, data and size pointers are NULL.
            ESP_LOGI(TAG_WiFi, "Last call to getDataCallback (result for %dth block uploaded: %s)", block_count, ENUM_TO_STRING(IOTHUB_CLIENT_FILE_UPLOAD_RESULT, result));
        }
    }
    else
    {
        ESP_LOGE(TAG_WiFi, "Received unexpected result %s", ENUM_TO_STRING(IOTHUB_CLIENT_FILE_UPLOAD_RESULT, result));
    }

    // This callback returns IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_OK to indicate that the upload shall continue.
    // To abort the upload, it should return IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_ABORT
	if(config.STATE == ADVERTISING || config.STATE == CONNECTED){
		return IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_OK;
	} else {
		// Abort if we are suddently in RECORD or DEEPSLEEP state.
		return IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_ABORT;
	}
}

/**
 * @brief Initializes the IoTHub client, sets options, sets filename, uploads data to blob.
 * @param pvParameter
 */
void synch_with_azure(void *pvParameter)
{

	for(;;){
		if( xSemaphoreTake( xSemaphore, LONG_TIME ) == pdTRUE )
		{
			ESP_LOGI(TAG_WiFi, "Azure task got the lock...");
			break;
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
		ESP_LOGI(TAG_WiFi, "Azure task waiting for lock...");
	}

	(void)IoTHub_Init();
	device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(ft.get_azure_connection_string(), HTTP_Protocol);

	if (device_ll_handle == NULL)
	{
		ESP_LOGE(TAG_WiFi, "Failure creating Iothub device. Check conn string.");
	}
	else
	{

		//IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);
		if(config.STATE == ADVERTISING || config.STATE == CONNECTED)
		{

			time_t now;
			time_t tmp;
			now = get_time(&tmp);

			char strftime_buf[64];
			struct tm timeinfo;

			localtime_r(&now, &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

			/*config.initialize_rtc(); 
			rtc.set24Hour();
			rtc.setTime(timeinfo.tm_sec, timeinfo.tm_min, timeinfo.tm_hour, timeinfo.tm_wday, timeinfo.tm_mday, timeinfo.tm_mon +1, timeinfo.tm_year +1900);
			ESP_LOGI(TAG_WiFi, "Set time to %s", rtc.stringDate());*/

			for(int i=0;i<strlen(strftime_buf);i++){
				if(strftime_buf[i] == ' '){
					strftime_buf[i] = '_';
				}
			}
			char file_name[128]; //64+32+puffer
			strcpy(file_name, ft.get_device_id());
			strcat(file_name, "_");
			strcat(file_name, strftime_buf);
			strcat(file_name, ".txt");

			if (IoTHubDeviceClient_LL_UploadMultipleBlocksToBlob(device_ll_handle, file_name, getDataCallback, NULL) != IOTHUB_CLIENT_OK)
			{
				ESP_LOGE(TAG_WiFi, "Failed to upload");
			}
			else
			{
				ESP_LOGI(TAG_WiFi, "Successful upload.");
			}
		}

		IoTHubDeviceClient_LL_Destroy(device_ll_handle);
	}
	IoTHub_Deinit();
	if(xSemaphoreGive(xSemaphore) != pdTRUE)
	{
		ESP_LOGE(TAG_WiFi, "Failed to release the semaphore.");
	}
	ESP_LOGI(TAG_WiFi, "Delete this task.");
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
				ESP_LOGE(TAG_WiFi, "Failed to start WiFi");
				return false;
			}
		} else{
			ESP_LOGE(TAG_WiFi, "Failed to set WiFi mode.");
			return false;
		}
	} else{
		ESP_LOGE(TAG_WiFi, "Failed to initialize WiFi");
		return false;
	}

	// configure and run the scan process in blocking mode
	wifi_scan_config_t scan_config = {
		.ssid = (uint8_t *) ft.get_wifi_ssid(),
		.bssid = 0,
		.channel = 0,
		.show_hidden = true
	};
	if(esp_wifi_scan_start(&scan_config, true) != ESP_OK){
		ESP_LOGE(TAG_WiFi, "Failed to start scanning.");
		return false;
	}

	uint16_t ap_num = MAX_APs;
	wifi_ap_record_t ap_records[MAX_APs];
	if(esp_wifi_scan_get_ap_records(&ap_num, ap_records) != ESP_OK){
		ESP_LOGE(TAG_WiFi, "Failed to get AP records.");
		return false;
	}

	const char * tmp_ssid = ft.get_wifi_ssid();
	for(int i = 0; i < ap_num; i++){
		ESP_LOGI(TAG_WiFi, "Network found: %s\n", (char *)ap_records[i].ssid);
		if(strcmp((char *)ap_records[i].ssid, tmp_ssid) == 0){
			if(esp_wifi_stop() == ESP_OK){
				if(esp_wifi_deinit() != ESP_OK){
					ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi.");
					return false;
				} else {
					if(config.STATE != ADVERTISING && config.STATE != CONNECTED)
						return false;
					return true;
				}
			} else{
				ESP_LOGE(TAG_WiFi, "Failed to stop WiFi");
				return false;
			}
		}
	}
	esp_wifi_scan_stop();
	if(esp_wifi_stop() == ESP_OK){
		if(esp_wifi_deinit() != ESP_OK){
			ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi.");
		}
	} else{
			ESP_LOGE(TAG_WiFi, "Failed to stop WiFi");
	}
	return false;
}

/**
 * @brief This function is periodically called and firstly checks if the given SSID can be found on the WiFi.
 * If the SSID can be found, we establish a WiFi connection.
 * @return True on successful connection.
 */
bool poll_wifi(void){

	if(config.STATE != ADVERTISING && config.STATE != CONNECTED)
		return false;

	//First, scan to see if there is one ssid that matches ours.
	bool tmp;
	uint64_t start = (uint64_t) esp_timer_get_time();
	tmp = scan_for_ssid();
	printf("%lld\n", (uint64_t) esp_timer_get_time()-start);

	allow_to_connect = true; //Enable connect on event STA_READY
	ESP_LOGI(TAG_WiFi, "Enabled allow_to_connect");

	if(!tmp){
		ESP_LOGI(TAG_WiFi, "WiFi SSID not found.");
		allow_to_connect = false;
		return false;
	}

	ESP_LOGI(TAG_WiFi, "Abort");
	if(config.STATE != ADVERTISING && config.STATE != CONNECTED)
		return false;

	wifi_event_group = xEventGroupCreate();

	tcpip_adapter_init();
	esp_err_t ret = esp_event_loop_init(event_handler, NULL);
	if(ret == ESP_FAIL){
		ESP_LOGI(TAG_WiFi, "Event handler already initialized. That is ok. Continue.\n");
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	if(esp_wifi_init(&cfg) != ESP_OK){
		ESP_LOGE(TAG_WiFi, "Could not initialize WiFi.");
		allow_to_connect = false;
		return false;
	}

	wifi_sta_config_t sta_cnfg = {};
	strcpy((char *) sta_cnfg.ssid, ft.get_wifi_ssid());
	strcpy((char *) sta_cnfg.password, ft.get_wifi_password());

	wifi_config_t wifi_config = { .sta = sta_cnfg };
	if(esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK){
		ESP_LOGE(TAG_WiFi, "Could not set WiFi mode.");
		allow_to_connect = false;
		return false;
	}
	if(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK){
		ESP_LOGE(TAG_WiFi, "Could not set WiFi config.");
		allow_to_connect = false;
		return false;
	}

	if(esp_wifi_start() != ESP_OK){
		ESP_LOGE(TAG_WiFi, "WiFi start error.");
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
				ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi");
			}
		} else {
			ESP_LOGE(TAG_WiFi, "Could not stop WiFi");
		}
		ESP_LOGE(TAG_WiFi, "Experienced timeout when trying to connect to WiFi. Maybe password wrong.");
		allow_to_connect = false;
		ESP_LOGI(TAG_WiFi, "Disabled connect on STA mode.");
		return false;
	}
}
