#include "ChazeMeta.h"

ChazeMeta* global_chaze_meta = new ChazeMeta();

ChazeMeta::ChazeMeta() { }

bool ChazeMeta::initialize_flash()
{
	esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		if(nvs_flash_erase() == ESP_OK){
			if(nvs_flash_init() != ESP_OK){
				ESP_LOGE(TAG, "Failed to init NVS flash.");
                return false;
			}
		} else{
			ESP_LOGE(TAG, "Failed to erase flash.");
            return false;
		}
	}
	return true;
}

char * ChazeMeta::get_string_pointer_from_memory(const char * indicator, size_t max_len, char * default_value)
{
	char new_value[max_len];
	if(!initialize_flash())
	{
		uint16_t len_s = strlen(default_value);
		char * ret_val = (char *) malloc(len_s+1);
		memcpy(ret_val, default_value, len_s);
		ret_val[len_s] = '\0';
		return ret_val;
	}
	nvs_handle my_handle;
	esp_err_t err = nvs_open(indicator, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
		err = nvs_get_str(my_handle, indicator, new_value, &max_len);
		switch (err) {
			case ESP_OK: {
				uint16_t len = 0;
				for(int i=0;i<max_len;i++)
				{
					if(new_value[i] == '\0')
					{
						len = i+1;
						break;
					}
				}
				char * final_value = (char *) malloc(len);
				if(final_value == NULL){
					config.STATE = DEEPSLEEP;
					return (char*) malloc(1); // All hope is lost and we go to sleep.
				}
				memcpy(final_value, new_value, len);
				if(DEBUG) ESP_LOGI(TAG, "Done");
				if(DEBUG) ESP_LOGI(TAG, "%s is = %s", indicator,final_value);
				return final_value;
			} break;
			case ESP_ERR_NVS_NOT_FOUND: {
				ESP_LOGE(TAG, "The value is not initialized yet!");
			} break;
			default :
				ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
		}	
	}
	uint16_t len_s = strlen(default_value);
	char * ret_val = (char *) malloc(len_s+1);
	memcpy(ret_val, default_value, len_s);
	ret_val[len_s] = '\0';
	if(DEBUG) ESP_LOGI(TAG, "The %s is = %s", indicator, ret_val);
	return ret_val;
}

bool ChazeMeta::set_string_in_memory(char * indicator, char * value, uint8_t size)
{
	if(DEBUG) ESP_LOGI(TAG, "Size is %d", size);
	
	char new_value[size+1];
	memcpy(new_value, value, size);
	new_value[size] = '\0';
	if(!initialize_flash())
	{
		return false;
	}

	nvs_handle my_handle;
	esp_err_t err = nvs_open(indicator, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		return false;
    } else {
		err = nvs_set_str(my_handle, indicator, new_value);
		if(err != ESP_OK)
		{
			ESP_LOGE(TAG, "Error setting new %s.", indicator);
			return false;
		}
    }
	return true;
}

char * ChazeMeta::get_device_name(void)
{
	return this->get_string_pointer_from_memory("device_name", 128, "chaze-3");
}

char * ChazeMeta::get_azure_connection_string(void)
{
	return this->get_string_pointer_from_memory("conn_string", 512, "HostName=chaze-iot-hub.azure-devices.net;DeviceId=chaze-3;SharedAccessKey=9vhufTtWrFy9vtucS1rzc4eKt9eIyfk6XnCoYIfI/7Y=");
	// TODO Must be initially set to any value. Maybe set to one device to check if we forgot to save the correct conn string on this device?
}


char * ChazeMeta::get_wifi_ssid(void)
{
	return this->get_string_pointer_from_memory("ssid", 128, "Refhaus_Mitte/Zentral");
}


char * ChazeMeta::get_wifi_password(void)
{
	return this->get_string_pointer_from_memory("password", 128, "Amm+Tag+Vc+0");
}

char * ChazeMeta::get_container_name(void)
{
	return this->get_string_pointer_from_memory("container", 128, "default");
}

bool ChazeMeta::set_container_name(char * name, uint8_t size)
{
	return this->set_string_in_memory("container", name, size);
}

bool ChazeMeta::set_device_name(char * name, uint8_t size)
{
	return this->set_string_in_memory("device_name", name, size);
}

bool ChazeMeta::set_azure_connection_string(char * conn_string, uint8_t size)
{
	return this->set_string_in_memory("conn_string", conn_string, size);
}

bool ChazeMeta::set_wifi_ssid(char * ssid, uint8_t size)
{
	return this->set_string_in_memory("ssid", ssid, size);
}

bool ChazeMeta::set_wifi_password(char * pass, uint8_t size)
{
	return this->set_string_in_memory("password", pass, size);
}

uint8_t ChazeMeta::get_version(char * version_buffer)
{
	char * version_from_mem = this->get_string_pointer_from_memory("version", 128, "v1.0");
	uint8_t len = 4;
	for(int i=0;i<128;i++)
	{
		version_buffer[i] =  version_from_mem[i];
		if(version_from_mem[i] == '\0')
		{
			len = i+1;
			break;
		}
	}
	return len;
}

bool ChazeMeta::set_version(char * version, uint8_t size)
{
	return this->set_string_in_memory("version", version, size);
}

bool ChazeMeta::set_name(char * name, uint8_t size)
{
	if(DEBUG) ESP_LOGI(TAG, "Size is %d", size);
	if(size > 128 || size < 1)
	{
		ESP_LOGE(TAG, "Name length is too long.");
		return false;
	}
	char new_name[size+1];
	memcpy(new_name, name, size);
	new_name[size] = '\0';
	if(!initialize_flash())
	{
		return false;
	}

	nvs_handle my_handle;
	esp_err_t err = nvs_open("name", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		return false;
    } else {
		err = nvs_set_str(my_handle, "name", new_name);
		if(err != ESP_OK)
		{
			ESP_LOGE(TAG, "Error setting new number of unsynched trainings.");
			return false;
		}
    }
	return true;
}

char * ChazeMeta::get_name()
{
	return this->get_string_pointer_from_memory("name", 128, "Chaze Band");
}