#include "Chaze_Record.h"
#include "Chaze_Advertising.h"
#include "Chaze_Connected.h"

const char * TAG_MAIN = "Chaze-Main";

extern "C" void app_main()
{
	config.turn_on_main_circuit();

	for(int i=0;i<10;i++)
	{
		if (!rtc.begin()){
			ESP_LOGE(TAG_MAIN, "Error in rtc.begin");
		} else {
			ESP_LOGI(TAG_MAIN, "RTC initialized");
			break;
		}
	}

	config.initialize_vib();
	config.initialize_leds();
	gpio_set_level(GPIO_LED_GREEN, 1);
	vTaskDelay(200 / portTICK_PERIOD_MS);
	gpio_set_level(GPIO_LED_GREEN, 0);

	//! Change to ADVERTISING
	config.STATE = ADVERTISING;

	while(1)
	{
		switch(config.STATE) {
		case DEEPSLEEP:
			{
				ESP_LOGI(TAG_MAIN, "Entering deep sleep now.");
				config.vibration_signal_sleep();

				// Delete all trainings that should be deleted
				global_ft->erase_trainings_to_erase();
				ESP_LOGI(TAG_MAIN, "Start erasing trainings...");
				while(global_ft->get_STATE() == 4)
				{
					ESP_LOGI(TAG_MAIN, "State is 4");
					vTaskDelay(100 / portTICK_PERIOD_MS);
				}

				config.turn_off_main_circuit();
				vTaskDelay(3000 / portTICK_PERIOD_MS); //Wait for capacitor to discharge.
				break;
			}
		case ADVERTISING:
			{
				ESP_LOGI(TAG_MAIN, "Advertising...");
				advertise();
				break;
			}
		case RECORD:
			{
				ESP_LOGI(TAG_MAIN, "Recording...");
				record();
				break;
			}
		case CONNECTED:
			{
				ESP_LOGI(TAG_MAIN, "Connected...");
				connected();
				break;
			}
		default:
			{
				config.STATE = DEEPSLEEP;
				break;
			}
		}
	}

}