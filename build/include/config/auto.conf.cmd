deps_config := \
	/home/julian/esp/esp-idf/components/app_trace/Kconfig \
	/home/julian/esp/esp-idf/components/aws_iot/Kconfig \
	/home/julian/esp/esp-idf/components/bt/Kconfig \
	/home/julian/esp/esp-idf/components/driver/Kconfig \
	/home/julian/esp/esp-idf/components/efuse/Kconfig \
	/home/julian/esp/esp-idf/components/esp32/Kconfig \
	/home/julian/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/julian/esp/esp-idf/components/esp_event/Kconfig \
	/home/julian/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/julian/esp/esp-idf/components/esp_http_server/Kconfig \
	/home/julian/esp/esp-idf/components/esp_https_ota/Kconfig \
	/home/julian/esp/esp-idf/components/espcoredump/Kconfig \
	/home/julian/esp/esp-idf/components/ethernet/Kconfig \
	/home/julian/esp/esp-idf/components/fatfs/Kconfig \
	/home/julian/esp/esp-idf/components/freemodbus/Kconfig \
	/home/julian/esp/esp-idf/components/freertos/Kconfig \
	/home/julian/esp/esp-idf/components/heap/Kconfig \
	/home/julian/esp/esp-idf/components/libsodium/Kconfig \
	/home/julian/esp/esp-idf/components/log/Kconfig \
	/home/julian/esp/esp-idf/components/lwip/Kconfig \
	/home/julian/esp/esp-idf/components/mbedtls/Kconfig \
	/home/julian/esp/esp-idf/components/mdns/Kconfig \
	/home/julian/esp/esp-idf/components/mqtt/Kconfig \
	/home/julian/esp/esp-idf/components/nvs_flash/Kconfig \
	/home/julian/esp/esp-idf/components/openssl/Kconfig \
	/home/julian/esp/esp-idf/components/pthread/Kconfig \
	/home/julian/esp/esp-idf/components/spi_flash/Kconfig \
	/home/julian/esp/esp-idf/components/spiffs/Kconfig \
	/home/julian/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/julian/esp/esp-idf/components/unity/Kconfig \
	/home/julian/esp/esp-idf/components/vfs/Kconfig \
	/home/julian/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/julian/esp/esp-idf/components/wifi_provisioning/Kconfig \
	/home/julian/esp/esp-idf/components/app_update/Kconfig.projbuild \
	/home/julian/Documents/chaze-esp32/components/arduino/Kconfig.projbuild \
	/home/julian/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/julian/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/julian/Documents/chaze-esp32/main/Kconfig.projbuild \
	/home/julian/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/julian/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
