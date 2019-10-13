#include "Chaze_Connected.h"


const char * TAG_Con = "Chaze-Connected";


// TODO Set name when advertising
// TODO check if we are connected to wifi before synching data
// TODO Fix memory problem and maybe transmit some training meta data before sending the data

class ConnectedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        uint8_t size = rxValue.length();
        buffer->size = size - 1; // Because of \n
        ESP_LOGI(TAG_Con, "Length is %d", size);
        
        for (int i = 0; i < size; i++)
        {
            ESP_LOGI(TAG_Con, "C is %c", rxValue[i]);
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
                    ESP_LOGI(TAG_Con, "Received battery command.");
                } else if(strncmp(buffer->data, "n", buffer->size) == 0)
                {
                    CONNECTED_STATE = NAME;
                    ESP_LOGI(TAG_Con, "Received name command.");
                } else if(strncmp(buffer->data, "w", buffer->size) == 0)
                {
                    CONNECTED_STATE = WIFI;
                    ESP_LOGI(TAG_Con, "Received wifi command.");
                } else if(strncmp(buffer->data, "v", buffer->size) == 0)
                {
                    CONNECTED_STATE = VERSION;
                    ESP_LOGI(TAG_Con, "Received get version command.");
                } else if(strncmp(buffer->data, "f", buffer->size) == 0)
                {
                    CONNECTED_STATE = OTA;
                    ESP_LOGI(TAG_Con, "Received perform OTA command.");
                } else if(strncmp(buffer->data, "d", buffer->size) == 0)
                {
                    CONNECTED_STATE = DATA;
                    ESP_LOGI(TAG_Con, "Received send data command.");
                } else { //default
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
                        ESP_LOGI(TAG_Con, "Synched successful.");
                        config.synched_training = SYNCH_COMPLETE;
                    } else {
                        ESP_LOGE(TAG_Con, "Synch unsuccessful.");
                        config.synched_training = SYNCH_INCOMPLETE;
                    }
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
    }
};

BLECharacteristicCallbacks *callback = new ConnectedCallbacks();


void connected()
{
    CONNECTED_STATE = IDLE;
    buffer = (Rx_buffer *) malloc(sizeof(Rx_buffer));
    
    ble->pRxCharacteristic->setCallbacks(new ConnectedCallbacks());

    ESP_LOGI(TAG_Con, "Connected");


    while(config.ble_connected)
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
                perform_ota();
                break;
            }
            case DATA:
            {
                synch_data();
                break;
            }
            default:
            {
                ESP_LOGI(TAG_Con, "Waiting...");
                break;
            }
        }

        // Constantly check if we received smth. from BLE and then react accordingly.
        vTaskDelay(500 / portTICK_PERIOD_MS);

    }
    config.STATE = ADVERTISING;
    free(buffer);

}

void send_battery()
{
    uint8_t battery_percentage = config.get_battery_level();
    ble->write(std::to_string(battery_percentage));
    CONNECTED_STATE = IDLE;
}

void set_name()
{
    ESP_LOGI(TAG_Con, "Set Name");
    char name[buffer->size+1];
    memcpy(name, buffer->data, buffer->size);
    name[buffer->size] = '\0';
    if(ft.set_name(name, buffer->size))
    {
        ble->write("1\n");
    } else{
        ble->write("0\n");
    }
    ESP_LOGI(TAG_Con, "Set the name to %s", name);
    CONNECTED_STATE = IDLE;
}

void set_ssid()
{
    ESP_LOGI(TAG_Con, "Set SSID");
    //char ssid[buffer->size];
    char * ssid = new char[buffer->size];
    memcpy(ssid, buffer->data, buffer->size);

    if(ft.set_wifi_ssid(ssid, buffer->size))
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
    ESP_LOGI(TAG_Con, "Set password");
    char password[buffer->size];
    memcpy(password, buffer->data, buffer->size);

    if(ft.set_wifi_password(password, buffer->size))
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
    uint8_t n = ft.get_version(version);
    ble->write(version, n);
    CONNECTED_STATE = IDLE;
}

// TODO
void perform_ota()
{
    ESP_LOGI(TAG_Con, "Performing OTA update.");
    config.OTA_request = true;

    if(config.wifi_synch_semaphore != NULL)
	{
		unsigned long start_timer = esp_timer_get_time()/1000000;
		for(;;)
		{
			//! Add a global #define to Configuration.h
			if((esp_timer_get_time()/1000000) - start_timer > 9)
			{
				ESP_LOGI(TAG_Con, "Waited too long for lock.");
				ble->write("n\n");
                CONNECTED_STATE = IDLE;
                return;
			}
			if(xSemaphoreTake(config.wifi_synch_semaphore, 300) == pdTRUE){
				ESP_LOGI(TAG_Con, "Acquired wifi_synch_semaphore.");
				break;
			}
			vTaskDelay(80 / portTICK_PERIOD_MS);
		}
	}

    uint8_t result = perform_OTA();
    if(result == 0) { // Success
        ble->write("s\n");
    } else if(result == 1) { // No internet
        ble->write("n\n");
    } else { // Other
        ble->write("e\n");
    }
    config.OTA_request = false;

    if(config.wifi_synch_semaphore != NULL)
    {
        xSemaphoreGive(config.wifi_synch_semaphore);
    }
    // Write result to BLE
    ESP_LOGI(TAG_Con, "Released WiFi Synch semaphore.");
    CONNECTED_STATE = IDLE;
}

void synch_data()
{

    ESP_LOGI(TAG_Con, "Synching data");
    bool done = false;
    uint16_t num_unsynched_trainings = ft.get_number_of_unsynched_trainings();

    uint8_t data[UPLOAD_BLOCK_SIZE_BLE];
    ble->write(std::to_string(num_unsynched_trainings));

    for(int i=0;i<num_unsynched_trainings;i++)
    {
        // Call again for each training
        ft.start_reading_data();
        
        ble->write("\nTraining number ");
        ble->write(std::to_string(i));
        ble->write("\n");


        int32_t response = 0;
        do {
            response = ft.get_next_buffer_of_training(data);
            if(response == -1) // data is full
            {
                ble->write(data, UPLOAD_BLOCK_SIZE_BLE);
            }

        } while(response == -1);
         // This was the last chunk
        ESP_LOGI(TAG_Con, "Response is %d", response);
        ble->write(data, response);
        ble->write(EOF); // Write EOF

        unsigned long response_timer = millis();
        while (millis() - response_timer < 10000)
        {
            if(config.synched_training != AWAITING)
                break;
            vTaskDelay(300 / portTICK_PERIOD_MS);
        }
        
        if(config.synched_training == SYNCH_COMPLETE)
        {
            ft.completed_synch_of_training(true);
            ESP_LOGI(TAG_Con, "Successful synch.");
        } else if(config.synched_training == SYNCH_INCOMPLETE)
        {
            ft.completed_synch_of_training(false);
            ESP_LOGE(TAG_Con, "Unsuccessful synch.");
            CONNECTED_STATE = IDLE; 
            return;
        } else {
            ESP_LOGE(TAG_Con, "Timeout waiting for response.");
            CONNECTED_STATE = IDLE; 
            return;
        }
        config.synched_training = AWAITING; // Reset the variable for the next trainings.
            
    }
    ble->write(EOF);
    CONNECTED_STATE = IDLE;
}