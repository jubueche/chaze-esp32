#include "wifi_synch.h"
#include "Chaze_Advertising.h"

//After we scanned and saw that the SSID was available, connect and timeout connection try after 9s.
#define WAIT_TIME 9000
#define MAX_APs 20
#define SCAN_INTERVAL 20 //Scan every X seconds, then go to light sleep and resume
#define LONG_TIME 0xffff


const char * TAG_WiFi = "Chaze-WIFI-Synch";
uint8_t tmp_upload_buffer[UPLOAD_BLOCK_SIZE];

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

			uint16_t number_of_unsynched_trainings = global_ft->meta_number_of_unsynced_trainings();
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
					if(!success)
					{
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
				int num_written = global_ft->get_next_buffer_with_size(tmp_upload_buffer, UPLOAD_BLOCK_SIZE);
				ESP_LOGI(TAG_WiFi, "Free heap space: %d", esp_get_free_heap_size());

				*data = (const uint8_t *) tmp_upload_buffer;

				if(num_written == -1)
				{
					if(DEBUG)
					{
						// 24 bytes lost everytime
						ESP_LOGI(TAG_WiFi, "Block with size %d", UPLOAD_BLOCK_SIZE);
						for(int i=0;i<UPLOAD_BLOCK_SIZE;i++)
						{
							printf("%d ",tmp_upload_buffer[i]);
						}
						printf("\n");
					}
					*size = UPLOAD_BLOCK_SIZE;
				} else {
					if(DEBUG)
					{
						ESP_LOGI(TAG_WiFi, "Last block with size %d", num_written);
						for(int i=0;i<num_written;i++)
						{
							printf("%d ",tmp_upload_buffer[i]);
						}
						printf("\n");
					}
					*size = num_written;
					last_block = true;
				}
			} else {
				*data = NULL;
                *size = 0;
                if(DEBUG) ESP_LOGI(TAG_WiFi, "Indicating upload is complete.");
				last_block = false;
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
		// Abort if we are suddenly in RECORD or DEEPSLEEP state.
		ESP_LOGW(TAG_WiFi, "State has changed in Callback.");
		return IOTHUB_CLIENT_FILE_UPLOAD_GET_DATA_ABORT;
	}
}

/**
 * @brief Initializes the IoTHub client, sets options, sets filename, uploads data to blob.
 * @param
 */
bool synch_with_azure(void)
{
	bool success = true;
	(void)IoTHub_Init();
	char * conn_string = global_chaze_meta->get_azure_connection_string();
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

		// rtc is initialized in main
		rtc.set24Hour();
		rtc.setTime(timeinfo.tm_sec, timeinfo.tm_min, timeinfo.tm_hour, timeinfo.tm_wday, timeinfo.tm_mday, timeinfo.tm_mon +1, timeinfo.tm_year +1900);

		char * container_name = global_chaze_meta->get_container_name();
		int num_trainings = global_ft->meta_total_number_of_trainings();
		ESP_LOGI(TAG_WiFi, "Number of trainings is %d", num_trainings);

		
		for(int i=0;i<num_trainings;i++)
		{
			// Skip if this training was already synched
			if(global_ft->meta_is_synced(i))
			{
				ESP_LOGI(TAG_WiFi, "Training %d is already synched.", i);
				continue;
			}
			char file_name[128]; //64+32+puffer
			strcpy(file_name, container_name); // This is the user-id of the current user
			strcat(file_name, "/");
			uint8_t date_buf[5];
			if(global_ft->meta_get_training_starttime(i,date_buf))
			{
				uint8_t year = date_buf[0];
				uint8_t month = date_buf[1];
				uint8_t date = date_buf[2];
				uint8_t hour = date_buf[3];
				uint8_t min = date_buf[4];
				char date_string[128];
				sprintf(date_string, "%02d-%02d-20%02d-%02d-%02d", date,month,year,hour,min);
				ESP_LOGI(TAG_WiFi, "Start training date is %s", date_string);
				strcat(file_name, (const char *) date_string);				
			} else {
				strcat(file_name, "00-00-0000-00-00");
			}

			strcat(file_name, ".txt");
			if(DEBUG) ESP_LOGI(TAG_WiFi, "File name is %s", file_name);

			if(!global_ft->start_reading_data(i)){
				success = false;
				ESP_LOGE(TAG_WiFi, "Failed to start reading training.");
				break;
			}

			ESP_LOGI(TAG_WiFi, "Synching training with size %d bytes", global_ft->meta_get_training_size(i));

			if (IoTHubDeviceClient_LL_UploadMultipleBlocksToBlob(device_ll_handle, file_name, getDataCallback, NULL) != IOTHUB_CLIENT_OK)
			{
				ESP_LOGE(TAG_WiFi, "Failed to upload");
				success = false;
				break;
			}
			else
			{
				if(DEBUG) ESP_LOGI(TAG_WiFi, "Successful upload.");
				global_ft->init_delete_training(i); // This sets the ready-to-delete-flag AND the is_synced flag
				success = true;
			}

		}
		free(container_name);

		if(!success)
		{
			global_ft->abort_reading_data();
			ESP_LOGE(TAG_WiFi, "Unsuccessful upload. Calling abort_reading_data()");
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
void poll_wifi(void){

	if(config.STATE != ADVERTISING && config.STATE != CONNECTED)
		return;

	config.wifi_setup_once = true;
	
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
	char * ssid = global_chaze_meta->get_wifi_ssid();
	char * pass = global_chaze_meta->get_wifi_password();
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
