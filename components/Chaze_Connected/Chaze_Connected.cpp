#include "Chaze_Connected.h"

const char * TAG_Con = "Chaze-Connected";


// TODO Fix memory problem and maybe transmit some training meta data before sending the data

class ConnectedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        uint8_t size = rxValue.length();
        buffer->size = size - 1; // Because of \n
        if(DEBUG) ESP_LOGI(TAG_Con, "Length is %d", size);
        
        for (int i = 0; i < size; i++)
        {
            if(DEBUG) ESP_LOGI(TAG_Con, "C is %c", rxValue[i]);
            buffer->data[i] = rxValue[i];
        }

        if(CONNECTED_STATE == IDLE)
        {
            if(buffer->size == 1)
            {
                // Received a command. Switch the state accordingly
                if(strncmp(buffer->data, "b", buffer->size) == 0)
                {
                    CONNECTED_STATE = BATTERY;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received battery command.");
                } else if(strncmp(buffer->data, "n", buffer->size) == 0)
                {
                    CONNECTED_STATE = NAME;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received name command.");
                } else if(strncmp(buffer->data, "w", buffer->size) == 0)
                {
                    CONNECTED_STATE = WIFI;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received wifi command.");
                } else if(strncmp(buffer->data, "v", buffer->size) == 0)
                {
                    CONNECTED_STATE = VERSION;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received get version command.");
                } else if(strncmp(buffer->data, "f", buffer->size) == 0)
                {
                    CONNECTED_STATE = OTA;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received perform OTA command.");
                } else if(strncmp(buffer->data, "d", buffer->size) == 0)
                {
                    CONNECTED_STATE = DATA;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received send data command.");
                } else if(strncmp(buffer->data, "c", buffer->size) == 0)
                {
                    CONNECTED_STATE = CONN_STRING;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received set conn string command.");
                } else if(strncmp(buffer->data, "q", buffer->size) == 0)
                {
                    CONNECTED_STATE = DEVICE_NAME;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received set device name command.");
                } else if(strncmp(buffer->data, "s", buffer->size) == 0)
                {
                    CONNECTED_STATE = SET_VERSION;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received set version command.");
                } else if(strncmp(buffer->data, "x", buffer->size) == 0)
                {
                    CONNECTED_STATE = CONTAINER;
                    if(DEBUG) ESP_LOGI(TAG_Con, "Received set container command.");
                }
                else { //default
                    ESP_LOGE(TAG_Con, "Command %c not recognized. Reset the state to IDLE.", buffer->data[0]);
                    CONNECTED_STATE = IDLE;
                }
            }

        } else
        {
            // Update the STATE according to the last state.
            // Example: WiFi -> Received Name
            switch(CONNECTED_STATE) {
                case NAME:
                {
                    CONNECTED_STATE = NAME_RECEIVED;
                    break;
                }
                case WIFI:
                {
                    CONNECTED_STATE = SSID_RECEIVED;
                    break;
                }
                case SSID_RECEIVED:
                {
                    CONNECTED_STATE = WIFI_2;
                    break;
                }
                case WIFI_2:
                {
                    CONNECTED_STATE = PASS_RECEIVED;
                    break;
                }
                case DATA:
                {
                    // Check if we have received a "1" or a "0" and set the "synched training flag" corresp.
                    if(strncmp(buffer->data, "1", buffer->size) == 0)
                    {
                        if(DEBUG) ESP_LOGI(TAG_Con, "Synched successful.");
                        config.synched_training = SYNCH_COMPLETE;
                    } else {
                        ESP_LOGE(TAG_Con, "Synch unsuccessful.");
                        config.synched_training = SYNCH_INCOMPLETE;
                    }
                    break;
                }
                case CONN_STRING:
                {
                    CONNECTED_STATE = CONN_STRING_RECEIVED;
                    break;
                }
                case DEVICE_NAME:
                {
                    CONNECTED_STATE = DEVICE_NAME_RECEIVED;
                    break;
                }
                case SET_VERSION:
                {
                    CONNECTED_STATE = VERSION_RECEIVED;
                    break;
                }
                case CONTAINER:
                {
                    CONNECTED_STATE = CONTAINER_RECEIVED;
                    break;
                }
                default:
                {
                    CONNECTED_STATE = IDLE;
                    break;
                }
            }
            vTaskDelay(100);
        }
      }
      else {
          if(DEBUG) ESP_LOGI(TAG_Con, "Received one char.");
      }
    }
};

BLECharacteristicCallbacks *callback = new ConnectedCallbacks();


