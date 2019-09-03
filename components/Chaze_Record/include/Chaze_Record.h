#ifndef __CHAZE_RECORD_H__
#define __CHAZE_RECORD_H__

#include "Configuration.h"
#include "esp_log.h"
#include "compression.h"
#include "heart_rate.h"
#include "BNO055.h"
#include "MAX30101.h"
#include "MS5837.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdlib.h"
#include "stdio.h"

extern "C" {
#include "compression.h"
}

#include "ChazeFlashtrainingWrapper.h"
#include "freertos/semphr.h"
#include "esp_log.h"

// Callbacks for button and no-motion interrupt
//...

// Boolean variables for callbacks etc.
//...

static MS5837 pressure;
static HeartRate hr;
static BNO055 bno;
static SemaphoreHandle_t sensor_semaphore = NULL;
static volatile uint8_t buff_idx = 0;

void setup_hr(void);
esp_err_t setup_bno(FlashtrainingWrapper_t *ft);
esp_err_t setup_pressure(void);

void aquire_lock_and_write_to_buffer(buffer_t *, uint8_t *, FlashtrainingWrapper_t *, uint8_t);

//Tasks
void sample_hr(void *);
void sample_bno(void *);
void sample_pressure(void *);

void record(void);



#endif