#include "Chaze_OTA.h"

const char * TAG_OTA = "Chaze OTA";

const int OTA_CONNECTED_BIT = BIT0;

EventGroupHandle_t ota_wifi_event_group;
EventBits_t ota_uxBits;	

cJSON *json;
bool new_firmware_available;
float current_version;
float new_version;

extern const char msft_ssl_cert_pem_start[] asm("_binary_msft_ssl_cert_pem_start");
extern const char msft_ssl_cert_pem_end[]   asm("_binary_msft_ssl_cert_pem_end");


/**
 * @ brief Event handler for OTA update. Sets flag received JSON with version.
 * @param evt
 * @return Error if not successful.
 */
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG_OTA, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA: {
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                char rcv_buffer[evt->data_len];
                memcpy(rcv_buffer,(char *)evt->data, evt->data_len);
    			json = cJSON_Parse(rcv_buffer);
    			if(json == NULL) printf("downloaded file is not a valid json, aborting...\n");
    			else {
    				cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
					if(version == NULL)
					{
						ESP_LOGE(TAG_OTA, "Encountered null pointer in field.");
					} else {
						new_version = version->valuedouble;
						if(new_version > current_version){
							//Set the flag
							new_firmware_available = true;
						}
						ESP_LOGI(TAG_OTA, "Version is: %.02f\n", new_version);
					}
    			}
        }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}


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
			ESP_LOGI(TAG_OTA, "SYSTEM_EVENT_STA_START");
			ESP_ERROR_CHECK(esp_wifi_connect());
		break;
		case SYSTEM_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG_OTA, "SYSTEM_EVENT_STA_GOT_IP");
			ESP_LOGI(TAG_OTA, "got ip:%s\n",
			ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
			xEventGroupSetBits(ota_wifi_event_group, OTA_CONNECTED_BIT);
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG_OTA, "SYSTEM_EVENT_STA_DISCONNECTED");
			xEventGroupClearBits(ota_wifi_event_group, OTA_CONNECTED_BIT);
			break;
		default:
			break;
	}
	return ESP_OK;
}


//! NOT TESTED
/**
 * @brief Writes the new firmware version to the SPI flash.
 * @return bool. True if successful.
 */
bool write_new_firmware_version(void)
{
	const char numbers[] = {"0123456789"};
	int before_dot = (int) new_version;
	int after_dot = (int) (10*(new_version - (float) before_dot));
	char version_string[5];
	if(before_dot > 9 || after_dot > 9)
	{
		return false;
	}
	version_string[0] = 'v';
	version_string[1] = numbers[before_dot];
	version_string[2] = '.';
	version_string[3] = numbers[after_dot];
	version_string[4] = '\0';
	return global_ft->set_version(version_string, 5);
}

/**
 * @brief Send a single GET request to firmware page. This will trigger an event, which is
 * handles by the event handler. The event handler will then set a flag that the file is ready to be read.
 */
void check_for_update(void)
{
    esp_http_client_config_t http_config = {
        .url = "https://www.chaze-sports.com/firmware.json",
		.cert_pem = msft_ssl_cert_pem_start,
		.event_handler = _http_event_handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    esp_err_t err = esp_http_client_perform(client);
	if(err != ESP_OK)
	{
		ESP_LOGE(TAG_OTA, "Error when performing request");
		config.STATE = DEEPSLEEP;
		return;
	}

	esp_http_client_cleanup(client);
}

// WiFi semaphore was already acquired prior to this call. 
void perform_OTA(void)
{	
	// Get current version
	const char numbers[] = {"0123456789"};
	char current_version_string[128];
	bool dot_occ = false;
	float version_float = 9.9;
	uint8_t n = global_ft->get_version(current_version_string);
	for(int i=0;i<n;i++)
	{
		if(current_version_string[i] == '.')
		{
			dot_occ = true;
		} else {
			int8_t num = -1;
			for(int j=0;j<10;j++)
			{
				if(current_version_string[i] == numbers[j])
					num = j;
			}
			if(num > -1)
			{
				if(!dot_occ)
					version_float = (float) num;
				if(dot_occ)
					version_float += 0.1*((float) num);
			}
		}
	}
	current_version = version_float;
	new_version = current_version;
	ESP_LOGI(TAG_OTA, "Current version %s transformed to %.4f", current_version_string, current_version);

	check_for_update();

	ble->~Chaze_ble();

	esp_err_t ret;
	esp_http_client_config_t ota_client_config = {
			.url = "https://firmwarestorage.blob.core.windows.net/firmware/app-template.bin",
			.cert_pem = msft_ssl_cert_pem_start,};
	ret = esp_https_ota(&ota_client_config);
	if (ret == ESP_OK) {
		write_new_firmware_version();
		ESP_LOGI(TAG_OTA, "OTA OK, restarting...");
		esp_restart();
	} else {
		ESP_LOGE(TAG_OTA, "OTA failed...");
	}

	// This is the real code.
	/*if(new_firmware_available){
		cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");
		if(cJSON_IsString(file) && (file->valuestring != NULL)) {
			ESP_LOGI(TAG_OTA, "downloading and installing new firmware (%s).", file->valuestring);

			esp_http_client_config_t ota_client_config = {
					.url = file->valuestring,
					.cert_pem = msft_ssl_cert_pem_start,};
			ret = esp_https_ota(&ota_client_config);
			if (ret == ESP_OK) {
				write_new_firmware_version();
				ESP_LOGI(TAG_OTA, "OTA OK, restarting...");
				esp_restart();
			} else {
				ESP_LOGE(TAG_OTA, "OTA failed...");
			}
		}
		else ESP_LOGE(TAG_OTA, "unable to read the new file name, aborting.");
	}*/
}