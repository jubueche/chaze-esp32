#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
//#include "nvs_flash.h"
#include "driver/gpio.h"

#include "Arduino.h"
#include "Chaze_ble.h"

Chaze_ble ble;
bool connected;
bool oldDeviceConnected;
uint8_t throughput[600];

void setup(){
  ble = Chaze_ble();
  ble.initialize_connection();
  connected = false;
  oldDeviceConnected = false;
  std::fill_n(throughput,600,1);
  Serial.println("Done initializing...");
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
  vTaskDelay(10);
}
