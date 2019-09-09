#include "Chaze_Record.h"


const char * TAG_RECORD = "Chaze Record";

// TODO Emergency training stop when eg buffer can not be written anymore
//! Check exits (quick return statements and free objects there)

void sample_hr(void * pvParams)
{
	FlashtrainingWrapper_t *ft = (FlashtrainingWrapper_t *) pvParams; 
	
	for(;;){

		//Try to aquire the mutex for a given amount of time then back-off
		buffer_t * curr_buff = buffers[buff_idx];

		if(xSemaphoreTakeRecursive(config.i2c_semaphore, 20) == pdPASS)
		{
			// Sample heart rate
			//! Uncomment if heart rate sensor attached, use config.I2C methods for thread safety
			//uint32_t heart_rate = hr.get_heart_rate();
			uint32_t heart_rate = 170;
			//ESP_LOGI(TAG_RECORD, "Heart rate sampled.");
			vTaskDelay(20);

			// Get time of sample
			unsigned long sample_time = millis() - base_time;

			// Populate uint8_t array.
			uint8_t bytes[9] = {};
			config.populate_heart_rate(bytes, heart_rate, sample_time);
			char const * name = "HR";
			aquire_lock_and_write_to_buffer(curr_buff, bytes, ft, sizeof(bytes), name);

			xSemaphoreGiveRecursive(config.i2c_semaphore);
		}
		vTaskDelay(config.random_between(5,50));
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
			unsigned long sample_time = millis() - base_time;
			if(DEBUG) ESP_LOGI(TAG_RECORD, "Pressure: Time: %lu  Pressure: %f", sample_time, value);
			uint8_t bytes[9] = {};
			config.populate_pressure(bytes, value, sample_time);
			char const * name = "Pressure";
			aquire_lock_and_write_to_buffer(curr_buff, bytes, ft, sizeof(bytes), name);

			xSemaphoreGiveRecursive(config.i2c_semaphore);
		}
		vTaskDelay(config.random_between(5,50));
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
			unsigned long sample_time = millis() - base_time;
			imu::Quaternion quat = bno.getQuat();
			imu::Vector<3> acc = bno.getVector(BNO055::VECTOR_LINEARACCEL);
			double values_d[7] = {acc.x(),acc.y(),acc.z(),quat.w(),quat.y(),quat.x(),quat.z()};
			float values[7] = {};
			for(int i=0;i<7;i++){
				values[i] = (float) values_d[i];
			}
			if(DEBUG) ESP_LOGI(TAG_RECORD, "BNO: Time: %lu  AccX: %f, AccY: %f, AccZ: %f, QuatW: %f, QuatY: %f, QuatX: %f, QuatZ: %f, ", sample_time, values[0], values[1], values[2], values[3], values[4], values[5], values[6]);
			uint8_t bytes[33] = {};
			config.populate_bno(bytes, values, sample_time);

			char const * name = "BNO055";
			aquire_lock_and_write_to_buffer(curr_buff, bytes, ft, sizeof(bytes), name);

			xSemaphoreGiveRecursive(config.i2c_semaphore);
		}
		vTaskDelay(config.random_between(5,50));
	}
}

// Interrupts
void IRAM_ATTR record_gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(config.gpio_evt_queue, &gpio_num, NULL);
}

void gpio_interrupt_handler_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(config.gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if(io_num == GPIO_BNO_INT_NUM){
				nm_interrupt = true;
			} else{
				if(io_num == GPIO_BUTTON_NUM){
					rising_interrupt = true;
				}
			}
        }
    }
}

