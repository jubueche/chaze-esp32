#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include "time.h"
#include "driver/i2c.h"
#include "Wire.h"
#include "Chaze_Realtime.h"

/*
 * Section: GPIO
 */

#define GPIO_MAIN_CIRCUIT 26
#define GPIO_OUTPUT_MC  (1ULL<<GPIO_MAIN_CIRCUIT)

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

class Configuration {
  public:
    esp_err_t initialize_spi(void);
    esp_err_t turn_on_main_circuit(void);
    esp_err_t i2c_master_init_IDF(i2c_port_t port_num);
    esp_err_t i2c_master_driver_initialize(void);
    int do_i2cdetect_cmd(void);
    esp_err_t initialize_rtc(void);
  private:
    const char * TAG = "Configuration";
};

extern Configuration config;   

#endif
