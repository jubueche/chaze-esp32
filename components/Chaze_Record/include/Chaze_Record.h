#ifndef __CHAZE_RECORD_H__
#define __CHAZE_RECORD_H__

#include "Configuration.h"
#include "esp_log.h"
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
static volatile uint8_t buff_idx = 0;

static volatile bool nm_interrupt;
static volatile bool rising_interrupt;
static unsigned long base_time;

void setup_hr(void);
void clean_up(TaskHandle_t, TaskHandle_t, TaskHandle_t, FlashtrainingWrapper_t *);
esp_err_t setup_bno(FlashtrainingWrapper_t *ft);
esp_err_t setup_pressure(void);

void aquire_lock_and_write_to_buffer(buffer_t *, uint8_t *, FlashtrainingWrapper_t *, uint8_t, char const *);

//Tasks
void sample_hr(void *);
void sample_bno(void *);
void sample_pressure(void *);

void record(void);



#endif