#ifndef _WDT_H
#define _WDT_H

#include "esp_err.h"
#include "stdbool.h"

esp_err_t start_wdt(uint32_t, bool);
esp_err_t stop_wdt(void);
esp_err_t feed_wdt(void);
esp_err_t get_wdt_status(void);

#endif
