#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include "time.h"
#include "driver/i2c.h"
#include "Wire.h"
#include "Chaze_Realtime.h"
#include "freertos/queue.h"


/*
 * Section: GPIO
 */

// Main circuit
#define GPIO_MAIN_CIRCUIT 26
#define GPIO_OUTPUT_MC  (1ULL<<GPIO_MAIN_CIRCUIT)
// Vibration motor
#define GPIO_VIB GPIO_NUM_25
#define GPIO_VIB_MASK (1ULL<<25)
//BNO055 interrupt
#define GPIO_BNO_INT GPIO_NUM_36
#define GPIO_BNO_INT_NUM 36
#define GPIO_BNO_INT_MASK (1ULL<<GPIO_BNO_INT_NUM)

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
#define TIMEOUT_AFTER_ADVERTISING 30000
#define TIMEOUT_BUTTON_PUSHED_TO_SLEEP 1200
#define TIMEOUT_BUTTON_PUSHED_TO_ADVERTISING 1200
#define LED_RED_TIMEOUT 5000
#define LED_BLUE_TIMEOUT 1000

#define NO_MOTION_DURATION 50
#define NO_MOTION_THRESHOLD 5
#define ANY_MOTION_THRESHOLD 244
#define ANY_MOTION_DURATION 10

/*
 * Main state machine variables.
 */
enum {DEEPSLEEP, RECORD, ADVERTISING, CONNECTED, CONNECTED_WIFI, CONNECTED_BLE};


class Configuration {
  public:
    void attach_bno_int(void (*)(void *), void (*)(void *));
    void detach_bno_int(void);
    esp_err_t initialize_spi(void);
    esp_err_t turn_on_main_circuit(void);
    esp_err_t turn_off_main_circuit(void);
    esp_err_t i2c_master_init_IDF(i2c_port_t port_num);
    esp_err_t i2c_master_driver_initialize(void);
    int do_i2cdetect_cmd(void);
    esp_err_t initialize_rtc(void);
    esp_err_t vibration_signal_sleep(void);
    esp_err_t initialize_vib(void);
    volatile uint8_t STATE;
    // Queue used to handle interrupts
    xQueueHandle gpio_evt_queue;
  private:
    const char * TAG = "Configuration";
};

extern Configuration config;   

#endif