void record()
{
	nm_interrupt = false;
	rising_interrupt = false;
	base_time = millis();
	unsigned long red_led_timer = millis();

	// Create wrapper struct for Flashtraining
	FlashtrainingWrapper_t *ft = FlashtrainingWrapper_create();
	// Start a new training
	if(!FlashtrainingWrapper_start_new_training(ft))
	{
		ESP_LOGE(TAG_RECORD, "Error creating training.");
		config.flicker_led(GPIO_LED_RED);
		config.STATE = DEEPSLEEP;
		return;
	} else {
		config.flicker_led(GPIO_LED_GREEN);
	}

	buffers = (buffer_t **) malloc(2*sizeof(buffer_t *));
	buffer_t * buffer0;
	buffer_t * buffer1;
	//Initialize buffers 1 and 2
	if(buffers != NULL)
	{
		buffer0 = (buffer_t *) malloc(sizeof(buffer_t));
		buffer1 = (buffer_t *) malloc(sizeof(buffer_t));

		if(buffer0 != NULL && buffer1 != NULL)
		{
			buffer0->data = (uint8_t *) malloc(BUFFER_SIZE);
			buffer0->counter = 0;
			buffer1->data = (uint8_t *) malloc(BUFFER_SIZE);
			buffer1->counter = 0;

			buffers[0] = buffer0;
			buffers[1] = buffer1;

			if(buffer0->data == NULL || buffer1->data == NULL){
				ESP_LOGE(TAG_RECORD, "Error allocating heap space for buffers->data");
				config.STATE = DEEPSLEEP;
				return;
			}
		} else {
			ESP_LOGE(TAG_RECORD, "Error allocating heap space for buffer0 or buffer1.");
			config.STATE = DEEPSLEEP;
			return;
		}
	} else {
		ESP_LOGE(TAG_RECORD, "Error allocating heap space for buffers");
		config.STATE = DEEPSLEEP;
		return;
	}

	// Initialize all sensors (+ Calibration)
	if(setup_bno(ft) == ESP_OK){
		if(setup_pressure() == ESP_OK){
			//setup_hr();
			if(DEBUG) ESP_LOGI(TAG_RECORD, "Setup complete.");
		} else {
			return;
		}
	} else {
		return;
	}

	// Attach interrupts
	config.attach_btn_int(&gpio_interrupt_handler_task, &record_gpio_isr_handler);
	config.attach_bno_int(&gpio_interrupt_handler_task, &record_gpio_isr_handler);

	//Enable no-motion interrupts on BNO
	bno.enableSlowNoMotion(NO_MOTION_THRESHOLD, NO_MOTION_DURATION, NO_MOTION);
 	bno.enableInterruptsOnXYZ(ENABLE, ENABLE, ENABLE);
  	bno.setExtCrystalUse(true);
	
	bool timeout_before = false;


	TaskHandle_t hr_task_handle = NULL;
	TaskHandle_t bno_task_handle = NULL;
	TaskHandle_t pressure_task_handle = NULL;

	// Start all tasks. Heart rate has the highest priority followed by pressure and BNO.
	bool success = (bool) xTaskCreate(&sample_hr, "sample_hr", 1024 * 8, ft, 10, &hr_task_handle);
	success = success && (bool) xTaskCreate(&sample_bno, "sample_bno", 1024 * 8, ft, 10, &bno_task_handle);
	success = success && (bool) xTaskCreate(&sample_pressure, "sample_pressure", 1024 * 8, ft, 10, &pressure_task_handle);
	if(!success){
		ESP_LOGE(TAG_RECORD, "Failed to create tasks. Going to sleep.");
		config.STATE = DEEPSLEEP;
		return;
	}


	// Loop and check if the state is still record.
	while(config.STATE == RECORD)
	{
		if(nm_interrupt){
			if(DEBUG) ESP_LOGI(TAG_RECORD, "No motion interrupt");
			bno.resetInterrupts();
			nm_interrupt = false;

			clean_up(hr_task_handle, bno_task_handle, pressure_task_handle, ft);
			return;
		}

		if(rising_interrupt){
			if(DEBUG) ESP_LOGI(TAG_RECORD, "Interrupt");
			rising_interrupt = false;
			long timer = millis();

			while(gpio_get_level(GPIO_BUTTON))
			{
				if(DEBUG) ESP_LOGI(TAG_RECORD, "Waiting for release...");
				if (millis() - timer > TIMEOUT_BUTTON_PUSHED_TO_ADVERTISING && !timeout_before)
				{
					timeout_before = true;
					clean_up(hr_task_handle, bno_task_handle, pressure_task_handle, ft);
					return;
				}
			}
			if(timeout_before)
			{
				gpio_set_level(GPIO_LED_RED, 0);
			}
		}

		if(millis() - red_led_timer > LED_RED_TIMEOUT)
		{
			red_led_timer = millis();
			gpio_set_level(GPIO_LED_RED, 1);
			vTaskDelay(80 / portTICK_PERIOD_MS);
			gpio_set_level(GPIO_LED_RED, 0);
		}

		vTaskDelay(100 /portTICK_PERIOD_MS);
	}

	//! Absolute timeout where the swimmer is swimming for more than 6h

}

