#include "Chaze_OTA.h"

const char * TAG_OTA = "Chaze OTA";

const int OTA_CONNECTED_BIT = BIT0;

EventGroupHandle_t ota_wifi_event_group;
EventBits_t ota_uxBits;
Flashtraining ota_ft;

cJSON *json;
bool new_firmware_available;
float current_version;

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
            ESP_LOGD(TAG_OTA, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG_OTA, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG_OTA, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG_OTA, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA: {
            ESP_LOGD(TAG_OTA, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                char rcv_buffer[evt->data_len];
                memcpy(rcv_buffer,(char *)evt->data, evt->data_len);
    			json = cJSON_Parse(rcv_buffer);
    			if(json == NULL) printf("downloaded file is not a valid json, aborting...\n");
    			else {
    				cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
    				double new_version = version->valuedouble;
    				if(new_version > current_version){
    					//Set the flag
    					new_firmware_available = true;
    				}
    				ESP_LOGI(TAG_OTA, "Version is: %.02f\n", new_version);
    			}
        }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG_OTA, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG_OTA, "HTTP_EVENT_DISCONNECTED");
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


/**
 * @brief Send a single GET request to firmware page. This will trigger an event, which is
 * handles by the event handler. The event handler will then set a flag that the file is ready to be read.
 */
void check_for_update(void)
{
    esp_http_client_config_t config = {
        .url = "https://www.chaze-sports.com/firmware.json",
		.cert_pem = msft_ssl_cert_pem_start,
		.event_handler = _http_event_handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    esp_http_client_cleanup(client);
}

// WiFi semaphore was already acquired prior to this call. Need to check if we are connected to the WiFi
uint8_t perform_OTA(void)
{
	if(!config.wifi_connected)
	{
		config.wifi_connected = poll_wifi();
		if(!config.wifi_connected)
			return 1;
	}
	// We are connected to WiFi here and we acquired the wifi lock. That means the wifi task will block
	check_for_update();

	if(new_firmware_available)
	{
		ESP_LOGI(TAG_OTA, "New firmware available");
	}

	return 0;

}