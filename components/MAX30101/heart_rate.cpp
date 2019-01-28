#include <stdio.h>
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "heart_rate.h"

HeartRate::HeartRate(uint8_t sample_time)
{
	this->sampling_time = sample_time;

	//Initialise MAX30101 object
	maxim_config_t ex1 = {.PA1 = DEFAULT_PA, .PA2 = PA_0_0, .PA3 = PA_0_0, .PA4 = PA_0_0,
								  .MODE = DEFAULT_MODE, .SMP_AVE = DEFAULT_SMP_AVE, .FIFO_ROLL = FIFO_ROLL_EN,
								  .SPO2_ADC_RGE = DEFAULT_ADC, .SPO2_SR = DEFAULT_SR, .LED_PW = DEFAULT_PW};

	max30101 = MAX30101(&ex1);
	this->init_structs();
}

HeartRate::HeartRate(uint8_t sample_time, maxim_config_t * ex1){
	this->sampling_time = sample_time;
	max30101 = MAX30101(ex1);
	this->init_structs();
}

void HeartRate::init_structs(void)
{
	dcFilter.w = 0;
	dcFilter.result = 0;

	lpbFilter.v[0] = 0;
	lpbFilter.v[1] = 0;
	lpbFilter.result = 0;

	meanDiff.index = 0;
	meanDiff.sum = 0;
	meanDiff.count = 0;
}

int32_t HeartRate::get_heart_rate(void)
{
	uint64_t t1,t2;
	int32_t * read_data =  (int32_t *) malloc(100*this->sampling_time*sizeof(int32_t));

	if(read_data == NULL){
		printf("Could not allocate.");
	}

	uint32_t samplesTaken = 0;
	t1 = (uint64_t) esp_timer_get_time();

	while(samplesTaken < 100*this->sampling_time)
	{
	this->max30101.check(); //Check the sensor, read up to 3 samples
		while (this->max30101.available()) //do we have new data?
		{
		  read_data[samplesTaken] = (int32_t) this->max30101.getFIFORed();
		  this->max30101.nextSample(); //We're finished with this sample so move to next sample
		  samplesTaken++;
		  if(samplesTaken >= 100*this->sampling_time) break;
		}
	}

	t2 = (uint64_t) esp_timer_get_time();
	printf("samples[%d]\n Hz [%.02f]\n", samplesTaken, (float)samplesTaken / ((t2 - t1) / 1000000.0));

	for(int i=0;i<100*this->sampling_time;i++){
				printf("%d\n", read_data[i]);
	}

	filter(read_data, 100*this->sampling_time, read_data);



	return 0;
}

void HeartRate::filter(int32_t * data_in, int32_t n, int32_t * data_out)
{
	for(int i=0;i<n;i++){
		this->dcFilter = this->dcRemoval( (float)data_in[i], this->dcFilter.w, ALPHA);

		float meanDiffResIR = this->get_meanDiff( dcFilter.result, &meanDiff);
		this->lowPassButterworthFilter(meanDiffResIR/*-dcFilterIR.result*/, &lpbFilter);

		data_out[i] = this->lpbFilter.result;
	}
}

dcFilter_t HeartRate::dcRemoval(float x, float prev_w, float alpha)
{
  dcFilter_t filtered;
  filtered.w = x + alpha * prev_w;
  filtered.result = filtered.w - prev_w;

  return filtered;
}

float HeartRate::get_meanDiff(float M, meanDiffFilter_t* filterValues)
{
  float avg = 0;

  filterValues->sum -= filterValues->values[filterValues->index];
  filterValues->values[filterValues->index] = M;
  filterValues->sum += filterValues->values[filterValues->index];

  filterValues->index++;
  filterValues->index = filterValues->index % MEAN_FILTER_SIZE;

  if(filterValues->count < MEAN_FILTER_SIZE)
    filterValues->count++;

  avg = filterValues->sum / filterValues->count;
  return avg - M;
}

void HeartRate::lowPassButterworthFilter(float x, butterworthFilter_t * filterResult)
{
  filterResult->v[0] = filterResult->v[1];

  //Fs = 100Hz and Fc = 10Hz
  filterResult->v[1] = (2.452372752527856026e-1 * x) + (0.50952544949442879485 * filterResult->v[0]);

  //Fs = 100Hz and Fc = 4Hz
  //filterResult->v[1] = (1.367287359973195227e-1 * x) + (0.72654252800536101020 * filterResult->v[0]); //Very precise butterworth filter

  filterResult->result = filterResult->v[0] + filterResult->v[1];
}

