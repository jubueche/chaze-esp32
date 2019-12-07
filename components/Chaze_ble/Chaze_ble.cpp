#include "Chaze_ble.h"


const char * TAG_BLE = "Chaze-BLE";

class MyServerCallbacks: public BLEServerCallbacks {
  
  void onConnect(BLEServer* pServer) {
    config.ble_connected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    config.ble_connected = false;
  }
};

MyServerCallbacks * callbacks = NULL;
BLE2902 * ble_2902 = NULL;

Chaze_ble::Chaze_ble(void){

  // Try to obtain the wifi_synch_semaphore
  for(;;)
  {
    if(xSemaphoreTake(config.wifi_synch_semaphore, portMAX_DELAY) == pdTRUE)
    {
        if(DEBUG) ESP_LOGI(TAG_BLE, "Took the WiFi semaphore. WiFi-Synch task terminated.");
        break;
    }
    vTaskDelay(80);
  }
  if(DEBUG) ESP_LOGI(TAG_BLE, "Obtained WiFi synch semaphore in BLE constructor.");

  pServer = NULL;
  pTxCharacteristic = NULL;
  txValue = 0;
}

Chaze_ble::~Chaze_ble(void){
  
  BLEDevice::deinit();

  pServer->removeService(pService);

  free(pServer);
  free(pService);
  
  pTxCharacteristic->~BLECharacteristic();
  pRxCharacteristic->~BLECharacteristic();

  free(pTxCharacteristic);
  free(pRxCharacteristic);

  free(ble_2902);
  ble_2902 = NULL;

  //Release the wifi_synch_semaphore
  xSemaphoreGive(config.wifi_synch_semaphore);
  // vTaskDelay(1000);
  if(DEBUG) ESP_LOGI(TAG_BLE, "Released wifi_synch_semaphore after BLE has been deallocated.");
}

bool Chaze_ble::is_initialized()
{
  return BLEDevice::getInitialized();
}


void Chaze_ble::initialize_connection(){

  // Create the BLE Device
  if(!BLEDevice::getInitialized())
  {
    if(DEBUG) ESP_LOGI(TAG_BLE, "Not yet initialized, start init.");
    char* name = global_ft->get_name();
    if(DEBUG) ESP_LOGI(TAG_BLE, "Setting device name to %s", name);
    std::string name_s = name;
    BLEDevice::init(name_s);
    free(name);
  }
  
  //Set the MTU of the packets sent, maximum is 500, Apple needs 23 apparently.
  BLEDevice::setMTU(185);
  config.MTU_BLE = 182;

  // Create the BLE Server
  pServer = BLEDevice::createServer(); 

  if(DEBUG) ESP_LOGI(TAG_BLE, "Created server");
  if(callbacks == NULL)
  {
    callbacks = new MyServerCallbacks();
  }
  pServer->setCallbacks(callbacks);
  // Create the BLE Service
  pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
  if(ble_2902 == NULL)
  {
    ble_2902 = new BLE2902();
  }

  pTxCharacteristic->addDescriptor(ble_2902);

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
}