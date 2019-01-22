#include "MAX30101.h"
#include "Configuration.h"

extern "C" void app_main()
{

	MAX30101 max30101(I2C_NUM_1);
	//max30101.initHR(SMP_AVE_2, FIFO_ROLL_EN, FIFO_A_FULL_3, INT_A_FULL_EN, INT_PPG_RDY_EN, INT_ALC_OVF_EN, INT_DIE_TEMP_RDY_EN);

	/*IMPORTANT: Check Table 11 and 12 if SR vs. PW is ok. @ https://datasheets.maximintegrated.com/en/ds/MAX30101.pdf */

	multi_led_config_t ex1 = {.PA1 = PA_0_2, .PA2 = PA_0_2, .PA3 = PA_0_2, .PA4 = PA_0_2,
							  .SMP_AVE = SMP_AVE_NO, .FIFO_ROLL = FIFO_ROLL_EN, .FIFO_A_FULL = FIFO_A_FULL_3,
							  .SLOT1 = SLOT_RED, .SLOT2 = SLOT_IR, .SLOT3 = SLOT_GREEN, .SLOT4 = SLOT_NONE,
							  .SPO2_ADC_RGE = ADC_2048, .SPO2_SR = SR_50, .LED_PW = PW_69,
							  .A_FULL_EN = INT_A_FULL_EN, .PPG_RDY_EN = INT_PPG_RDY_EN, .ALC_OVF_EN = INT_ALC_OVF_EN,
							  .DIE_TEMP_RDY_EN = INT_DIE_TEMP_RDY_EN };

	max30101.init_multi_led(&ex1);
	uint8_t temp = 0;
	double frac = 0.0;
	uint16_t flag = 0;
	while(1){
		max30101.setTEMP_EN() ; /* trigger temperature read */
		temp = max30101.getTEMP_INT();
		frac = 10.0 / (double) max30101.getTEMP_FRAC();
		flag = max30101.getIntStatus();
		printf("%.2f, 0x%02X\n", (double) temp+frac, flag);
		vTaskDelay(5000 /portTICK_RATE_MS);
	}

}
