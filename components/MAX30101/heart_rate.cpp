#include <stdio.h>
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "heart_rate.h"


/**
 * @brief Empty constructor for heart rate class.
 */
HeartRate::HeartRate(void) {}

/**
 * @brief Constructor for heart rate class. Default settings apply when initializing this way.
 * @param sample_time Length of the sampling window in seconds.
 */
HeartRate::HeartRate(uint8_t sample_time)
{
	this->sampling_time = sample_time;

	maxim_config_t ex1 = {.PA1 = DEFAULT_PA, .PA2 = PA_0_0, .PA3 = PA_0_0, .PA4 = PA_0_0,
								  .MODE = DEFAULT_MODE, .SMP_AVE = DEFAULT_SMP_AVE, .FIFO_ROLL = FIFO_ROLL_EN,
								  .SPO2_ADC_RGE = DEFAULT_ADC, .SPO2_SR = DEFAULT_SR, .LED_PW = DEFAULT_PW};

	max30101 = MAX30101(&ex1);
	this->init_structs();
}

/**
 * @brief Second, more customized constructor.
 * @param sample_time Length of the sampling window in seconds.
 * @param ex1 maxim_config_t pointer used for custom initialisation.
 */
HeartRate::HeartRate(uint8_t sample_time, maxim_config_t * ex1){
	this->sampling_time = sample_time;
	max30101 = MAX30101(ex1);
	this->init_structs();
}

void HeartRate::shutdown()
{
	max30101.shutdown();
}

void HeartRate::resume()
{
	max30101.resume();
	max30101.setMODE_CONFIG(DEFAULT_MODE);
}

void HeartRate::reset()
{
	max30101.reset();
}

/**
 * @brief Initialize structs for filtering.
 */
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

uint8_t HeartRate::get_id(void)
{
	return max30101.readID();
}

/**
 * @brief Measures the heart rate in beats per minute.
 * @return Returns heart rate measure over window define by sample time.
 */
int32_t HeartRate::get_heart_rate(void)
{
	int32_t freq = max30101.get_sampling_freq();
	if(DEBUG) ESP_LOGI(TAG, "Frquency is: %d\n", freq);
	int32_t total = freq*this->sampling_time;
	int32_t * raw_data = this->get_heart_rate_raw();

	for(int i=0;i<total/2;i++){
		printf("%d\n", raw_data[i]);
	}
	printf("\n");

	free(raw_data);

	return 0; //Return heart rate
}

/*
 * @brief Returns pointer to int32_t array holding raw heart rate data. Size: sampling_time/2 due to DC correction.
*/
int32_t * HeartRate::get_heart_rate_raw()
{
	int32_t freq = max30101.get_sampling_freq();
	if(DEBUG) ESP_LOGI(TAG, "Frquency is: %d\n", freq);
	int32_t total = freq*this->sampling_time;
	uint64_t t1,t2;
	int32_t * read_data =  (int32_t *) malloc(total*sizeof(int32_t));

	if(read_data == NULL){
		ESP_LOGE(TAG, "Could not allocate.");
		return 0;
	}

	uint32_t samplesTaken = 0;
	t1 = (uint64_t) esp_timer_get_time();

	while(samplesTaken < total)
	{
	this->max30101.check(); //Check the sensor, read up to 3 samples
		while (this->max30101.available()) //do we have new data?
		{
		  read_data[samplesTaken] = (int32_t) this->max30101.getFIFORed();
		  this->max30101.nextSample(); //We're finished with this sample so move to next sample
		  samplesTaken++;
		  if(samplesTaken >= total) break;
		}
	}

	t2 = (uint64_t) esp_timer_get_time();
	if(DEBUG) ESP_LOGI(TAG, "samples[%d] Hz [%.02f]\n", samplesTaken, (float)samplesTaken / ((t2 - t1) / 1000000.0));

	filter_low_mem(read_data, total, read_data);

	/*int32_t * out =  (int32_t *) malloc((total/2)*sizeof(int32_t));
	for(int i=total/2;i<total;i++){
		out[i-(total/2)] = read_data[i];
	}*/

	//free(read_data);
	return read_data;
}

/**
 * @brief Applies a DC-removal filter, mean difference filter and a low pass filter with cutoff frequency of 4Hz (roughly 250BpM).
 * @param data_in Pointer to array holding data to be filtered.
 * @param n Length of the data to be filtered.
 * @param data_out Pointer to array of the filtered data. If data_in == data_out, memory is saved and the
 * filtering is done in-place.
 */
void HeartRate::filter(int32_t * data_in, int32_t n, int32_t * data_out)
{
	for(int i=0;i<n;i++){
		this->dcFilter = this->dcRemoval( (float)data_in[i], this->dcFilter.w, ALPHA);

		float meanDiffResIR = this->get_meanDiff( dcFilter.result, &meanDiff);
		this->lowPassButterworthFilter(meanDiffResIR, &lpbFilter);

		data_out[i] = this->lpbFilter.result;
	}
}

void HeartRate::filter_low_mem(int32_t * data_in, int32_t n, int32_t * data_out)
{
	for(int i=0;i<n;i++){
		this->dcFilter = this->dcRemoval( (float)data_in[i], this->dcFilter.w, ALPHA);

		float meanDiffResIR = this->get_meanDiff( dcFilter.result, &meanDiff);
		this->lowPassButterworthFilter(meanDiffResIR, &lpbFilter);
		if(i >= n/2)
		{
			data_out[i-n/2] = this->lpbFilter.result;
		}
	}
}

/**
 * @brief Filter for removing the DC component of the signal.
 * @param x Input value that is currently being filtered.
 * @param prev_w Used for filtering.
 * @param alpha Fraction of "how much is being let through". Keep close to 1.0 (e.g. 0.95).
 * @return Struct of type dcFilter_t
 */
dcFilter_t HeartRate::dcRemoval(float x, float prev_w, float alpha)
{
  dcFilter_t filtered;
  filtered.w = x + alpha * prev_w;
  filtered.result = filtered.w - prev_w;

  return filtered;
}

/**
 * @brief Applies mean difference filter.
 * @param M To be filtered value.
 * @param filterValues Current values used by the filter.
 * @return
 */
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

/**
 * @brief Performs low pass filter. Options generated by http://www.schwietering.com/jayduino/filtuino/
 * @param x Value to be filtered.
 * @param filterResult Value filtered.
 */
void HeartRate::lowPassButterworthFilter(float x, butterworthFilter_t * filterResult)
{
  filterResult->v[0] = filterResult->v[1];

  //Fs = 100Hz and Fc = 10Hz
  filterResult->v[1] = (2.452372752527856026e-1 * x) + (0.50952544949442879485 * filterResult->v[0]);

  //Fs = 100Hz and Fc = 4Hz
  //filterResult->v[1] = (1.367287359973195227e-1 * x) + (0.72654252800536101020 * filterResult->v[0]); //Very precise butterworth filter

  filterResult->result = filterResult->v[0] + filterResult->v[1];
}


/*
//Working main.cpp file

#include "heart_rate.h"
#include "Configuration.h"

extern "C" void app_main()
{

	//IMPORTANT: Check Table 11 and 12 if SR vs. PW is ok. @ https://datasheets.maximintegrated.com/en/ds/MAX30101.pdf
	HeartRate hr(1);

	while(1){
		hr.get_heart_rate();
		while(1){
			vTaskDelay(10000 / portTICK_PERIOD_MS);
		}

	}
}
 */
