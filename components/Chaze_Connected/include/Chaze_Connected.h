#ifndef __CHAZE_CONNECTED_H__
#define __CHAZE_CONNECTED_H__

#include "Chaze_Advertising.h"
#include "Configuration.h"
#include "Chaze_ble.h"

struct Rx_buffer {
    char data[128];
    uint8_t size;
};

enum {IDLE, BATTERY, NAME, NAME_RECEIVED, WIFI, WIFI_2, SSID_RECEIVED, PASS_RECEIVED, VERSION, OTA};
static uint8_t CONNECTED_STATE = IDLE;


static Rx_buffer *buffer;
static volatile bool available;

void connected(void);
void send_battery(void);
void set_name(void);
void set_ssid(void);
void set_password(void);
void get_version(void);
void perform_ota(void);
void synch_data(void);

#endif