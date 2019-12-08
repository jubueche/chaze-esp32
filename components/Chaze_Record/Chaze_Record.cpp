#include "Chaze_Record.h"


const char * TAG_RECORD = "Chaze Record";

#define DEBUG_RECORDING

// TODO Emergency training stop when eg buffer can not be written anymore
// TODO Enable no motion again
//! Check exits (quick return statements and free objects there)

void sample_hr(void * pvParams)
{
	uint32_t heart_rate;
	unsigned long sample_time;
	uint8_t bytes[9] = {};
	char const * name = "HR";
	for(;;){

		// Sample heart rate
		//! Uncomment if heart rate sensor attached, use config.I2C methods for thread safety
		//uint32_t heart_rate = hr.get_heart_rate();
		heart_rate = 170;
		//if(DEBUG) ESP_LOGI(TAG_RECORD, "Heart rate sampled.");
		vTaskDelay(20);
		// Get time of sample
		sample_time = millis() - base_time;
		// Populate uint8_t array.
		config.populate_heart_rate(bytes, heart_rate, sample_time);
		aquire_lock_and_write_to_buffer(bytes, sizeof(bytes), name);
	}
	
}


void sample_pressure(void * pvParams)
{
	float value;
	unsigned long sample_time;
	uint8_t bytes[9] = {};
	char const * name = "Pressure";
	for(;;){
		pressure.read_vals();
		value = pressure.pressure();
		// Get time of sample
		sample_time = millis() - base_time;
		if(DEBUG) ESP_LOGI(TAG_RECORD, "Pressure: Time: %lu  Pressure: %f", sample_time, value);
		config.populate_pressure(bytes, value, sample_time, false);
		aquire_lock_and_write_to_buffer(bytes, sizeof(bytes), name);
	}
}

void sample_pressure_backside(void * pvParams)
{
	float value;
	unsigned long sample_time;
	uint8_t bytes[9] = {};
	char const * name = "Pressure Back";
	for(;;){
		pressure_backside.read_vals();
		value = pressure_backside.pressure();
		// Get time of sample
		sample_time = millis() - base_time;
		if(DEBUG) ESP_LOGI(TAG_RECORD, "Pressure Backside: Time: %lu  Pressure: %f", sample_time, value);
		config.populate_pressure(bytes, value, sample_time,true);
		aquire_lock_and_write_to_buffer(bytes, sizeof(bytes), name);
	}
}


void sample_bno(void * pvParams)
{
	unsigned long sample_time;
	imu::Quaternion quat;
	imu::Vector<3> acc;
	uint8_t bytes[33];
	float values[7];
	double values_d[7];
	char const * name = "BNO055";
	for(;;){
		sample_time = millis() - base_time;
		quat = bno.getQuat();
		acc = bno.getVector(BNO055::VECTOR_LINEARACCEL);
		if(DEBUG) ESP_LOGI(TAG_RECORD, "Acc is %.3f", acc.x());
		values_d[0] = acc.x(); values_d[1]=acc.y();values_d[2]=acc.z();values_d[3]=quat.w();values_d[4]=quat.y();values_d[5]=quat.x();values_d[6]=quat.z(); 
		for(int i=0;i<7;i++){
			values[i] = (float) values_d[i];
		}
		if(DEBUG) ESP_LOGI(TAG_RECORD, "BNO: Time: %lu  AccX: %f, AccY: %f, AccZ: %f, QuatW: %f, QuatY: %f, QuatX: %f, QuatZ: %f, ", sample_time, values[0], values[1], values[2], values[3], values[4], values[5], values[6]);
		config.populate_bno(bytes, values, sample_time);
		aquire_lock_and_write_to_buffer(bytes, sizeof(bytes), name);
	}
}

