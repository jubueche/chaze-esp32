#ifndef HEART_RATE_H
#define HEART_RATE_H

#include "MAX30101.h"

#define MEAN_FILTER_SIZE	15
#define ALPHA 				0.95

#define DEFAULT_SR 		SR_200
#define DEFAULT_ADC 	ADC_16384
#define DEFAULT_PW		PW_411
#define DEFAULT_MODE 	HEART_RATE_MODE
#define DEFAULT_PA 		PA_51
#define DEFAULT_SMP_AVE SMP_AVE_2


struct dcFilter_t {
  float w;
  float result;
};

struct butterworthFilter_t
{
  float v[2];
  float result;
};

struct meanDiffFilter_t
{
  float values[MEAN_FILTER_SIZE];
  uint8_t index;
  float sum;
  uint8_t count;
};


class HeartRate {
public:
	HeartRate(uint8_t); //Constructor
	HeartRate(uint8_t, maxim_config_t *);
	HeartRate();

	uint8_t sampling_time; //in sec
	MAX30101 max30101;

	struct dcFilter_t dcFilter;
	struct butterworthFilter_t lpbFilter;
	struct meanDiffFilter_t meanDiff;

	void shutdown(void);
	void reset(void);
	void resume(void);
	void filter(int32_t *, int32_t, int32_t *);
	void filter_low_mem(int32_t *, int32_t, int32_t *);
	uint8_t get_id(void);
	void init_structs(void);
	dcFilter_t dcRemoval(float, float, float);
	float get_meanDiff(float, meanDiffFilter_t*);
	void lowPassButterworthFilter(float, butterworthFilter_t *);
	int32_t get_heart_rate(void);
	int32_t * get_heart_rate_raw(void);

private:
	const char * TAG = "Chaze-HeartRate";

};


#endif
