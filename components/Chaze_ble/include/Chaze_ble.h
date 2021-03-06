#ifndef CHAZE_BLE_H
#define CHAZE_BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>
#include "ChazeFlashtraining.h"
#include "ChazeMeta.h"

#include "Arduino.h"
#include "Configuration.h"

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define STACK_CONGESTION_TIMEOUT 20

class Chaze_ble {
public:
    //Constructor
    Chaze_ble();
    ~Chaze_ble();

    // Attributes
    
    BLEServer *pServer = NULL;
    BLECharacteristic * pTxCharacteristic;
    BLECharacteristic * pRxCharacteristic;
    BLEService *pService;
    uint8_t txValue;
    
    //Functions
    void initialize_connection();
    void write(uint8_t);
    void write(std::string);
    void write(uint8_t*,size_t);
    void write(char*,size_t);
    void advertise(void);
    bool is_initialized(void);
};

#endif
