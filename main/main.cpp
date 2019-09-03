#include "Chaze_Record.h"
#include "Chaze_Advertising.h"

const char * TAG_MAIN = "Chaze-Main";

extern "C" void app_main()
{
	config.turn_on_main_circuit();
	config.initialize_vib();
	config.initialize_leds();

	vTaskDelay(2000 / portTICK_PERIOD_MS);
	//! Change to ADVERTISING
	config.STATE = RECORD;

	while(1)
	{
		//! please_call_every_loop here?
		switch(config.STATE) {
		case DEEPSLEEP:
			{
				ESP_LOGI(TAG_MAIN, "Entering deep sleep now.");
				config.vibration_signal_sleep();
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