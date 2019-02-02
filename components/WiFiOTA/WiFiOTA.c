#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "app_wifi.h"
#include "cJSON.h"

#include "esp_http_client.h"

#define MAX_HTTP_RECV_BUFFER 512
static const char *TAG = "HTTP_CLIENT";

cJSON *json;
bool new_firmware_available;
float current_version;

esp_err_t esp_https_ota(esp_http_client_config_t *);

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
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA: {
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
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
    				ESP_LOGI(TAG, "Version is: %.02f\n", new_version);
    			}
        }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
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
        .event_handler = _http_event_handler,
		.cert_pem = msft_ssl_cert_pem_start
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    esp_http_client_cleanup(client);
}

/**
 * @brief Retrieves and returns current firmware version from the SPI flash.
 * @return Firmware version in float.
 */
float get_current_version(void){
	return 0.0;
}

/**
 * @brief Writes the new firmware version to the SPI flash.
 * @return bool. True if successful.
 */
bool write_new_firmware_version(void)
{
	return true;
}

/**
 * @brief Initialises WiFi connection, waits for a connection, checks for updates, updates if
 * necessary and writes the new firmware version to the flash.
 * TODO: Need to verify if we already have a WiFi connection, need to signal if the firmware was updated
 */
void check_and_update(void)
{
	new_firmware_available = false;
	current_version = get_current_version();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ret = nvs_flash_erase();
      if(ret != ESP_OK) ESP_LOGE(TAG, "Failed to erase nvs flash");
      ret = nvs_flash_init();
    }
    if(ret != ESP_OK){
    	ESP_LOGE(TAG, "Failed to init flash.");
    }
    wifi_initialise();

    wifi_wait_connected();
	ESP_LOGI(TAG, "Connected to AP, begin http example");

	check_for_update();

	if(new_firmware_available){
		cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");
		if(cJSON_IsString(file) && (file->valuestring != NULL)) {
			ESP_LOGI(TAG, "downloading and installing new firmware (%s).", file->valuestring);

			esp_http_client_config_t ota_client_config = {
					.url = file->valuestring,
					.cert_pem = msft_ssl_cert_pem_start,};
			ret = esp_https_ota(&ota_client_config);
			if (ret == ESP_OK) {
				write_new_firmware_version();
				ESP_LOGI(TAG, "OTA OK, restarting...");
				esp_restart();
			} else {
				ESP_LOGE(TAG, "OTA failed...");
			}
		}
		else ESP_LOGE(TAG, "unable to read the new file name, aborting.");
	}
}
