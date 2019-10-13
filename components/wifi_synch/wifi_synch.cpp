#include "wifi_synch.h"


//After we scanned and saw that the SSID was available, connect and timeout connection try after 9s.
#define WAIT_TIME 9000
#define MAX_APs 20
#define SCAN_INTERVAL 20 //Scan every X seconds, then go to light sleep and resume
#define LONG_TIME 0xffff


const char * TAG_WiFi = "Chaze-WIFI-Synch";


//This semaphore is used so that the synch_via_wifi task waits for the azure task to end.
SemaphoreHandle_t azure_task_semaphore = NULL;

static bool last_block = false;

//Used for the interrupt generated when we have connected
const int CONNECTED_BIT = BIT0;

EventGroupHandle_t wifi_event_group;
EventBits_t uxBits;


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
			ESP_ERROR_CHECK(esp_wifi_connect());
		break;
		case SYSTEM_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG_WiFi, "SYSTEM_EVENT_STA_GOT_IP");
			ESP_LOGI(TAG_WiFi, "got ip:%s\n",
			ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG_WiFi, "SYSTEM_EVENT_STA_DISCONNECTED");
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
			config.wifi_connected = false;
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

		// Continue to try and synch even if we are connected with the phone.
		// We will stay in Adv. mode max. 10min. This should be more than enough time to try & synch. We continue to
		// Synch in connected mode.
		while(config.STATE == ADVERTISING || config.STATE == CONNECTED ){

			for(;;)
			{
				if(xSemaphoreTake(config.wifi_synch_semaphore, 300) == pdTRUE){
					ESP_LOGI(TAG_WiFi, "Acquired wifi_synch_semaphore.");
					break;
				}
				vTaskDelay(80 / portTICK_PERIOD_MS);
			}

			uint16_t number_of_unsynched_trainings = ft.get_number_of_unsynched_trainings();
			// TODO Test that case
			// vTaskResume(task_handle) will be called when the number of trainings is greater zero again
			if(number_of_unsynched_trainings == 0)
			{
				config.wifi_synch_task_suspended = true;
				xSemaphoreGive(config.wifi_synch_semaphore);
				vTaskSuspend(NULL);
			} else {
				if(!config.wifi_connected)
				{
					config.wifi_connected = poll_wifi();
				}
				if(config.wifi_connected){
					ESP_LOGI(TAG_WiFi, "Connected to WiFi, synching data now...");
					bool success = synch_with_azure();
				}
			}

			// Release the semaphore and suspend for SCAN_INTERVAL number of seconds
			xSemaphoreGive(config.wifi_synch_semaphore);
			vTaskDelay(SCAN_INTERVAL*1000 / portTICK_PERIOD_MS); //Delay for 20s
			ESP_LOGI(TAG_WiFi, "After delay.");
		}
		ESP_LOGI(TAG_WiFi, "State changed. Wifi is stopping.");
		esp_err_t stop_wifi_res = esp_wifi_stop();
		if(stop_wifi_res == ESP_OK){
			esp_err_t deinit_wifi_res = esp_wifi_deinit();
			if(deinit_wifi_res != ESP_OK){
				ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi: %s", esp_err_to_name(deinit_wifi_res));
			} else 	ESP_LOGI(TAG_WiFi, "Deinited WiFi: l.167");
		} else {
			ESP_LOGE(TAG_WiFi, "Could not stop WiFi: %s", esp_err_to_name(stop_wifi_res));
		}
		xSemaphoreGive(config.wifi_synch_semaphore);
		ESP_LOGI(TAG_WiFi, "Suspending task");
		config.wifi_synch_task_suspended = true;
		vTaskSuspend(NULL);
	}
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

			if(!last_block)
			{
				uint8_t * tmp = (uint8_t *) malloc(UPLOAD_BLOCK_SIZE);
				int32_t bytes_read = ft.get_next_buffer_of_training(tmp);
				
				*data = (const uint8_t *) tmp;
				if(bytes_read == -1){
					*size = UPLOAD_BLOCK_SIZE;
				} else {
					last_block = true;
					*size = bytes_read;
				}
			} else {
				*data = NULL;
                *size = 0;
                ESP_LOGI(TAG_WiFi, "Indicating upload is complete.");
			}
        }
        else
        {
            // The last call to this callback is to indicate the result of uploading the previous data block provided.
            // Note: In this last call, data and size pointers are NULL.
            ESP_LOGI(TAG_WiFi, "Last call to getDataCallback (result: %s)", ENUM_TO_STRING(IOTHUB_CLIENT_FILE_UPLOAD_RESULT, result));
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
 * @param
 */
bool synch_with_azure(void)
{
	bool success = false;
	(void)IoTHub_Init();
	device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(ft.get_azure_connection_string(), HTTP_Protocol);

	if (device_ll_handle == NULL)
	{
		ESP_LOGE(TAG_WiFi, "Failure creating Iothub device. Check conn string.");
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

		if(rtc.begin()) {
			rtc.set24Hour();
			rtc.setTime(timeinfo.tm_sec, timeinfo.tm_min, timeinfo.tm_hour, timeinfo.tm_wday, timeinfo.tm_mday, timeinfo.tm_mon +1, timeinfo.tm_year +1900);	
			ESP_LOGI(TAG_WiFi, "Set time to %s", rtc.stringDate());
		} else {
			ESP_LOGE(TAG_WiFi, "Error setting time.");
		}

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

		ft.start_reading_data();
		if (IoTHubDeviceClient_LL_UploadMultipleBlocksToBlob(device_ll_handle, file_name, getDataCallback, NULL) != IOTHUB_CLIENT_OK)
		{
			ESP_LOGE(TAG_WiFi, "Failed to upload");
		}
		else
		{
			ESP_LOGI(TAG_WiFi, "Successful upload.");
			success = true;
		}

		IoTHubDeviceClient_LL_Destroy(device_ll_handle);
	}
	IoTHub_Deinit();
	return success;
}


/**
 * @brief This function is periodically called and firstly checks if the given SSID can be found on the WiFi.
 * If the SSID can be found, we establish a WiFi connection.
 * @return True on successful connection. If True returned: Is connected, else not connected
 */
bool poll_wifi(void){

	if(config.STATE != ADVERTISING && config.STATE != CONNECTED)
		return false;

	wifi_event_group = xEventGroupCreate();

	tcpip_adapter_init();
	esp_err_t ret = esp_event_loop_init(event_handler, NULL);
	if(ret == ESP_FAIL){
		ESP_LOGI(TAG_WiFi, "Event handler already initialized. That is ok. Continue.\n");
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_err_t wifi_init_res = esp_wifi_init(&cfg);  
	if(wifi_init_res != ESP_OK){
		ESP_LOGE(TAG_WiFi, "Could not initialize WiFi: %s", esp_err_to_name(wifi_init_res));
		return false;
	} else ESP_LOGI(TAG_WiFi, "Initialized WiFi");

	wifi_sta_config_t sta_cnfg = {};
	strcpy((char *) sta_cnfg.ssid, ft.get_wifi_ssid());
	strcpy((char *) sta_cnfg.password, ft.get_wifi_password());

	wifi_config_t wifi_config = { .sta = sta_cnfg };
	bool res = (esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK);
	res = res &&  (esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) == ESP_OK);
	res = res &&  (esp_wifi_start() == ESP_OK);
	if(!res)
	{
		esp_wifi_deinit();
		ESP_LOGI(TAG_WiFi, "Set mode, set config or start failed.");
		return false;
	}

	esp_wifi_set_ps(/*WIFI_PS_MAX_MODEM*/WIFI_PS_NONE); //Give 100% power for short amount of time

	//Wait for connection. If not connected after WAIT_TIME many ms, continue as usual.
	ESP_LOGI(TAG_WiFi, "Waiting for connection");
	uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, WAIT_TIME / portTICK_PERIOD_MS);

	if(CONNECTED_BIT & uxBits){
		return true;
	} else{
		esp_err_t wifi_stop_res = esp_wifi_stop();
		if(wifi_stop_res == ESP_OK){
			if(esp_wifi_deinit() != ESP_OK){
				ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi");
			} else 	ESP_LOGI(TAG_WiFi, "Deinited WiFi l.342");
		} else {
			ESP_LOGE(TAG_WiFi, "Could not stop WiFi: %s", esp_err_to_name(wifi_stop_res));
		}
		ESP_LOGW(TAG_WiFi, "Experienced timeout when trying to connect to WiFi. Maybe password wrong.");
		return false;
	}
}
