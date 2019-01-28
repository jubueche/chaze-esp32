#include "heart_rate.h"
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

	/*IMPORTANT: Check Table 11 and 12 if SR vs. PW is ok. @ https://datasheets.maximintegrated.com/en/ds/MAX30101.pdf */
	HeartRate hr(10);
	uint32_t bpm;

	while(1){
		bpm = hr.get_heart_rate();
		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}
}

