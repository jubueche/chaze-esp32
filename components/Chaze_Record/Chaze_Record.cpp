#include "Chaze_Record.h"


const char * TAG_RECORD = "Chaze Record";

//! Check exits (quick return statements and free objects there)

void sample_hr(void * pvParams)
{
	FlashtrainingWrapper_t *ft = (FlashtrainingWrapper_t *) pvParams; 
	
	for(;;){

		//Try to aquire the mutex for a given amount of time then back-off
		buffer_t * curr_buff = buffers[buff_idx];

		
		// Sample heart rate
		//! Uncomment if heart rate sensor attached, need I2C mux?
		//uint32_t heart_rate = hr.get_heart_rate();
		uint32_t heart_rate = 170; // Simulate slow heart rate sampling
		vTaskDelay(8000 / portTICK_PERIOD_MS);
		ESP_LOGI(TAG_RECORD, "Heart rate sampled.");

		// Get time of sample
		unsigned long sample_time = millis();

		// Populate uint8_t array.
		uint8_t bytes[9] = {};
		config.populate_heart_rate(bytes, heart_rate, sample_time);
		char const * name = "HR";
		aquire_lock_and_write_to_buffer(curr_buff, bytes, ft, sizeof(bytes), name);

		vTaskDelay(100 / portTICK_PERIOD_MS); //Back off a little longer
	}
	
}


void sample_pressure(void * pvParams)
{
	FlashtrainingWrapper_t *ft = (FlashtrainingWrapper_t *) pvParams; 
	
	for(;;){

		//Try to aquire the mutex for a given amount of time then back-off
		buffer_t * curr_buff = buffers[buff_idx];

		if(xSemaphoreTakeRecursive(config.i2c_semaphore, 20) == pdPASS)
		{
			//! Refactor to pressure_obj
			pressure.read_vals();
			vTaskDelay(30 / portTICK_PERIOD_MS);
			float value = pressure.pressure();
			// Get time of sample
			unsigned long sample_time = millis();
			ESP_LOGI(TAG_RECORD, "Pressure: Time: %lu  Pressure: %f", sample_time, value);
			uint8_t bytes[9] = {};
			config.populate_pressure(bytes, value, sample_time);
			char const * name = "Pressure";
			aquire_lock_and_write_to_buffer(curr_buff, bytes, ft, sizeof(bytes), name);

			xSemaphoreGiveRecursive(config.i2c_semaphore);
		}
		
		//vTaskDelay(100 / portTICK_PERIOD_MS); //Back off a little longer
	}
}


void sample_bno(void * pvParams)
{
	FlashtrainingWrapper_t *ft = (FlashtrainingWrapper_t *) pvParams; 
	
	for(;;){

		//Try to aquire the mutex for a given amount of time then back-off
		buffer_t * curr_buff = buffers[buff_idx];

		if(xSemaphoreTakeRecursive(config.i2c_semaphore, 20) == pdPASS)
		{
			// Get time of sample
			unsigned long sample_time = millis();
			imu::Quaternion quat = bno.getQuat();
			imu::Vector<3> acc = bno.getVector(BNO055::VECTOR_LINEARACCEL);
			double values_d[7] = {acc.x(),acc.y(),acc.z(),quat.w(),quat.y(),quat.x(),quat.z()};
			float values[7] = {};
			for(int i=0;i<7;i++){
				values[i] = (float) values_d[i];
			}
			ESP_LOGI(TAG_RECORD, "BNO: Time: %lu  AccX: %f, AccY: %f, AccZ: %f, QuatW: %f, QuatY: %f, QuatX: %f, QuatZ: %f, ", sample_time, values[0], values[1], values[2], values[3], values[4], values[5], values[6]);
			uint8_t bytes[33] = {};
			config.populate_bno(bytes, values, sample_time);

			char const * name = "BNO055";
			aquire_lock_and_write_to_buffer(curr_buff, bytes, ft, sizeof(bytes), name);

			xSemaphoreGiveRecursive(config.i2c_semaphore);
		}
		//vTaskDelay(100 / portTICK_PERIOD_MS); //Back off a little longer
		
	}
}



