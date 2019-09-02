#ifndef __CHAZE_ADVERTISING_H__
#define __CHAZE_ADVERTISING_H__

#include "BNO055.h"

static BNO055 bno_adv;
static bool am_interrupt;

void advertise(void);
void attach_am_interrupt(void);

#endif