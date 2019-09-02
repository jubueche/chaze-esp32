#include "Configuration.h"


Configuration config;

esp_err_t Configuration::initialize_rtc(void)
{
    Wire.begin(I2C_MASTER_SDA_IO,I2C_MASTER_SCL_IO,I2C_MASTER_FREQ_HZ);
    if (rtc.begin() == false) {
        ESP_LOGE(TAG, "Error initializing RT-Clock.");
        return ESP_FAIL;
    }
    else
        return ESP_OK;
}

esp_err_t Configuration::initialize_spi(void)
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


esp_err_t Configuration::turn_on_main_circuit(void)
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
esp_err_t Configuration::i2c_master_init_IDF(i2c_port_t port_num)
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

esp_err_t Configuration::i2c_master_driver_initialize()
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

int Configuration::do_i2cdetect_cmd(void)
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