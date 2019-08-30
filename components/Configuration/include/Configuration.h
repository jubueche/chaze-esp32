#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include "time.h"
#include "driver/i2c.h"


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


#define B(x) S_to_binary_(#x)
#define I2B(x) to_bin_string(x)


static esp_err_t initialize_spi(void)
{
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = GPIO_SPI;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  esp_err_t err =  gpio_config(&io_conf);

  gpio_set_level(GPIO_NUM_13,1);
  gpio_set_level(GPIO_NUM_32,1);
  gpio_set_level(GPIO_NUM_14,1);

  return err;
}


static esp_err_t turn_on_main_circuit(void)
{
	gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = GPIO_OUTPUT_MC;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);

	esp_err_t err = gpio_set_level(GPIO_NUM_26,1);
	return err;
}

/**
 * @brief Initialize I2C.
 * @return Error message or ESP_OK.
 */
esp_err_t static i2c_master_init_IDF(i2c_port_t port_num)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(port_num, &conf);
    return i2c_driver_install(port_num, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

static esp_err_t i2c_master_driver_initialize()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        {.master = { .clk_speed = I2C_MASTER_FREQ_HZ }}
    };
    return i2c_param_config(I2C_NUM_1, &conf);
}

static int do_i2cdetect_cmd(void)
{
    i2c_master_driver_initialize();
    i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    uint8_t address;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (address << 1) | WRITE_BIT, ACK_CHECK_EN);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 50 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                printf("%02x ", address);
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }

    i2c_driver_delete(I2C_NUM_1);
    return 0;
}

/*
static inline unsigned long long S_to_binary_(const char *s)
{
    unsigned long long i = 0;
    while (*s) {
        i <<= 1;
        i += *s++ - '0';
    }
    return i;
}

static int string_length(char *pointer)
{
   int c = 0;

   while( *(pointer + c) != '\0' )
      c++;

   return c;
}

static void my_reverse(char *s)
{
   int length, c;
   char *begin, *end, temp;

   length = string_length(s);
   begin  = s;
   end    = s;

   for (c = 0; c < length - 1; c++)
      end++;

   for (c = 0; c < length/2; c++)
   {
      temp   = *end;
      *end   = *begin;
      *begin = temp;

      begin++;
      end--;
   }
}

static void to_bin_string(uint16_t n){
	char word[17];
	int c,k;
	for (c = 15; c >= 0; c--)
	  {
	    k = n >> c;
	    if (k & 1)
	      word[c] = (char) '1';
	    else
	      word[c] = '0';
	  }
	word[16] = '\0';
	my_reverse(word);
	printf(word);
	printf("\n");
}

static void to_bin_string(uint8_t n){
	char word[9];
	int c,k;
	for (c = 7; c >= 0; c--)
	  {
	    k = n >> c;
	    if (k & 1)
	      word[c] = (char) '1';
	    else
	      word[c] = '0';
	  }
	word[8] = '\0';
	my_reverse(word);
	printf(word);
	printf("\n");
}

static void to_bin_string(uint32_t n){
	char word[33];
	int c,k;
	for (c = 31; c >= 0; c--)
	  {
	    k = n >> c;
	    if (k & 1)
	      word[c] = (char) '1';
	    else
	      word[c] = '0';
	  }
	word[32] = '\0';
	my_reverse(word);
	printf(word);
	printf("\n");
}

static uint32_t min(uint32_t a, uint32_t b){
	if(a < b){
		return a;
	} else return b;
}

static uint32_t max(uint32_t a, uint32_t b){
	if(a < b){
		return b;
	} else return a;
}

static float min(float a, float b){
	if(a < b){
		return a;
	} else return b;
}

static float max(float a, float b){
	if(a < b){
		return b;
	} else return a;
}
*/
#endif
