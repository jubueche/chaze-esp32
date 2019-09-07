#ifndef __CHAZE_ADVERTISING_H__
#define __CHAZE_ADVERTISING_H__

#include "BNO055.h"
#include "Chaze_ble.h"
#include "nvs_flash.h"
#include "wifi_synch.h"
#include "ChazeFlashtraining.h"

static BNO055 bno_adv;
static bool am_interrupt;
static Chaze_ble ble;
static TaskHandle_t wifi_synch_task_handle = NULL;

void advertise(void);
void attach_am_interrupt(void);
void clean_up(void);

#endif