/* This task is spawned when record is called and deleted when the RECORD state is exited.
   It constantly checks if either of the buffers is full. If that is the case, it calls compress_and_write,
   passing this buffer as an argument.
   This task ensures that even if one buffer is full, the sensors can keep logging and the data is still
   written to the flash. Main assumption: It takes longer to sample a complete buffer of size 8kB than 
   compressing and writing it to flash.
*/
void check_buffer_and_write_to_flash_task(void * pvParams)
{
	buffer_t * buf1 = buffers[0];
	buffer_t * buf2 = buffers[1];
	FlashtrainingWrapper_t *ft = (FlashtrainingWrapper_t *) pvParams;
	for (;;)
	{
		if(buf1->counter == BUFFER_SIZE)
		{
			if(xSemaphoreTake(config.compress_and_save_semaphore, portMAX_DELAY) == pdTRUE)
			{
				if(DEBUG) ESP_LOGI(TAG_RECORD, "Buffer 0 is full. Call compress and save.");
				compress_and_save(ft, 0, buffers); // Compress buffer 1 (index 0 in buffers)
				buf1->counter = 0; //Reset the buffer
				xSemaphoreGive(config.compress_and_save_semaphore);
			}
		}
		if(buf2->counter == BUFFER_SIZE)
		{
			if(xSemaphoreTake(config.compress_and_save_semaphore, portMAX_DELAY) == pdTRUE)
			{
				if(DEBUG) ESP_LOGI(TAG_RECORD, "Buffer 1 is full. Call compress and save.");
				compress_and_save(ft, 1, buffers); // Compress buffer 1 (index 0 in buffers)
				buf2->counter = 0; //Reset the buffer
				xSemaphoreGive(config.compress_and_save_semaphore);
			}
		}
		vTaskDelay(20 / portTICK_PERIOD_MS); // Wait half a second to check again.

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
		ESP_LOGE(TAG_RECORD, "Error creating training on flash.");
		// TODO Close training or smth.?
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
			ESP_LOGE(TAG_RECORD, "Error initializing pressure.");
			config.STATE = DEEPSLEEP;
			return;
		}
	} else {
		ESP_LOGE(TAG_RECORD, "Error initializing BNO055");
		config.STATE = DEEPSLEEP;
		return;
	}

	// Attach interrupts
	config.attach_btn_int(&gpio_interrupt_handler_task, &record_gpio_isr_handler);
	config.attach_bno_int(&gpio_interrupt_handler_task, &record_gpio_isr_handler);

	//Enable no-motion interrupts on BNO
	/*bno.enableSlowNoMotion(NO_MOTION_THRESHOLD, NO_MOTION_DURATION, NO_MOTION);
 	bno.enableInterruptsOnXYZ(ENABLE, ENABLE, ENABLE);
  	bno.setExtCrystalUse(true);*/

	// Give the binary semaphore for writing to the buffer
	xSemaphoreGive(config.write_buffer_semaphore);

	bool timeout_before = false;

	TaskHandle_t hr_task_handle = NULL;
	TaskHandle_t bno_task_handle = NULL;
	TaskHandle_t pressure_task_handle = NULL;
	TaskHandle_t pressure_backside_task_handle = NULL;
	TaskHandle_t check_buffer_and_write_to_flash_task_task_handle = NULL;

	// Start all tasks. Heart rate has the highest priority followed by pressure and BNO.
	//! HIGH WATER MARK!
	bool success = (bool) xTaskCreate(&sample_hr, "sample_hr", 1024 * 7, NULL, 10, &hr_task_handle);
	success = success && (bool) xTaskCreate(&sample_bno, "sample_bno", 1024 * 8, NULL, 10, &bno_task_handle);
	success = success && (bool) xTaskCreate(&sample_pressure, "sample_pressure", 1024 * 7, NULL, 10, &pressure_task_handle);
	success = success && (bool) xTaskCreate(&sample_pressure_backside, "sample_pressure_backside", 1024 * 7, NULL, 10, &pressure_backside_task_handle);
	success = success && (bool) xTaskCreate(&check_buffer_and_write_to_flash_task, "check_and_compress", 1024*6, ft, 10, &check_buffer_and_write_to_flash_task_task_handle);
	
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
			clean_up(hr_task_handle, bno_task_handle, pressure_task_handle, pressure_backside_task_handle, check_buffer_and_write_to_flash_task_task_handle, ft);
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
					clean_up(hr_task_handle, bno_task_handle, pressure_task_handle, pressure_backside_task_handle, check_buffer_and_write_to_flash_task_task_handle, ft);
					return;
				}
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

