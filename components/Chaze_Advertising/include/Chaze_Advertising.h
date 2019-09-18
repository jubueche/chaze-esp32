#ifndef CHAZE_ADVERTISING_H
#define CHAZE_ADVERTISING_H

#include "BNO055.h"
#include "Chaze_ble.h"
#include "nvs_flash.h"
#include "wifi_synch.h"
#include "ChazeFlashtraining.h"

static BNO055 bno_adv;
static bool am_interrupt;
extern Chaze_ble * ble;
static TaskHandle_t wifi_synch_task_handle = NULL;

void advertise(void);
void attach_am_interrupt(void);
void clean_up(void);

#endif