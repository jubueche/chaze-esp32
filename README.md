# Chaze ESP32 Hardware code

## Bluetooth connection

- **Battery:** The app should be able to query the battery level of the device. ```Input: b``` followed by ```Output: 0-100``` which is the percentage.

- **Name:** User should be able to change the advertised name of the device. The name should not be too long and should not contain any characters apart from numbers and upper/lower case letters. Command to request a change of name: ```Input: n``` followed by the name, followed by ```-1```. Returns ```1``` if successful and ```0``` otherwise. Returns to menu.

- **WiFi Configuration:** The user should be able to configure the WiFi-SSID and the password. The SSID should have a reasonable maximum length. Command: ```Input: w``` followed by the ```SSID```, ```Output successful ? 1 : 0```, followed by the ```password```, ```Output successful ? 1 : 0```. Returns to menu.

- **Get version:** The App should be able to query the current firmware version of the device to be able to offer a firmware upgrade to the user. Command: ```Input: v```. ```Output: v1.3``` for example. Return to menu.

- **Execute OTA-Update:** The device should be able to perform an OTA-update to update the running firmware on the device. Command: ```Input: f```, ```Output successful ? s (success) :  n (no internet) : e else```. Return to menu. When performing the OTA, the device must check if an internet connection is available. If there is an internet connection, call ```check_and_update()``` to perform the update. TODO: Change signature to return a boolean to indicate success. Afterwards, return to menu.

- **Synchronize trainings:** Synch all the trainings that are currently in the memory that have not been synched. Command: ```Input: s```. ```Output: Number of trainings to be synched.``` followed by ```600 byte packages```, followed by ```training synched indicator```. After a single training has been synched, wait for the phone to send an indicator that it received the training so we can mark it as synched. We the continue with ```more trainings```, followed by ```all trainings synched indicator```. After that, the device starts the deletion process and goes into deep sleep after it is done deleting. The deletion is postponed if the user chooses an immediate action after the synching process. In general, deletion should always happen before going into deep sleep mode. TODO: What will be the indicators? Note we are sending compressed data so no special chracters are distinguishable.

- [Not so important for now] **Alarms:** Alarms are used to signal the swimmer when she/he is swimming too slow, too inefficient or with too much/less power.

## Flashtraining library further requirements

- **New training:** Upon call to ```start_new_training()``` create a new training instance and populate fields such as starting write-index and date. Furthermore, switch states and check if enough space is available. Return ```ESP_OK``` on success or an error of type ```esp_err_t```.

- **No lost training data:** After writing a block of a certain size, update the training object/struct to hold the current write index in case the device dies (then the data is not lost).

- **Training meta data:** Supply functions that let the caller query trainings by different indicators. ```get_unsynched_trainings()``` should return an array of trainings that have not been synched yet. ```get_all_trainings()``` should return an array of all the trainings available. If there are not trainings, return ```NULL```.

- **Store training meta data in memory:** Upon creating a new training, store the object in memory and update regularly during recording.

- **Get training data:** Function that takes a pointer to a training struct and a pointer to a 512 byte buffer that fills the buffer perfectly and returns ```true``` if there is more to come and ```false``` if this was the last package. Signature: ```bool get_training_data(training_t * current_training, uint8_t *buf)```.

- **Write compressed chunk:** Function that receives a pointer to an array that is holding a variable amount of data. Second argument is the amount of data ```n```. Returns ```ESP_OK``` if successfully written to memory or any error otherwise. Signature: ```esp_err_t write_compressed_chunk(uint8_t *buf, uint32_t n)```.

- **Store device meta data:** Functions in ```Configuration.cpp``` that make it possible to retrieve the following device meta data:
    - Device ID
    - Azure Connection String
    - WiFi SSID
    - WiFi password
One example would be ```char * get_device_id(void)```, which will be used like this: ```char * d_id = config.get_device_id();```. This function will internally make a call to a function in the flash library, which returns the deviceID.

## TODO

- Make config class immutable
- Add watchdog

## Bugs when setting up

1) When compiling the arduino core for ESP32, the pragma on line 238 in BLERemoteService.cpp is ignored. To fix it, move the line ```#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"``` to the top of the file. Change ignore to ```warning```.

2) Use 2018 version of the xtensa compiler (```xtensa-esp32-elf-gcc8_2_0-esp32-2018r1-linux-amd64```) and the version 3.3 of the ESP-IDF.

3) Fix bug in ```esp-idf/components/newlib/include/ctype.h``` where we replace ```#define __ctype_lookup(__c) ((__ctype_ptr__+sizeof(""[__c]))[(int)(__c)])``` with ```#define __ctype_lookup(__c) ((__ctype_ptr__+sizeof(""[(int)__c]))[(int)(__c)])```

4) Delete AzureIoT from Arduino libraries and Camera library.

5) Fix I2C bug, which can be found [here](https://github.com/espressif/esp-idf/issues/680) (see arsinios answer)