void connected()
{
    gpio_set_level(GPIO_BLUE, 1);
    CONNECTED_STATE = IDLE;
    buffer = (Rx_buffer *) malloc(sizeof(Rx_buffer));
    if(buffer == NULL)
    {
        ESP_LOGE(TAG_Con, "Could not allocate buffer for receiving data");
    }
    
    ble->pRxCharacteristic->setCallbacks(callback);

    if(DEBUG) ESP_LOGI(TAG_Con, "Connected");

    ble->write("r");

    while(config.ble_connected && !config.wifi_connected)
    {
        switch(CONNECTED_STATE) {
            case IDLE:
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;
            }
            case BATTERY:
            {
                send_battery();
                break;
            }
            case NAME_RECEIVED:
            {
                set_name();
                break;
            }
            case SSID_RECEIVED:
            {
                set_ssid();
                break;
            }
            case PASS_RECEIVED:
            {
                set_password();
                break;
            }
            case VERSION:
            {
                get_version();
                break;
            }
            case OTA:
            {
                ota();
                break;
            }
            case DATA:
            {
                synch_data();
                break;
            }
            case CONN_STRING_RECEIVED:
            {
                set_conn_string();
                break;
            }
            case DEVICE_NAME_RECEIVED:
            {
                set_device_name();
                break;
            }
            case VERSION_RECEIVED:
            {
                set_version();
                break;
            }
            case CONTAINER_RECEIVED:
            {
                set_container();
                break;
            }
            default:
            {
                if(DEBUG) ESP_LOGI(TAG_Con, "Waiting...");
                break;
            }
        }

        // Constantly check if we received smth. from BLE and then react accordingly.
        vTaskDelay(500 / portTICK_PERIOD_MS);

    }
    if(DEBUG) ESP_LOGI(TAG_Con, "Going back to advertising mode.");
    if(config.wifi_connected)
    {
        ble->pServer->disconnect(ble->pServer->getConnId());
    }
    config.STATE = ADVERTISING;
    free(buffer);
    gpio_set_level(GPIO_BLUE, 0);
}

void set_container()
{
    if(buffer->size > 128)
        return;
    if(DEBUG) ESP_LOGI(TAG_Con, "Set container name");
    char container[buffer->size+1];
    memcpy(container, buffer->data, buffer->size);
    container[buffer->size] = '\0';
    if(global_chaze_meta->set_container_name(container, buffer->size))
    {
        ble->write("1\n");
    } else{
        ble->write("0\n");
    }
    if(DEBUG) ESP_LOGI(TAG_Con, "Set the container to to %s", container);
    CONNECTED_STATE = IDLE;
}

void set_conn_string()
{
    if(DEBUG) ESP_LOGI(TAG_Con, "Set Conn String");
    char cs[buffer->size+1];
    memcpy(cs, buffer->data, buffer->size);
    cs[buffer->size] = '\0';
    if(global_chaze_meta->set_azure_connection_string(cs, buffer->size))
    {
        ble->write("1\n");
    } else{
        ble->write("0\n");
    }
    if(DEBUG) ESP_LOGI(TAG_Con, "Set the conn string to to %s", cs);
    CONNECTED_STATE = IDLE;
}

void set_device_name()
{
    if(DEBUG) ESP_LOGI(TAG_Con, "Set device name");
    if(buffer->size > 128)
        return;
    char device_name[buffer->size+1];
    memcpy(device_name, buffer->data, buffer->size);
    device_name[buffer->size] = '\0';
    if(global_chaze_meta->set_device_name(device_name, buffer->size))
    {
        ble->write("1\n");
    } else{
        ble->write("0\n");
    }
    if(DEBUG) ESP_LOGI(TAG_Con, "Set the device name to to %s", device_name);
    CONNECTED_STATE = IDLE;
}

void set_version()
{
    if(DEBUG) ESP_LOGI(TAG_Con, "Set version");
    if(buffer->size > 128)
        return;
    char version[buffer->size+1];
    memcpy(version, buffer->data, buffer->size);
    version[buffer->size] = '\0';
    if(global_chaze_meta->set_version(version, buffer->size))
    {
        ble->write("1\n");
    } else{
        ble->write("0\n");
    }
    if(DEBUG) ESP_LOGI(TAG_Con, "Set version to to %s", version);
    CONNECTED_STATE = IDLE;
}

void send_battery()
{
    uint8_t battery_percentage = config.get_battery_level();
    ble->write(std::to_string(battery_percentage));
    CONNECTED_STATE = IDLE;
}

void set_name()
{
    if(buffer->size > 128)
        return;
    if(DEBUG) ESP_LOGI(TAG_Con, "Set Name");
    char name[buffer->size+1];
    memcpy(name, buffer->data, buffer->size);
    name[buffer->size] = '\0';
    if(global_chaze_meta->set_name(name, buffer->size))
    {
        ble->write("1\n");
    } else{
        ble->write("0\n");
    }
    if(DEBUG) ESP_LOGI(TAG_Con, "Set the name to %s", name);
    CONNECTED_STATE = IDLE;
}

