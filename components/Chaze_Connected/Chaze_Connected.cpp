#include "Chaze_Connected.h"

const char * TAG_Con = "Chaze-Connected";

// TODO Set name when advertising
// TODO check if we are connected to wifi before synching data

class ConnectedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        available = true;
        uint8_t size = rxValue.length();
        buffer->size = size;
        
        for (int i = 0; i < size; i++)
        {
            ESP_LOGI(TAG_Con, "%c", rxValue[i]);
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
                } else { //default
                    ESP_LOGE(TAG_Con, "Command %c not recognized. Reset the state to IDLE.", buffer->data[0]);
                    CONNECTED_STATE = IDLE;
                }
            }

        } else // Size is bigger than 1
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
                default:
                {
                    CONNECTED_STATE = IDLE;
                    break;
                }
            }
            vTaskDelay(100);
        }

        ESP_LOGI(TAG_Con, "\n");
      }
    }
};

void connected()
{
    CONNECTED_STATE = IDLE;

    ble.pRxCharacteristic->setCallbacks(new ConnectedCallbacks());

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
            default:
            {
                ESP_LOGE(TAG_Con, "Should not get here.");
                break;
            }
        }

        // Constantly check if we received smth. from BLE and then react accordingly.
        vTaskDelay(500 / portTICK_PERIOD_MS);

    }
    config.STATE = ADVERTISING;

}

void send_battery()
{
    uint8_t battery_percentage = config.get_battery_level();
    ble.write(battery_percentage);
    CONNECTED_STATE = IDLE;
}

void set_name()
{
    char name[buffer->size];
    memcpy(name, buffer->data, buffer->size);

    if(ft.set_name(name, buffer->size))
    {
        ble.write("1");
    } else{
        ble.write("0");
    }
    ESP_LOGI(TAG_Con, "Set the name to %s", name);
    CONNECTED_STATE = IDLE;
}

void set_ssid()
{
    char ssid[buffer->size];
    memcpy(ssid, buffer->data, buffer->size);

    if(ft.set_wifi_ssid(ssid, buffer->size))
    {
        ble.write("1");
    } else {
        ble.write("0");
    }
}

void set_password()
{
    char password[buffer->size];
    memcpy(password, buffer->data, buffer->size);

    if(ft.set_wifi_password(password, buffer->size))
    {
        ble.write("1");
    } else {
        ble.write("0");
    }
    CONNECTED_STATE = IDLE;
}

void get_version()
{
    char version[128];
    uint8_t n = ft.get_version(version);
    ble.write(version, n);
    CONNECTED_STATE = IDLE;
}

// TODO
void perform_ota()
{
    ESP_LOGI(TAG_Con, "Performing OTA update.");
    ble.write("s");
    CONNECTED_STATE = IDLE;
}

void synch_data()
{
    bool done = false;

    uint16_t num_unsynched_trainings = ft.get_number_of_unsynched_trainings();

    uint8_t data[UPLOAD_BLOCK_SIZE_BLE];
    ble.write(num_unsynched_trainings);

    for(int i=0;i<num_unsynched_trainings;i++)
    {
        // Call again for each training
        ft.start_reading_data();

        available = false;
        int32_t response = ft.get_next_buffer_of_training(data);

        if(response == -1) // data is full
        {
            ble.write(data, UPLOAD_BLOCK_SIZE_BLE);
        } else { // This was the last chunk
            ble.write(data, response);
            ble.write(EOF); // Write EOF

            // Wait for response
            unsigned long t1 = millis();
            bool response;
            while(!available)
            {
                // Safety net against infinity loop
                if(millis() - t1 > 20000) // Wait 20s for an answer.
                {
                    ESP_LOGE(TAG_Con, "Waited too long for BLE response.");
                    response = false;
                    break;
                }
                vTaskDelay(1000);
            }
            
            response = strncmp(buffer->data, "1", 1);

            ft.completed_synch_of_training(response);

        }
    }
    ble.write(EOF);
    CONNECTED_STATE = IDLE;
}