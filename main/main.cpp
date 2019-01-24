#include "MAX30101.h"
#include "Configuration.h"

extern "C" void app_main()
{

	MAX30101 max30101(I2C_NUM_1);
	uint32_t red[10];


	//max30101.initHR(SMP_AVE_2, FIFO_ROLL_EN, FIFO_A_FULL_3, INT_A_FULL_EN, INT_PPG_RDY_EN, INT_ALC_OVF_EN, INT_DIE_TEMP_RDY_EN);

	/*IMPORTANT: Check Table 11 and 12 if SR vs. PW is ok. @ https://datasheets.maximintegrated.com/en/ds/MAX30101.pdf */

	maxim_config_t ex1 = {.PA1 = PA_0_2, .PA2 = PA_0_2, .PA3 = PA_0_2, .PA4 = PA_0_2,
							  .SMP_AVE = SMP_AVE_NO, .FIFO_ROLL = FIFO_ROLL_DIS, .FIFO_A_FULL = FIFO_A_FULL_0,
							  .MODE = HEART_RATE_MODE, .SPO2_ADC_RGE = ADC_2048, .SPO2_SR = SR_50, .LED_PW = PW_69};


	//SAMPLE 10 converted points (uint32_t)
	max30101.init(&ex1);
	uint8_t n = 10;
	uint8_t num_channels = 1; // If mode  = HEART RATE it is 1, for SPO2 its 2, for Multi LED its 3
	uint32_t data[num_channels * n];

	while(1){
		max30101.read_n(data, num_channels * n);
		for(int i=0;i<num_channels*n;i++){
			printf("Int: %d\n", data[i]);
		}
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}

}