void clean_up(TaskHandle_t hr_task_handle, TaskHandle_t bno_task_handle, TaskHandle_t pressure_task_handle, FlashtrainingWrapper_t *ft)
{
	config.STATE = DEEPSLEEP;
	gpio_set_level(GPIO_VIB, 1);
	vTaskDelay(80 / portTICK_PERIOD_MS);
	gpio_set_level(GPIO_VIB, 0);
	
	config.detach_bno_int();
	config.detach_btn_int();

	// Delete the tasks
	vTaskDelete(hr_task_handle);
	vTaskDelete(bno_task_handle);
	vTaskDelete(pressure_task_handle);

	// Write last buffer and stop training
	buffer_t * curr_buff = buffers[buff_idx];
	if(DEBUG) ESP_LOGI(TAG_RECORD, "Filling up %d many bytes", BUFFER_SIZE-curr_buff->counter);
	for(int i=curr_buff->counter;i<BUFFER_SIZE/*-curr_buff->counter*/;i++)
		curr_buff->data[i] = 'f';
	curr_buff->counter += BUFFER_SIZE-curr_buff->counter;
	if(DEBUG) ESP_LOGI(TAG_RECORD, "Curr buffer counter is %d", curr_buff->counter);
	if(DEBUG) ESP_LOGI(TAG_RECORD, "Buffers pointer is %p", buffers);
	
	compress_and_save(ft, buff_idx, buffers);
	curr_buff->counter = 0;

	// Stop training
	if(FlashtrainingWrapper_stop_training(ft))
	{
		config.flicker_led(GPIO_LED_GREEN);
	} else {
		config.flicker_led(GPIO_LED_RED);
	}
	
	if(buffers != NULL)
	{
		// Free buffers 0 and 1 if they are not null.
		if(buffers[0] != NULL) {
			// Free buffer 1 if not NULL
			if(buffers[1] != NULL) {
				// Free buffer0.data and buffer1.data if they are not NULL
				if(buffers[1]->data != NULL){
					// Free buffer0->data
					if(buffers[0]->data != NULL){
						free(buffers[0]->data);
					}
					free(buffers[1]->data);
				}
				free(buffers[1]);
			}
			free(buffers[0]);
		}
		free(buffers);
	}
	pressure.~MS5837();
	bno.~BNO055();
	hr.~HeartRate();

	if(DEBUG) ESP_LOGI(TAG_RECORD, "Cleaned up");


	vTaskDelay(500 /portTICK_PERIOD_MS);
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
		if(DEBUG) ESP_LOGI(TAG_RECORD, "%s acquired the lock", name);
		if(BUFFER_SIZE-curr_buff->counter >= length){
			if(DEBUG) ESP_LOGI(TAG_RECORD, "Space left: %d", BUFFER_SIZE-curr_buff->counter);
			if(DEBUG)  ESP_LOGI(TAG_RECORD, "Enough space to write");
			//Enough space, we can write
			memcpy(curr_buff->data+(curr_buff->counter), bytes, length);
			curr_buff->counter += length;
		} else{
			//Fill up the buffer with 'f' and set the buff_idx to buff_idx XOR 1
			if(DEBUG)  ESP_LOGI(TAG_RECORD, "Buffer almost full, fill up");
			for(int i=curr_buff->counter;i<BUFFER_SIZE-curr_buff->counter;i++)
				curr_buff->data[i] = 'f';
			curr_buff->counter += BUFFER_SIZE-curr_buff->counter;
			if(DEBUG)  ESP_LOGI(TAG_RECORD, "Counter is %d", curr_buff->counter);

			if(buff_idx==0)
				buff_idx=1;
			else buff_idx=0;

			if(DEBUG)  ESP_LOGI(TAG_RECORD, "Call compress");
			//Call compress. Switch the bit back since we want to compress the buffer that is full
			if(ft == NULL){
				if(DEBUG)  ESP_LOGE(TAG_RECORD, "FlashtrainingWrapper is NULL");
				xSemaphoreGiveRecursive(config.i2c_semaphore);
			} else {
				if(DEBUG) printf("State is %d\n", FlashtrainingWrapper_get_STATE(ft));
				compress_and_save(ft, !buff_idx, buffers);
				curr_buff->counter = 0; //Reset the compressed buffer
			}
		}

		if(DEBUG) ESP_LOGI(TAG_RECORD, "Release mutex");
		//Release mutex
		xSemaphoreGiveRecursive(config.i2c_semaphore);

	}
}