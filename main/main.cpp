#include "Configuration.h"
#include "Chaze_Record.h"
#include "Chaze_Advertising.h"

const char * TAG = "Chaze-Main";

extern "C" void app_main()
{
	config.turn_on_main_circuit();
	vTaskDelay(2000 / portTICK_PERIOD_MS);
	//! Change to ADVERTISING
	config.STATE = ADVERTISING;

	while(1)
	{
		//! please_call_every_loop here?
		switch(config.STATE) {
		case DEEPSLEEP:
			{
				ESP_LOGI(TAG, "Entering deep sleep now.");
				config.vibration_signal_sleep();
				config.turn_off_main_circuit();
				vTaskDelay(3000 / portTICK_PERIOD_MS); //Wait for capacitor to discharge.
				break;
			}
		case ADVERTISING:
			{
				ESP_LOGI(TAG, "Advertising...");
				advertise();
				
				break;
			}
		case RECORD:
			{
				printf("Recoring...\n");
				break;
			}
		case CONNECTED:
			{
				printf("Recoring...\n");
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