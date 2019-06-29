#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C"{
#include "wifi_synch.h"
}

extern "C" void app_main()
{
	if (xTaskCreate(&synch_via_wifi, "synch_via_wifi", 1024 * 8, NULL, 5, NULL) != pdPASS )
	{
		printf("Synch task create failed.\n");
	}
	while(1){
		vTaskDelay(100);
	}
}
