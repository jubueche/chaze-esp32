#ifndef CHAZE_CONNECTED_H
#define CHAZE_CONNECTED_H

#include "Chaze_Advertising.h"
#include "Chaze_OTA.h"

struct Rx_buffer {
    char data[128];
    uint8_t size;
};

enum {IDLE, BATTERY, NAME, NAME_RECEIVED, WIFI, WIFI_2, SSID_RECEIVED, PASS_RECEIVED, VERSION, OTA, DATA};
static uint8_t CONNECTED_STATE = IDLE;

static Rx_buffer *buffer;


void connected(void);
void send_battery(void);
void set_name(void);
void set_ssid(void);
void set_password(void);
void get_version(void);
void perform_ota(void);
void synch_data(void);

#endif