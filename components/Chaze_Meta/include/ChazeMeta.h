#ifndef CHAZEMETA_H
#define CHAZEMETA_H

#include "nvs_flash.h"
#include "nvs.h"
#include "Configuration.h"


class ChazeMeta {

    public:
        ChazeMeta();

        // Helper functions for reading and writing to NVS flash memory
        char * get_string_pointer_from_memory(const char *, size_t , char *);
        bool set_string_in_memory(char *, char *, uint8_t);
        bool initialize_flash();
        char * get_device_name(void); //Returns device ID //! Maybe in non-er. memory of flash?
        char * get_azure_connection_string(void); // Returns connection string for Azure.
        char * get_wifi_ssid(void);
        char * get_wifi_password(void);
        bool set_device_name(char *, uint8_t); //Sets device name
        bool set_azure_connection_string(char *, uint8_t); // Sets connection string for Azure.
        bool set_wifi_ssid(char *, uint8_t);
        bool set_wifi_password(char *, uint8_t);
        bool set_name(char *, uint8_t);
        char * get_name(void);
        bool set_container_name(char *, uint8_t);
        char * get_container_name(void);
        uint8_t get_version(char *); //! Done Maybe have #define in main.cpp or Configuration.c?
        bool set_version(char *, uint8_t);

    private:
        const char * TAG = "Chaze-Flashtraining";
};

extern ChazeMeta* global_chaze_meta;

#endif