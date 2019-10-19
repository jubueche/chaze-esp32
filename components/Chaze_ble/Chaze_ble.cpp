#include "Chaze_ble.h"

const char * TAG_BLE = "Chaze-BLE";

Chaze_ble::Chaze_ble(void){
    pServer = NULL;
    pTxCharacteristic = NULL;
    txValue = 0;
}

Chaze_ble::~Chaze_ble(void){
  BLEDevice::deinit(true);
  free(pServer);
  free(pService);
  pTxCharacteristic->~BLECharacteristic();
  pRxCharacteristic->~BLECharacteristic();
}

class MyServerCallbacks: public BLEServerCallbacks {
  
  void onConnect(BLEServer* pServer) {
    config.ble_connected = true;
    config.ble_old_device_connected = true;

  };

  void onDisconnect(BLEServer* pServer) {
    config.ble_connected = false;
  }
};


void Chaze_ble::initialize_connection(){

  // Create the BLE Device
  BLEDevice::init("UART Service");

  //Set the MTU of the packets sent, maximum is 500, Apple needs 23 apparently.
  BLEDevice::setMTU(185);
  config.MTU_BLE = 182;

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  // Create the BLE Service
  pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  pRxCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);

  // Start the service
  pService->start();

}

void Chaze_ble::write(uint8_t to_write){
    if (config.ble_connected){
      pTxCharacteristic->setValue(&to_write, 1);
      pTxCharacteristic->notify();
      vTaskDelay(STACK_CONGESTION_TIMEOUT / portTICK_PERIOD_MS); // bluetooth stack will go into congestion, if too many packets are sent
	  }
}

void Chaze_ble::write(std::string to_write){
    if (config.ble_connected){
        pTxCharacteristic->setValue(to_write);
        pTxCharacteristic->notify();
        vTaskDelay(STACK_CONGESTION_TIMEOUT / portTICK_PERIOD_MS);
    }
}

void Chaze_ble::write(uint8_t* data,size_t length){
    if (config.ble_connected){

        uint16_t rest = length % config.MTU_BLE;
        uint16_t num_iters = length / config.MTU_BLE;

        // Fragmentation
        for(int i=0;i<num_iters;i++)
        {
          pTxCharacteristic->setValue(data+i*config.MTU_BLE, config.MTU_BLE);
          pTxCharacteristic->notify();
          vTaskDelay(STACK_CONGESTION_TIMEOUT / portTICK_PERIOD_MS);

        }
        pTxCharacteristic->setValue(data+num_iters*config.MTU_BLE, rest);
        pTxCharacteristic->notify();
        vTaskDelay(STACK_CONGESTION_TIMEOUT / portTICK_PERIOD_MS);
    }
}

void Chaze_ble::write(char* data,size_t length){
    if (config.ble_connected){
        write((uint8_t *) data, length);
    }
}

void Chaze_ble::advertise(void){
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    ESP_LOGI(TAG_BLE, "Start advertising");
    config.ble_old_device_connected = config.ble_connected;
}