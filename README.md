# Chaze ESP32 Hardware code

## TODO

- Priority queue for sampling on same I2C bus between BNO055 and MS587

## Bugs when setting up

1) When compiling the arduino core for ESP32, the pragma on line 238 in BLERemoteService.cpp is ignored. To fix it, move the line ```#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"``` to the top of the file. Change ignore to ```warning```.

2) Use 2018 version of the xtensa compiler (```xtensa-esp32-elf-gcc8_2_0-esp32-2018r1-linux-amd64```) and the version 3.3 of the ESP-IDF.

3) Fix bug in ```esp-idf/components/newlib/include/ctype.h``` where we replace ```#define __ctype_lookup(__c) ((__ctype_ptr__+sizeof(""[__c]))[(int)(__c)])``` with ```#define __ctype_lookup(__c) ((__ctype_ptr__+sizeof(""[(int)__c]))[(int)(__c)])```

4) Delete AzureIoT from Arduino libraries and Camera library.

5) Fix I2C bug, which can be found [here](https://github.com/espressif/esp-idf/issues/680) (see arsinios answer)