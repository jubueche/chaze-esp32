#include "Chaze_ble.h"


Chaze_ble::Chaze_ble(void){
    pServer = NULL;
    pTxCharacteristic = NULL;
    txValue = 0;
}

extern bool connected;
extern bool oldDeviceConnected;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      connected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      connected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        Serial.println();
        Serial.println("*********");
      }
    }
};


void Chaze_ble::initialize_connection(){

  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("UART Service");

  //Set the MTU of the packets sent, maximum is 500, Apple needs 23 apparently.
  BLEDevice::setMTU(500);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void Chaze_ble::write(uint8_t to_write){
    if (connected){
      pTxCharacteristic->setValue(&to_write, 1);
      pTxCharacteristic->notify();
      delay(STACK_CONGESTION_TIMEOUT); // bluetooth stack will go into congestion, if too many packets are sent
	  }
}

void Chaze_ble::write(std::string to_write){
    if (connected){
        pTxCharacteristic->setValue(to_write);
        pTxCharacteristic->notify();
        delay(STACK_CONGESTION_TIMEOUT);
    }
}

void Chaze_ble::write(uint8_t* data,size_t length){
    if (connected){
        pTxCharacteristic->setValue(data, length);
        pTxCharacteristic->notify();
        delay(STACK_CONGESTION_TIMEOUT);
    }
}

void Chaze_ble::advertise(void){
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("Start advertising");
    oldDeviceConnected = connected;
}


//Working example: main.cpp
/*
//This program measures the throughput of the BLE module. Achieves ~34s/1 MB
#include "Chaze_ble.h"

Chaze_ble ble;
bool connected;
bool oldDeviceConnected;
uint8_t throughput[600];

void setup(){
  ble = Chaze_ble();  
  ble.initialize_connection();
  Serial.println("Done initializing...");
  connected = false;
  oldDeviceConnected = false;
  std::fill_n(throughput,600,1);
}

void loop() {
  if(connected){
    delay(10000);
    Serial.println("Start");
    time_t start = time(0);
    for (int i=0;i<854;i++){ //~512 kByte
      ble.write(throughput, sizeof(throughput)); 

      // disconnecting
      if (!connected && oldDeviceConnected) {
          ble.advertise();
      }
      // connecting
      if (connected && !oldDeviceConnected) {
      // do stuff here on connecting
          oldDeviceConnected = connected;
      }
    }
    time_t end = time(0);
    double expired = difftime(end,start);
    Serial.println(expired);
  }
}
*/