void set_ssid()
{
    if(buffer->size > 128)
        return;
    if(DEBUG) ESP_LOGI(TAG_Con, "Set SSID");
    //char ssid[buffer->size];
    char * ssid = new char[buffer->size];
    memcpy(ssid, buffer->data, buffer->size);

    if(global_chaze_meta->set_wifi_ssid(ssid, buffer->size))
    {
        ble->write("1\n");
    } else {
        ble->write("0\n");
    }

    delete [] ssid;
    ssid = NULL;
    CONNECTED_STATE = WIFI_2;
}

void set_password()
{
    if(buffer->size > 128)
        return;
    if(DEBUG) ESP_LOGI(TAG_Con, "Set password");
    char password[buffer->size];
    memcpy(password, buffer->data, buffer->size);

    if(global_chaze_meta->set_wifi_password(password, buffer->size))
    {
        ble->write("1\n");
    } else {
        ble->write("0\n");
    }
    CONNECTED_STATE = IDLE;
}

void get_version()
{
    char version[128];
    uint8_t n = global_chaze_meta->get_version(version);
    ble->write(version, n);
    CONNECTED_STATE = IDLE;
}

void ota()
{
    // We can be sure this task has the wifi_synch_semaphore. This means we are either connected to WiFi or not,
    // but there cannot happen a synch with azure.
    if(DEBUG) ESP_LOGI(TAG_Con, "Performing OTA update.");
    if(!config.wifi_connected)
	{
		poll_wifi();
		if(!config.wifi_connected)
        {
            if(DEBUG) ESP_LOGI(TAG_Con, "Not connected.");
			ble->write("\nn");
            CONNECTED_STATE = IDLE;
            return;
        }
	}
    // We are connected
    ble->write("\ns");
    if(DEBUG) ESP_LOGI(TAG_Con, "Connected to WiFi, attempting OTA update.");
    perform_OTA();
    
    esp_err_t wifi_stop_res = esp_wifi_stop();
    if(wifi_stop_res == ESP_OK){
        if(esp_wifi_deinit() != ESP_OK){
            ESP_LOGE(TAG_Con, "Failed to deinit WiFi");
        } else {
            if(DEBUG) ESP_LOGI(TAG_Con, "Deinited WiFi l.342");
        }
    } else {
        ESP_LOGE(TAG_Con, "Could not stop WiFi: %s", esp_err_to_name(wifi_stop_res));
    }
    config.wifi_connected = false;
    CONNECTED_STATE = IDLE;

}

void synch_data()
{

    if(DEBUG) ESP_LOGI(TAG_Con, "Synching data");
    uint16_t num_trainings = global_ft->meta_total_number_of_trainings();

    uint8_t data[UPLOAD_BLOCK_SIZE_BLE];
    ble->write(std::to_string(global_ft->meta_number_of_unsynced_trainings()));
    uint8_t date_buf[5];

    for(int i=0;i<num_trainings;i++)
    {
        // Skip this training if not synced yet
        if(global_ft->meta_is_synced(i))
        {
            continue;
        }
        // Call again for each training
        global_ft->start_reading_data(i);

        //! TODO Need to get time of the training and send it here!
        if(global_ft->meta_get_training_starttime(i,date_buf))
        {
            uint8_t year = date_buf[0];
            uint8_t month = date_buf[1];
            uint8_t date = date_buf[2];
            uint8_t hour = date_buf[3];
            uint8_t min = date_buf[4];
            char date_string[128];
            sprintf(date_string, "%02d-%02d-20%02d-%02d-%02d", date,month,year,hour,min);
            ESP_LOGI(TAG_Con, "Start training date is %s", date_string);
            ble->write((const char *) date_string);
        } else {
            ble->write("00-00-0000-00-00");
        }

        uint32_t num_written;
        do {
            num_written = global_ft->get_next_buffer_of_training(data);
            ble->write(data, num_written);
            for(int j=0;j<num_written;j++)
            {
                printf("%d ",data[j]);
            }
            printf("\n");

        } while(num_written == UPLOAD_BLOCK_SIZE_BLE);
        printf("\n");
        ble->write(EOF); // Write EOF

        unsigned long response_timer = millis();
        while (millis() - response_timer < 60000)
        {
            if(config.synched_training != AWAITING)
                break;
            vTaskDelay(300 / portTICK_PERIOD_MS);
        }
        
        if(config.synched_training == SYNCH_COMPLETE)
        {
            if(DEBUG) ESP_LOGI(TAG_Con, "Successful synch.");
            // A call to this function sets the ready-to-delete and synced flag of this training
            global_ft->init_delete_training(i);
        } else if(config.synched_training == SYNCH_INCOMPLETE)
        {
            ESP_LOGE(TAG_Con, "Unsuccessful synch.");
            CONNECTED_STATE = IDLE; 
            global_ft->abort_reading_data();
            return;
        } else {
            ESP_LOGE(TAG_Con, "Timeout waiting for response.");
            CONNECTED_STATE = IDLE; 
            global_ft->abort_reading_data();
            return;
        }
        config.synched_training = AWAITING; // Reset the variable for the next trainings.
            
    }
    ble->write(EOF);
    CONNECTED_STATE = IDLE;
}