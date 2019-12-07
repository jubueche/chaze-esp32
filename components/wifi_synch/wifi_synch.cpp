#include "wifi_synch.h"
#include "Chaze_Advertising.h"

//After we scanned and saw that the SSID was available, connect and timeout connection try after 9s.
#define WAIT_TIME 9000
#define MAX_APs 20
#define SCAN_INTERVAL 20 //Scan every X seconds, then go to light sleep and resume
#define LONG_TIME 0xffff


const char * TAG_WiFi = "Chaze-WIFI-Synch";
uint8_t * tmp_upload_buffer = new uint8_t[UPLOAD_BLOCK_SIZE];

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
			if(DEBUG) ESP_LOGI(TAG_WiFi, "SYSTEM_EVENT_STA_START");
			ESP_ERROR_CHECK(esp_wifi_connect());
		break;
		case SYSTEM_EVENT_STA_GOT_IP:
			if(DEBUG) ESP_LOGI(TAG_WiFi, "SYSTEM_EVENT_STA_GOT_IP");
			if(DEBUG) ESP_LOGI(TAG_WiFi, "got ip:%s\n",
			ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
			config.wifi_connected = true;
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			if(DEBUG) ESP_LOGI(TAG_WiFi, "SYSTEM_EVENT_STA_DISCONNECTED");
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

	if(config.wifi_synch_semaphore == NULL)
	{
		config.wifi_synch_semaphore = xSemaphoreCreateMutex();
		xSemaphoreGive(config.wifi_synch_semaphore);
	}

	config.wifi_connected = false;
	
	for(;;)
	{
		// Continue to try and synch even if we are connected with the phone.
		// We will stay in Adv. mode max. 10min. This should be more than enough time to try & synch. We continue to
		// Synch in connected mode.
		while(config.STATE == ADVERTISING || config.STATE == CONNECTED ){

			uint16_t number_of_unsynched_trainings = global_ft->get_number_of_unsynched_trainings();
			// TODO Test that case
			// vTaskResume(task_handle) will be called when the number of trainings is greater zero again
			// Pre: config.wifi_connected == false
			if(number_of_unsynched_trainings == 0 || !config.allow_azure)
			{
				config.wifi_synch_task_suspended = true;
				xSemaphoreGive(config.wifi_synch_semaphore);
				vTaskSuspend(NULL);
				if(DEBUG) ESP_LOGI(TAG_WiFi, "Not allowing azure or num. uns. trainings is 0: Suspended wifi synch task");
			} else {
				if(!config.wifi_connected)
				{
					poll_wifi(); // Tries to connect to WiFi. If successful sets config.wifi_connected to true.
					if(DEBUG) ESP_LOGI(TAG_WiFi, "After poll_wifi(): Free heap space is %d", esp_get_free_heap_size());
				} // No else if!
				if(config.wifi_connected){
					if(DEBUG) ESP_LOGI(TAG_WiFi, "Connected to WiFi, trying to synch data now...");

					for(;;)
					{
						if(xSemaphoreTake(config.wifi_synch_semaphore, portMAX_DELAY) == pdTRUE)
						{
							break;
						}
						vTaskDelay(80);
					}
					if(DEBUG) ESP_LOGI(TAG_WiFi, "Obtained wifi_synch_semaphore. Synching with azure.");
					bool success = false;
					if(config.allow_azure)
					{
						success = synch_with_azure();
					}
					xSemaphoreGive(config.wifi_synch_semaphore);
					if(DEBUG) ESP_LOGI(TAG_WiFi, "Released wifi_synch_semaphore after synch with azure.");

					// Set the new number of trainings
					if(success)
					{
						global_ft->remove_unsynched_training();
					} else {
						ESP_LOGE(TAG_WiFi, "Unsuccessful synch with azure");
						config.allow_azure = false;
					}
					// Clean up WiFi
					esp_err_t wifi_stop_res = esp_wifi_stop();
					if(wifi_stop_res == ESP_OK){
						if(esp_wifi_deinit() != ESP_OK){
							ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi");
						} else 	if(DEBUG) ESP_LOGI(TAG_WiFi, "Deinited WiFi after successful upload.");
					} else {
						ESP_LOGE(TAG_WiFi, "Could not stop WiFi: %s", esp_err_to_name(wifi_stop_res));
					}
					config.wifi_connected = false;
					//Start deleting this training in the background
				}
			}

			// Release the semaphore and suspend for SCAN_INTERVAL number of seconds
			xSemaphoreGive(config.wifi_synch_semaphore);
			vTaskDelay(SCAN_INTERVAL*1000 / portTICK_PERIOD_MS); //Delay for 20s
			if(DEBUG) ESP_LOGI(TAG_WiFi, "After delay.");
		}
		if(DEBUG) ESP_LOGI(TAG_WiFi, "State changed. Wifi is stopping.");
		esp_err_t stop_wifi_res = esp_wifi_stop();
		if(stop_wifi_res == ESP_OK){
			esp_err_t deinit_wifi_res = esp_wifi_deinit();
			if(deinit_wifi_res != ESP_OK){
				ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi: %s", esp_err_to_name(deinit_wifi_res));
			} else 	if(DEBUG) ESP_LOGI(TAG_WiFi, "Deinited WiFi: l.167");
		} else {
			ESP_LOGE(TAG_WiFi, "Could not stop WiFi: %s", esp_err_to_name(stop_wifi_res));
		}
		xSemaphoreGive(config.wifi_synch_semaphore);
		if(DEBUG) ESP_LOGI(TAG_WiFi, "Suspending task");
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
				int32_t bytes_read = global_ft->get_next_buffer_of_training(tmp_upload_buffer);
				

				*data = (const uint8_t *) tmp_upload_buffer;
				if(bytes_read == -1){
					*size = UPLOAD_BLOCK_SIZE;
				} else {
					last_block = true;
					*size = bytes_read;
				}
			} else {
				*data = NULL;
                *size = 0;
                if(DEBUG) ESP_LOGI(TAG_WiFi, "Indicating upload is complete.");
			}
        }
        else
        {
            // The last call to this callback is to indicate the result of uploading the previous data block provided.
            // Note: In this last call, data and size pointers are NULL.
            if(DEBUG) ESP_LOGI(TAG_WiFi, "Last call to getDataCallback (result: %s)", ENUM_TO_STRING(IOTHUB_CLIENT_FILE_UPLOAD_RESULT, result));
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
	char * conn_string = global_ft->get_azure_connection_string();
	device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(conn_string, HTTP_Protocol);
	free(conn_string);
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
			if(DEBUG) ESP_LOGI(TAG_WiFi, "Set time to %s", rtc.stringDate());
		} else {
			ESP_LOGE(TAG_WiFi, "Error setting time.");
		}

		for(int i=0;i<strlen(strftime_buf);i++){
			if(strftime_buf[i] == ' '){
				strftime_buf[i] = '_';
			}
		}
		char file_name[128]; //64+32+puffer
		//char * device_name = global_ft->get_device_name();
		char * container_name = global_ft->get_container_name();
		
		strcpy(file_name, container_name); // This is the user-id of the current user
		strcat(file_name, "/");
		strcat(file_name, "09-01-1997-59-13"); //DD-MM-YYYY-MM-HH, Needs to come from training meta data
		strcat(file_name, ".txt");
		//free(device_name);
		free(container_name);
		if(DEBUG) ESP_LOGI(TAG_WiFi, "File name is %s", file_name);

		global_ft->start_reading_data();
		
		if (IoTHubDeviceClient_LL_UploadMultipleBlocksToBlob(device_ll_handle, file_name, getDataCallback, NULL) != IOTHUB_CLIENT_OK)
		{
			ESP_LOGE(TAG_WiFi, "Failed to upload");
		}
		else
		{
			if(DEBUG) ESP_LOGI(TAG_WiFi, "Successful upload.");
			success = true;
		}


		global_ft->abort_reading_data();

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
void poll_wifi(void){

	if(config.STATE != ADVERTISING && config.STATE != CONNECTED)
		return;

	tcpip_adapter_init();
	if(!config.event_handler_set)
	{
		esp_err_t ret = esp_event_loop_init(event_handler, NULL);
		if(ret == ESP_FAIL){
			if(DEBUG) ESP_LOGI(TAG_WiFi, "Event handler already initialized. That is ok. Continue.\n");
		}
		config.event_handler_set = true;
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	esp_err_t wifi_init_res = esp_wifi_init(&cfg);  

	if(wifi_init_res != ESP_OK){
		ESP_LOGE(TAG_WiFi, "Could not initialize WiFi: %s", esp_err_to_name(wifi_init_res));
		return;
	} else if(DEBUG) ESP_LOGI(TAG_WiFi, "Initialized WiFi");

	wifi_sta_config_t sta_cnfg = {};
	char * ssid = global_ft->get_wifi_ssid();
	char * pass = global_ft->get_wifi_password();
	strcpy((char *) sta_cnfg.ssid, ssid);
	strcpy((char *) sta_cnfg.password, pass);
	free(ssid);
	free(pass);

	wifi_config_t wifi_config = { .sta = sta_cnfg };
	bool res = (esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK);
	res = res &&  (esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) == ESP_OK);
	res = res &&  (esp_wifi_start() == ESP_OK);
	if(!res)
	{
		esp_wifi_deinit();
		if(DEBUG) ESP_LOGI(TAG_WiFi, "Set mode, set config or start failed.");
		return;
	}

	esp_wifi_set_ps(/*WIFI_PS_MAX_MODEM*/WIFI_PS_NONE); //Give 100% power for short amount of time

	//Wait for connection. If not connected after WAIT_TIME many ms, continue as usual.
	if(DEBUG) ESP_LOGI(TAG_WiFi, "Waiting for connection");
	
	unsigned long start = millis();
	while(millis() - start < WAIT_TIME)
	{
		if(config.wifi_connected)
			break;
		vTaskDelay(500);
	}
	
	if(!config.wifi_connected) {
		esp_err_t wifi_stop_res = esp_wifi_stop();
		if(wifi_stop_res == ESP_OK){
			if(esp_wifi_deinit() != ESP_OK){
				ESP_LOGE(TAG_WiFi, "Failed to deinit WiFi");
			} else {
				if(DEBUG) ESP_LOGI(TAG_WiFi, "Deinited WiFi l.342");
			}
		} else {
			ESP_LOGE(TAG_WiFi, "Could not stop WiFi: %s", esp_err_to_name(wifi_stop_res));
		}
		ESP_LOGW(TAG_WiFi, "Experienced timeout when trying to connect to WiFi. Maybe password wrong.");
		return;
	} else {
		if(DEBUG) ESP_LOGI(TAG_WiFi, "Connected to WiFi");
	}
}
