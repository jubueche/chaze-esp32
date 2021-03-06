#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "time.h"
#include "driver/i2c.h"
#include "Chaze_Realtime.h"
#include "freertos/queue.h"
#include "ChazeFlashtrainingWrapper.h"
#include "BLECharacteristic.h"
#define DEBUG 1


/*
 * Section: GPIO
 */

// Main circuit
#define GPIO_MAIN_CIRCUIT 26
#define GPIO_OUTPUT_MC  (1ULL<<GPIO_MAIN_CIRCUIT)
// Vibration motor
#define GPIO_VIB GPIO_NUM_25
#define GPIO_VIB_MASK (1ULL<<25)
// BNO055 interrupt
#define GPIO_BNO_INT GPIO_NUM_36
#define GPIO_BNO_INT_NUM 36
#define GPIO_BNO_INT_MASK (1ULL<<GPIO_BNO_INT_NUM)
// Button interrupt
#define GPIO_BUTTON_NUM 39
#define GPIO_BUTTON GPIO_NUM_39 
#define GPIO_BUTTON_MASK (1ULL<<GPIO_BUTTON_NUM)
// LEDs
#define GPIO_LED_RED GPIO_NUM_12
#define GPIO_LED_GREEN GPIO_NUM_27
#define GPIO_BLUE GPIO_NUM_33
#define GPIO_LED_MASK ((1ULL<<12) | (1ULL<<27) | (1ULL<<33))
// Heart rate sensor 5V boost
#define GPIO_HR_BOOST GPIO_NUM_2
#define GPIO_HR_BOOST_MASK (1ULL<<2)

/*
 * Section SPI
 */
//#define SPIFlash_SUPPLY 7
#define SPIFlash_CS 15
#define SPIFlash_RESET 13
#define SPIFlash_WP 32
#define SPIFlash_HOLD 14
#define SPI_MOSI 18
#define SPI_MISO 19
#define SPI_SCK 5
#define GPIO_SPI ((1ULL<<SPIFlash_RESET) | (1ULL<<SPIFlash_WP) | (1ULL<<SPIFlash_HOLD))

/*
 * Section: I2C
 */
#define I2C_MASTER_SCL_IO GPIO_NUM_22 //GPIO of SCL
#define I2C_MASTER_SDA_IO GPIO_NUM_23 //GPIO of SDA
#define I2C_MASTER_TWO_SCL_IO GPIO_NUM_4
#define I2C_MASTER_TWO_SDA_IO GPIO_NUM_21
#define I2C_MASTER_FREQ_HZ 100000 //100 kHz is standard, but also 400kHz is supported
#define READ_BIT 1
#define WRITE_BIT 0
#define ACK_CHECK_EN 0x1 //Master will check ACK from slave
#define ACK_CHECK_DIS 0x0
#define ACK_VAL I2C_MASTER_ACK //This is the I2C ACK value
#define NACK_VAL I2C_MASTER_NACK
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

/*
 * Section: General settings.
 */
#define TIMEOUT_AFTER_ACK 3000 /*3.0s*/
#define TIMEOUT_AFTER_SEND 20000
#define TIMEOUT_AFTER_ADVERTISING 60000 // 10 min.
#define TIMEOUT_BUTTON_PUSHED_TO_SLEEP 1200
#define TIMEOUT_BUTTON_PUSHED_TO_ADVERTISING 1200
#define TIMEOUT_BUTTON_PUSHED_TO_OFF 3000
#define LED_RED_TIMEOUT 5000
#define LED_BLUE_TIMEOUT 3000

#define UPLOAD_BLOCK_SIZE 512
#define UPLOAD_BLOCK_SIZE_BLE 512 /*Smaller than 600 and multiple of 2. 512 is ideal.*/

#define NO_MOTION_DURATION 50
#define NO_MOTION_THRESHOLD 5
#define ANY_MOTION_THRESHOLD 244
#define ANY_MOTION_DURATION 10

#define FLUID_DENSITY 970

/*
 * Offset in memory at which the 22 calibration offsets of the BNO are stored.
 */
#define SENSOR_CALIBRATION_OFFSET 20

/*
 * Main state machine variables.
 */
enum {DEEPSLEEP, RECORD, ADVERTISING, CONNECTED, CONNECTED_WIFI, CONNECTED_BLE};
enum {AWAITING, SYNCH_COMPLETE, SYNCH_INCOMPLETE};


class Configuration {
  public:

    uint32_t get_number_of_unsynched_trainings(void);

    void populate_pressure(uint8_t *, float, unsigned long, bool);
    void populate_bno(uint8_t *, float *, unsigned long);
    void populate_heart_rate(uint8_t *, uint32_t, unsigned long);
    void populate_heart_rate_raw(uint8_t *, int32_t *, int32_t, unsigned long);
    
    void attach_bno_int(void (*)(void *), void (*)(void *));
    void attach_btn_int(void (*)(void *), void (*)(void *));
    void detach_bno_int(void);
    void detach_btn_int(void);
    
    esp_err_t turn_on_boost(void);
    esp_err_t initialize_spi(void);
    esp_err_t turn_on_main_circuit(void);
    esp_err_t turn_off_main_circuit(void);
    
    esp_err_t i2c_master_init_IDF(i2c_port_t port_num);
    esp_err_t i2c_master_driver_initialize(void);
    esp_err_t write(uint8_t *, size_t, uint8_t, i2c_port_t);
    esp_err_t read(uint8_t *, size_t, uint8_t, i2c_port_t);
    int do_i2cdetect_cmd(void);
    
    esp_err_t vibration_signal_sleep(void);
    esp_err_t initialize_vib(void);
    esp_err_t initialize_leds(void);
    void flicker_led(gpio_num_t);
    int random_between(int, int);
    uint8_t get_battery_level(void);

    volatile uint8_t STATE;
    volatile bool ble_connected = false;
    volatile bool event_handler_set = false;
    volatile bool OTA_request = false;
    volatile bool wifi_synch_task_suspended = false;
    volatile uint8_t synched_training = AWAITING;
    volatile bool wifi_connected = false;
    volatile bool allow_azure = true;
    volatile bool wifi_setup_once = false;

    xQueueHandle gpio_evt_queue;
    SemaphoreHandle_t i2c_semaphore = xSemaphoreCreateRecursiveMutex();
    SemaphoreHandle_t i2c_back_semaphore = xSemaphoreCreateRecursiveMutex();
    SemaphoreHandle_t write_buffer_semaphore = xSemaphoreCreateMutex();
    SemaphoreHandle_t compress_and_save_semaphore = xSemaphoreCreateMutex();
    SemaphoreHandle_t wifi_synch_semaphore = NULL;

    TaskHandle_t bno_interrupt_task_handle = NULL;
    TaskHandle_t btn_interrupt_task_handle = NULL;

    bool initialized_port0 = false;
    bool initialized_port1 = false;
    bool isr_installed = false;

    uint16_t MTU_BLE = 512;

  private:
    const char * TAG = "Configuration";
};

extern Configuration config;   

#endif
