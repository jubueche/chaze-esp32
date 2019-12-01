# Chaze ESP32 Hardware code

## Bluetooth connection

- **Battery:** The app should be able to query the battery level of the device. ```Input: b``` followed by ```Output: 0-100``` which is the percentage.

- **Name:** User should be able to change the advertised name of the device. The name should not be too long and should not contain any characters apart from numbers and upper/lower case letters. Command to request a change of name: ```Input: n``` followed by the name, followed by ```-1```. Returns ```1``` if successful and ```0``` otherwise. Returns to menu.

- **WiFi Configuration:** The user should be able to configure the WiFi-SSID and the password. The SSID should have a reasonable maximum length. Command: ```Input: w``` followed by the ```SSID```, ```Output successful ? 1 : 0```, followed by the ```password```, ```Output successful ? 1 : 0```. Returns to menu.

- **Get version:** The App should be able to query the current firmware version of the device to be able to offer a firmware upgrade to the user. Command: ```Input: v```. ```Output: v1.3``` for example. Return to menu.

- **Execute OTA-Update:** The device should be able to perform an OTA-update to update the running firmware on the device. Command: ```Input: f```, ```Output internet connection available ? s (success) :  n (no internet)```. If internet is not available, the device will remain in the connected state. The device will be disconnected if there is internet available, due to memory constraints imposed by TLS. If a new firmware exists, the device will reset, regardless if the new update was successful or not. If no new firmware is available, the device will return to Advertising mode, since the BLE object needs to be re-initialized.

- **Synchronize trainings:** Synch all the trainings that are currently in the memory that have not been synched. Command: ```Input: s```. ```Output: Number of trainings to be synched.``` followed by ```512 byte packages```, followed by ```EOF```. After a single training has been synched, wait for the phone to send an indicator that it received the training so we can mark it as synched. Indicator: success: "1" else: "0". We then repeat this for the remaining trainings. At the end, the device sends one final ```EOF``` indicating that all trainings have been (tried) synched. After that, the device starts the deletion process and goes into deep sleep after it is done deleting. The deletion is postponed if the user chooses an immediate action after the synching process. In general, deletion should always happen before going into deep sleep mode.

- [Not so important for now] **Alarms:** Alarms are used to signal the swimmer when she/he is swimming too slow, too inefficient or with too much/less power.

## Flashtraining library further requirements

- **Get number of unsynched trainings:** Upon call to ```uint32_t get_number_of_unsynched_trainings(void)``` should return the number of unsynched trainings. This function is already implemented and always returns 2. TODO: Implement correctly.

- **Set number of unsynched trainings:** Upon call to ```void set_number_of_unsynched_trainings(uint16_t)``` should set the new number of unsynched trainings to the passed value. This function is called after a new training was recorded or a new training was successfully synched via BLE or WiFi.

- **New training:** Upon call to ```start_new_training()``` write meta-data into flash. Furthermore, switch states and check if enough space is available. Return ```ESP_OK``` on success or an error of type ```esp_err_t```.

- **No lost training data:** After writing a block of a certain size, update the training object/struct to hold the current write index in case the device dies (then the data is not lost).


- **Store training meta data in memory:** Upon creating a new training, store the meta data in memory and update regularly during recording.

- **Get training data:** Function that takes a pointer to a 512 byte buffer and fills the buffer perfectly. Returns ```int32_t```. If there is more to come, returns -1, else: the number of bytes written.  Signature: ```int32_t get_training_data(uint8_t *buf)```.

- **Write compressed chunk:** Function that receives a pointer to an array that is holding a variable amount of data. Second argument is the amount of data ```n```. Returns ```ESP_OK``` if successfully written to memory or any error otherwise. Signature: ```esp_err_t write_compressed_chunk(uint8_t *buf, uint32_t n)```.

- **Store device meta data:** Functions in ```Configuration.cpp``` that make it possible to retrieve the following device meta data:
    - Device ID
    - Azure Connection String
    - WiFi SSID
    - WiFi password
One example would be ```char * get_device_id(void)```, which will be used like this: ```char * d_id = config.get_device_id();```. This function will internally make a call to a function in the flash library, which returns the deviceID.

- **Functions:**
```
    int32_t get_next_buffer_of_training(uint8_t *); // Takes pointer to buffer. Returns -1 if wrote 512 bytes else the number of bytes written.
    void completed_synch_of_training(bool); // Passes a boolean indicating whether the training was successfully synched.
    uint32_t get_device_id(void); //Returns device ID
    const char * get_azure_connection_string(void); // Returns connection string for Azure.
    const char * get_wifi_ssid(void);
    const char * get_wifi_password(void);
    bool set_device_id(uint32_t); //Sets device ID
    bool set_azure_connection_string(const char *); // Sets connection string for Azure.
    bool set_wifi_ssid(char *, uint8_t ); // Second param is the size.
    bool set_wifi_password(char *, uint8_t);
    uint8_t get_version(char *);
    bool set_version(char *, uint8_t);
```

## TODO

- Add watchdog
- Add documentation for record.

## Bugs when setting up

1) When compiling the arduino core for ESP32, the pragma on line 238 in BLERemoteService.cpp is ignored. To fix it, move the line ```#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"``` to the top of the file. Change ignore to ```warning```.

2) Use 2018 version of the xtensa compiler (```xtensa-esp32-elf-gcc8_2_0-esp32-2018r1-linux-amd64```) and the version 3.3 of the ESP-IDF. See [here](https://www.esp32.com/viewtopic.php?t=7400)

3) Fix bug in ```esp-idf/components/newlib/include/ctype.h``` where we replace ```#define __ctype_lookup(__c) ((__ctype_ptr__+sizeof(""[__c]))[(int)(__c)])``` with ```#define __ctype_lookup(__c) ((__ctype_ptr__+sizeof(""[(int)__c]))[(int)(__c)])```

4) Delete AzureIoT from Arduino libraries and Camera library.

5) Fix I2C bug, which can be found [here](https://github.com/espressif/esp-idf/issues/680) (see arsinios answer)

6) Not a bug, but avoids redefinition warning: Uncomment ```#define DEC(x) C2(DEC,x)``` in ```esp-azure/azure-iot-sdk-c/c-utility/inc/azure_c_shared_utility/macro_utils.h```.

## Pre setup

- Call ```make erase_flash``` to erase the nvs flash.