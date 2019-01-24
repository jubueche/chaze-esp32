#include "MAX30101.h"
#include "Configuration.h"
#include "time.h"



extern "C" void app_main()
{


	/* TODO:
	 * - Implement read_hr()
	 * - Implement read_spo2()
	 * - Implement temperature compensation -> Change read_n_* functions to record the temp when triggering interrupt.
	 * - Safe error handling
	 */
	MAX30101 max30101(I2C_NUM_1);


	/*IMPORTANT: Check Table 11 and 12 if SR vs. PW is ok. @ https://datasheets.maximintegrated.com/en/ds/MAX30101.pdf */
	maxim_config_t ex1 = {.PA1 = PA_0_2, .PA2 = PA_0_2, .PA3 = PA_0_2, .PA4 = PA_0_2,
							  .MODE = HEART_RATE_MODE, .SMP_AVE = SMP_AVE_2, .FIFO_ROLL = FIFO_ROLL_DIS, .FIFO_A_FULL = FIFO_A_FULL_0,
							  .SPO2_ADC_RGE = ADC_16384, .SPO2_SR = SR_1000, .LED_PW = PW_411};

	max30101.init(&ex1);
	uint32_t red[32];

while(1){

	max30101.read_n(red, 32);

	for(int i=0;i<32;i++){
		printf("%d\n",red[i]);
	}
	//vTaskDelay(30);
}




}
