#ifndef CHAZE_ADVERTISING_H
#define CHAZE_ADVERTISING_H

#include "BNO055.h"
#include "Chaze_ble.h"
#include "nvs_flash.h"
#include "wifi_synch.h"
#include "ChazeFlashtraining.h"

extern Chaze_ble * ble;

void advertise(void);
void attach_am_interrupt(BNO055 *);
void clean_up(BNO055 *);

#endif