void record()
{
	// Create wrapper struct for Flashtraining
	FlashtrainingWrapper_t *ft = FlashtrainingWrapper_create();
	// Start a new training
	if(!FlashtrainingWrapper_start_new_training(ft))
	{
		ESP_LOGE(TAG_RECORD, "Error creating training.");
		config.STATE = DEEPSLEEP;
		return;
	}

	//Initialize buffers 1 and 2
	buffer_t buffer0 = {};
	buffer_t buffer1 = {};

	buffer0.data = (uint8_t *) malloc(BUFFER_SIZE);
	buffer0.counter = 0;
	buffer1.data = (uint8_t *) malloc(BUFFER_SIZE);
	buffer1.counter = 0;

	buffers[0] = &buffer0;
	buffers[1] = &buffer1;

	// Initialize all sensors (+ Calibration)
	if(setup_bno(ft) == ESP_OK){
		if(setup_pressure() == ESP_OK){
			//setup_hr();
			ESP_LOGI(TAG_RECORD, "Setup complete.");
		} else {
			return;
		}
	} else {
		return;
	}
	TaskHandle_t hr_task_handle = NULL;
	TaskHandle_t bno_task_handle = NULL;
	TaskHandle_t pressure_task_handle = NULL;

	// Start all tasks. Heart rate has the highest priority followed by pressure and then BNO.
	//bool success = (bool) xTaskCreate(&sample_hr, "sample_hr", 1024 * 8, ft, 30, &hr_task_handle);
	bool success = (bool) xTaskCreate(&sample_bno, "sample_bno", 1024 * 8, ft, 10, &bno_task_handle);
	success = success && (bool) xTaskCreate(&sample_pressure, "sample_pressure", 1024 * 8, ft, 10, &pressure_task_handle);
	if(!success){
		ESP_LOGE(TAG_RECORD, "Failed to create tasks. Going to sleep.");
		config.STATE = DEEPSLEEP;
		return;
	}
	

	// Attach button interrupt

	// Loop and check if the state is still record.
	while(config.STATE == RECORD)
	{
		vTaskDelay(1000 /portTICK_PERIOD_MS);
	}

	// Free all resources

}

void setup_hr()
{
	hr = HeartRate(1);
}

esp_err_t setup_pressure()
{
	pressure = MS5837();
	if(!pressure.init())
	{
		config.STATE = DEEPSLEEP;
		ESP_LOGE(TAG_RECORD, "Error initializing pressure.");
		return ESP_FAIL;
	}
	pressure.setModel(MS5837::MS5837_02BA);
	pressure.setFluidDensity(997);
	return ESP_OK;

}


esp_err_t setup_bno(FlashtrainingWrapper_t * ft)
{
	bno = BNO055();
	if(!bno.begin()){
		config.STATE = DEEPSLEEP;
		ESP_LOGE(TAG_RECORD, "Error starting BNO055.");
		return ESP_FAIL;
	}

	// Get sensor calibration data
	uint8_t sensor_calibration[22];

	for(int i=SENSOR_CALIBRATION_OFFSET; i<SENSOR_CALIBRATION_OFFSET+22;i++){
		sensor_calibration[i-SENSOR_CALIBRATION_OFFSET] = FlashtrainingWrapper_readCalibration(ft, i);
	}

	bno.setSensorOffsets(sensor_calibration);
	return ESP_OK;
}


void aquire_lock_and_write_to_buffer(buffer_t * curr_buff, uint8_t * bytes, FlashtrainingWrapper_t *ft, uint8_t length, char const * name)
{
	if(xSemaphoreTakeRecursive(config.i2c_semaphore,(TickType_t) 100) == pdTRUE)
	{
		ESP_LOGI(TAG_RECORD, "%s acquired the lock", name);
		if(BUFFER_SIZE-curr_buff->counter >= length){
			ESP_LOGI(TAG_RECORD, "Enough space to write");
			//Enough space, we can write
			memcpy(curr_buff->data+(curr_buff->counter), bytes, length);
			curr_buff->counter += length;
		} else{
			//Fill up the buffer with 'f' and set the buff_idx to buff_idx XOR 1
			ESP_LOGI(TAG_RECORD, "Buffer almost full, fill up");
			for(int i=curr_buff->counter;i<BUFFER_SIZE-curr_buff->counter;i++)
				curr_buff->data[i] = 'f';
			curr_buff->counter += BUFFER_SIZE-curr_buff->counter;
			ESP_LOGI(TAG_RECORD, "Counter is %d", curr_buff->counter);

			if(buff_idx==0)
				buff_idx=1;
			else buff_idx=0;

			ESP_LOGI(TAG_RECORD, "Call compress");
			//Call compress. Switch the bit back since we want to compress the buffer that is full
			if(ft == NULL){
				ESP_LOGE(TAG_RECORD, "FlashtrainingWrapper is NULL");
				xSemaphoreGiveRecursive(config.i2c_semaphore);
			} else {
				printf("State is %d\n", FlashtrainingWrapper_get_STATE(ft));
				compress_and_save(ft, !buff_idx);
				curr_buff->counter = 0; //Reset the compressed buffer
			}
		}

		ESP_LOGI(TAG_RECORD, "Release mutex");
		//Release mutex
		xSemaphoreGiveRecursive(config.i2c_semaphore);

	}
}