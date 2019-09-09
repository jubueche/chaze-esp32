#ifndef Chaze_ble_h_
#define Chaze_ble_h_

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>

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

    // Attributes
    BLEServer *pServer;
    BLECharacteristic * pTxCharacteristic;
    BLECharacteristic * pRxCharacteristic;
    uint8_t txValue;
    
    //Functions
    void initialize_connection();
    void write(uint8_t);
    void write(std::string);
    void write(uint8_t*,size_t);
    void write(char*,size_t);
    void advertise(void);
private:
    const char * TAG = "Chaze-Ble";
};

#endif
