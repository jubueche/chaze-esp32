# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/esp-azure/port $(PROJECT_PATH)/components/esp-azure/port/inc $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/certs $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/c-utility/inc $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/c-utility/inc/azure_c_shared_utility $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/c-utility/pal/inc $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/c-utility/pal/freertos $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/c-utility/pal/generic $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/iothub_client/inc $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/serializer/inc $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/umqtt/inc $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/umqtt/inc/azure_umqtt_c $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/deps/parson $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/provisioning_client/inc $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/provisioning_client/adapters $(PROJECT_PATH)/components/esp-azure/azure-iot-sdk-c/provisioning_client/deps/utpm/inc
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp-azure -lesp-azure
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp-azure
COMPONENT_LDFRAGMENTS += 
component-esp-azure-build: 
