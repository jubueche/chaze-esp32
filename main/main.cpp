#include "MS5837.h"

extern "C" void app_main()
{
	MS5837 ms5837 = MS5837();
	ms5837.init();
	ms5837.setModel(MS5837::MS5837_02BA);
	ms5837.setFluidDensity(997);

	while(1){
		ms5837.read_vals();
		vTaskDelay(20 / portTICK_PERIOD_MS);
		printf("Pressure: %.2f", ms5837.pressure());
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

}
