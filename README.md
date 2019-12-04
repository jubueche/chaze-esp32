# Chaze ESP32 Hardware code

## Bluetooth connection

- **Battery:** The app should be able to query the battery level of the device. ```Input: b``` followed by ```Output: 0-100``` which is the percentage.

- **Name:** User should be able to change the **advertised name** of the device. The name should not be too long and should not contain any characters apart from numbers and upper/lower case letters. Command to request a change of name: ```Input: n``` followed by the name. Returns ```1``` if successful and ```0``` otherwise. Returns to menu. The length of the name has to be, including the
null terminator between 1 and 128, including 128, characters long.

- **WiFi Configuration:** The user should be able to configure the WiFi-SSID and the password. The SSID has a maximum length of 128 bytes. The password has a maximum of 512 bytes, including the termination character. Command: ```Input: w``` followed by the ```SSID```, ```Output successful ? 1 : 0```, followed by the ```password```, ```Output successful ? 1 : 0```. Returns to menu.

- **Get version:** The App should be able to query the current firmware version of the device to be able to offer a firmware upgrade to the user. Command: ```Input: v```. ```Output: v1.3``` for example. Return to menu.

- **Set version:** The App can set the version. The version should have the following format: ```vX.X```. This allows for 89 different
versions, when starting at ```v1.0```. In theory, the immediate version is hardcoded in the hardware code, thus making this function
needless. It is still included for practicality. ```Input: q``` followed by the version e.g. ```v1.2```.  ```Output successful ? 1 : 0```.

- **Execute OTA-Update:** The device should be able to perform an OTA-update to update the running firmware on the device. Command: ```Input: f```, ```Output internet connection available ? s (success) :  n (no internet)```. If internet is not available, the device will remain in the connected state. The device will be disconnected if there is internet available, due to memory constraints imposed by TLS. If a new firmware exists, the device will reset, regardless if the new update was successful or not. If no new firmware is available, the device will return to Advertising mode, since the BLE object needs to be re-initialized.

- **Synchronize trainings:** Synch all the trainings that are currently in the memory that have not been synched. Command: ```Input: d```. ```Output: Number of trainings to be synched.``` followed by ```512 byte packages```, followed by ```EOF```. After a single training has been synched, wait for the phone to send an indicator that it received the training so we can mark it as synched. Indicator: success: "1" else: "0". We then repeat this for the remaining trainings. At the end, the device sends one final ```EOF``` indicating that all trainings have been (tried) synched. After that, the device starts the deletion process and goes into deep sleep after it is done deleting. The deletion is postponed if the user chooses an immediate action after the synching process. In general, deletion should always happen before going into deep sleep mode.

- **Send connection string:** The connection string for every device can be synched via the app. ```Input: c``` followed by the connection string, e.g. ```HostName=XXX.net;DeviceId=XXX;SharedAccessKey=XXX```. ```Output successful ? 1 : 0```.

- **Set device name:** Set the device name of the device. This will be the ID that is used for the container where the data is uploaded to. ```Input: q``` followed by the device name, with max. 128 bytes.

- **Set container name:** Set the name of the subfolder for the data to be uploaded in. If the devices name is for example set to "chaze-3" and the container to "28188", then the data will be stored as ```chaze-3/28188/XXX.txt```. ```Input: x``` followed by the name with max. 128 characters. ```Output successful ? 1 : 0```.

- [Not so important for now] **Alarms:** Alarms are used to signal the swimmer when she/he is swimming too slow, too inefficient or with too much/less power.

## Flashtraining library further requirements

- **New training:** Upon call to ```start_new_training()``` write meta-data into flash. Furthermore, switch states and check if enough space is available. Return ```ESP_OK``` on success or an error of type ```esp_err_t```.

- **No lost training data:** After writing a block of a certain size, update the training object/struct to hold the current write index in case the device dies (then the data is not lost).

- **Store training meta data in memory:** Upon creating a new training, store the meta data in memory and update regularly during recording.

- **Get training data:** Function that takes a pointer to a 512 byte buffer and fills the buffer perfectly. Returns ```int32_t```. If there is more to come, returns -1, else: the number of bytes written.  Signature: ```int32_t get_training_data(uint8_t *buf)```.

- **Write compressed chunk:** Function that receives a pointer to an array that is holding a variable amount of data. Second argument is the amount of data ```n```. Returns ```ESP_OK``` if successfully written to memory or any error otherwise. Signature: ```esp_err_t write_compressed_chunk(uint8_t *buf, uint32_t n)```.

- **Functions:**

```
    int32_t get_next_buffer_of_training(uint8_t *); // Takes pointer to buffer. Returns -1 if wrote 512 bytes else the number of bytes written.
    void completed_synch_of_training(bool); // Passes a boolean indicating whether the training was successfully synched.
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