void clean_up(TaskHandle_t hr_task_handle, TaskHandle_t bno_task_handle, TaskHandle_t pressure_task_handle, TaskHandle_t pressure_backside_task_handle,
						TaskHandle_t check_buffer_and_write_to_flash_task_task_handle, FlashtrainingWrapper_t *ft)
{
	if(DEBUG) ESP_LOGI(TAG_RECORD, "Called clean_up");
	config.STATE = DEEPSLEEP;
	gpio_set_level(GPIO_VIB, 1);
	vTaskDelay(80 / portTICK_PERIOD_MS);
	gpio_set_level(GPIO_VIB, 0);
	
	config.detach_bno_int();
	config.detach_btn_int();

	if(xSemaphoreTake(config.write_buffer_semaphore, portMAX_DELAY) == pdTRUE)
	{
		// Delete the tasks
		vTaskDelete(hr_task_handle);
		ESP_LOGI(TAG_RECORD, "Deleted HR");
		vTaskDelete(bno_task_handle);
		ESP_LOGI(TAG_RECORD, "Deleted BNO");
		vTaskDelete(pressure_task_handle);
		ESP_LOGI(TAG_RECORD, "Deleted Pressure");
		vTaskDelete(pressure_backside_task_handle);
		ESP_LOGI(TAG_RECORD, "Deleted Pressure back");
		xSemaphoreGive(config.write_buffer_semaphore);
	}

	if(xSemaphoreTake(config.compress_and_save_semaphore, portMAX_DELAY) == pdTRUE)
	{
		vTaskDelete(check_buffer_and_write_to_flash_task_task_handle);
		ESP_LOGI(TAG_RECORD, "Deleted Check buffer");
		xSemaphoreGive(config.compress_and_save_semaphore);
	}
	
	// Write last buffer and stop training
	buffer_t * curr_buff = buffers[buff_idx];
	if(DEBUG) ESP_LOGI(TAG_RECORD, "Filling up %d many bytes", BUFFER_SIZE-curr_buff->counter);
	for(int i=curr_buff->counter;i<BUFFER_SIZE;i++)
		curr_buff->data[i] = 'f';
	curr_buff->counter += BUFFER_SIZE-curr_buff->counter;
	if(DEBUG) ESP_LOGI(TAG_RECORD, "Curr buffer counter is %d", curr_buff->counter);
	if(DEBUG) ESP_LOGI(TAG_RECORD, "Buffers pointer is %p", buffers);
	
	compress_and_save(ft, buff_idx, buffers);
	curr_buff->counter = 0;
	FlashtrainingWrapper_add_unsynched_training(ft);

	// Stop training
	if(FlashtrainingWrapper_stop_training(ft))
	{
		config.flicker_led(GPIO_LED_GREEN);
	} else {
		ESP_LOGE(TAG_RECORD, "Failed stopping training.");
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
	pressure_backside.~MS5837();
	bno.~BNO055();
	hr.~HeartRate();

	if(DEBUG) ESP_LOGI(TAG_RECORD, "Cleaned up");
	config.STATE = DEEPSLEEP;

	vTaskDelay(500 /portTICK_PERIOD_MS);
}


void setup_hr()
{
	hr = HeartRate(1);
}

esp_err_t setup_pressure()
{
	pressure = MS5837();
	pressure_backside = MS5837();
	if(!pressure.init(I2C_NUM_0))
	{
		config.STATE = DEEPSLEEP;
		ESP_LOGE(TAG_RECORD, "Error initializing pressure.");
		return ESP_FAIL;
	}
	if(!pressure_backside.init(I2C_NUM_1))
	{
		config.STATE = DEEPSLEEP;
		ESP_LOGE(TAG_RECORD, "Error initializing pressure on the back.");
		return ESP_FAIL;
	}

	pressure.setModel(MS5837::MS5837_02BA);
	pressure.setFluidDensity(997);
	pressure_backside.setModel(MS5837::MS5837_02BA);
	pressure_backside.setFluidDensity(997);
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


void aquire_lock_and_write_to_buffer(uint8_t * bytes, uint8_t length, char const * name)
{
	if(xSemaphoreTake(config.write_buffer_semaphore, (TickType_t) 500) == pdTRUE)
	{
		buffer_t * curr_buff = buffers[buff_idx];
		if(DEBUG) ESP_LOGI(TAG_RECORD, "%s acquired the lock", name);
		if(BUFFER_SIZE-curr_buff->counter >= length){
			if(DEBUG) ESP_LOGI(TAG_RECORD, "Space left: %d", BUFFER_SIZE-curr_buff->counter);
			if(DEBUG)  ESP_LOGI(TAG_RECORD, "Enough space to write");
			//Enough space, we can write
			memcpy(curr_buff->data+(curr_buff->counter), bytes, length);
			curr_buff->counter += length;
		} else{
			//Fill up the buffer with 'f' and set the buff_idx to buff_idx XOR 1
			if(DEBUG)  ESP_LOGI(TAG_RECORD, "Buffer %d almost full, fill up", buff_idx);
			for(int i=curr_buff->counter;i<BUFFER_SIZE-curr_buff->counter;i++)
				curr_buff->data[i] = 'f';
			curr_buff->counter += BUFFER_SIZE-curr_buff->counter;
			if(DEBUG)  ESP_LOGI(TAG_RECORD, "Counter is %d", curr_buff->counter);

			if(buff_idx==0)
				buff_idx=1;
			else buff_idx=0;
		}

		if(DEBUG) ESP_LOGI(TAG_RECORD, "Release mutex");
		//Release mutex
		xSemaphoreGive(config.write_buffer_semaphore);

	}
}