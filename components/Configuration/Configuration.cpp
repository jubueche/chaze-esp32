#include "Configuration.h"


Configuration config;


// TODO Needs implementation
uint8_t Configuration::get_battery_level(void)
{
    //! Need to read calibration of the voltage divider
    float voltage = analogRead(GPIO_NUM_35) * 2.2 * 3.3 / 4096;   //! ACHTUNG: Spannungsteilerverhältnis kann variieren (hier 2.2, normal 2)
    uint8_t percentage = (voltage - 3.7) * 2 * 100;
    return percentage; 
}

uint32_t Configuration::get_number_of_unsynched_trainings(void)
{
    FlashtrainingWrapper *ft = FlashtrainingWrapper_create();
    uint32_t num_unsynched_trainings = FlashtrainingWrapper_get_number_of_unsynched_trainings(ft);
    FlashtrainingWrapper_destroy(ft);
    return num_unsynched_trainings;
}


void Configuration::populate_pressure(uint8_t * bytes, float pressure, unsigned long sample_time, bool is_back)
{
    long tmp = *(long*)&pressure;
    if(is_back)
    {
        bytes[0] = 2;
    } else {
        bytes[0] = 5;
    }
    bytes[1] = sample_time >> 24;
    bytes[2] = sample_time >> 16;
    bytes[3] = sample_time >> 8;
    bytes[4] = sample_time;
    
    bytes[5] = tmp >> 24;
    bytes[6] = tmp >> 16;
    bytes[7] = tmp >> 8;
    bytes[8] = tmp;
}

void Configuration::populate_heart_rate(uint8_t * bytes, uint32_t heart_rate, unsigned long sample_time)
{
    long tmp = *(long*)&heart_rate;

    bytes[0] = 3;
    bytes[1] = sample_time >> 24;
    bytes[2] = sample_time >> 16;
    bytes[3] = sample_time >> 8;
    bytes[4] = sample_time;
    
    bytes[5] = tmp >> 24;
    bytes[6] = tmp >> 16;
    bytes[7] = tmp >> 8;
    bytes[8] = tmp;
}

void Configuration::populate_heart_rate_raw(uint8_t * bytes, int32_t * heart_rate_raw, int32_t num_samples, unsigned long sample_time)
{
    bytes[0] = 6;
    bytes[1] = sample_time >> 24;
    bytes[2] = sample_time >> 16;
    bytes[3] = sample_time >> 8;
    bytes[4] = sample_time;

    int j = 0;
    for(int i=5;i<num_samples*4+5;i+=4)
    {
        long tmp = *(long*)&(heart_rate_raw[j]);
        bytes[i+0] = tmp >> 24;
        bytes[i+1] = tmp >> 16;
        bytes[i+2] = tmp >> 8;
        bytes[i+3] = tmp;
        j +=1;
    }
}

void Configuration::populate_bno(uint8_t * bytes, float * values, unsigned long sample_time)
{
    bytes[0] = 4;
    bytes[1] = sample_time >> 24;
    bytes[2] = sample_time >> 16;
    bytes[3] = sample_time >> 8;
    bytes[4] = sample_time;

    int j = 0;
    for(int i=5;i<33;i=i+4)
    {
        long tmp = *(long*)&values[j];
        bytes[i] = tmp >> 24;
		bytes[i+1] = tmp >> 16;
		bytes[i+2] = tmp >> 8;
		bytes[i+3] = tmp;
        j += 1;
    }

}

void Configuration::attach_btn_int( void (*handlerTask)(void *), void (*gpio_isr_handler)(void *) )
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_BUTTON_MASK;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    esp_err_t err = gpio_config(&io_conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting GPIO config.");
    }

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task

    if(!this->isr_installed)
    {
        //install gpio isr service
        gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
        this->isr_installed = true;
    }

    if(this->btn_interrupt_task_handle == NULL)
    {
        if(xTaskCreate(*handlerTask, "Button-interrupt handler", 2048, NULL, 10, &this->btn_interrupt_task_handle) != pdPASS)
        {
            ESP_LOGE(TAG, "Error creating Button handler.");
        }
    }
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_BUTTON, *gpio_isr_handler, (void*) GPIO_BUTTON);
}

void Configuration::attach_bno_int( void (*handlerTask)(void *), void (*gpio_isr_handler)(void *) )
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_BNO_INT_MASK;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    esp_err_t err = gpio_config(&io_conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting GPIO config.");
    }

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio 

    if(!this->isr_installed)
    {
        //install gpio isr service
        gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
        this->isr_installed = true;
    }

    if(this->bno_interrupt_task_handle == NULL)
    {
        if(xTaskCreate(*handlerTask, "BNO055-interrupt handler", 2048, NULL, 10, &this->bno_interrupt_task_handle) != pdPASS)
        {
            ESP_LOGE(TAG, "Error creating BNO handler.");
        }
    }
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_BNO_INT, *gpio_isr_handler, (void*) GPIO_BNO_INT);   
}

void Configuration::detach_btn_int()
{
    gpio_isr_handler_remove(GPIO_BUTTON);
    vTaskDelete(this->btn_interrupt_task_handle);
    this->btn_interrupt_task_handle = NULL;
}

void Configuration::detach_bno_int()
{
    gpio_isr_handler_remove(GPIO_BNO_INT);
    vTaskDelete(this->bno_interrupt_task_handle);
    this->bno_interrupt_task_handle = NULL;
}

esp_err_t Configuration::initialize_vib(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_VIB_MASK;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    esp_err_t err =  gpio_config(&io_conf);
    return err;
}

esp_err_t Configuration::initialize_leds(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_LED_MASK;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    esp_err_t err =  gpio_config(&io_conf);
    return err;
}

int Configuration::random_between(int l, int u)
{
    return (rand() % (u-l)) + l; 
}

void Configuration::flicker_led(gpio_num_t led)
{
    for(int i=0;i<15;i++){
			gpio_set_level(led, 1);
			vTaskDelay(80 / portTICK_PERIOD_MS);
			gpio_set_level(led, 0);
			vTaskDelay(80 / portTICK_PERIOD_MS);
	}
}

esp_err_t Configuration::vibration_signal_sleep(void)
{
    esp_err_t err = initialize_vib();

    if(err != ESP_OK)
        return err;
    
    gpio_set_level(GPIO_VIB, 1);
    vTaskDelay(400 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_VIB, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_VIB, 1);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_VIB, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_VIB, 1);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_VIB, 0);

    return err;
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

esp_err_t Configuration::turn_off_main_circuit(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_MC;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

	esp_err_t err = gpio_set_level(GPIO_NUM_26,0);
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

esp_err_t Configuration::turn_on_boost(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_HR_BOOST_MASK;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    esp_err_t err = gpio_set_level(GPIO_HR_BOOST,1);
    return err;
}

/**
 * @brief Initialize I2C.
 * @return Error message or ESP_OK.
 */
esp_err_t Configuration::i2c_master_init_IDF(i2c_port_t port_num)
{
    if(port_num == I2C_NUM_0){
        if(initialized_port0)
            return ESP_OK;
    } else{
        if(initialized_port1){
            return ESP_OK;
        }
    }
    port_num == I2C_NUM_0 ? initialized_port0 = true : initialized_port1 = true;

    gpio_num_t scl;
    gpio_num_t sda;
    if(port_num == I2C_NUM_0)
    {
        scl = I2C_MASTER_SCL_IO;
        sda = I2C_MASTER_SDA_IO;
    } else {
        scl = I2C_MASTER_TWO_SCL_IO;
        sda = I2C_MASTER_TWO_SDA_IO;
    }

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(port_num, &conf);
    return i2c_driver_install(port_num, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}


/**
 * @brief Write a package to the I2C bus.
 * @param data_wr Payload to write.
 * @param size Size of the payload.
 * @param uint8_t address to the sensor.
 * @param i2c_port_t Either port 0 or 1.
 * @return
 */
esp_err_t Configuration::write(uint8_t * data_wr, size_t size, uint8_t addr, i2c_port_t port_num){
	
	esp_err_t err = ESP_OK;
    bool res;
    if(port_num == I2C_NUM_0)
    {
        res = xSemaphoreTakeRecursive(i2c_semaphore, pdMS_TO_TICKS(300)) == pdPASS;
    } else {
        res = xSemaphoreTakeRecursive(i2c_back_semaphore, pdMS_TO_TICKS(300)) == pdPASS;
    }
	if(res)
	{
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (addr << 1) | WRITE_BIT, ACK_CHECK_EN);
		i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(port_num, cmd, 1000 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);
        if(port_num == I2C_NUM_0)
        {
            xSemaphoreGiveRecursive(i2c_semaphore);
        } else {
            xSemaphoreGiveRecursive(i2c_back_semaphore);
        }
	} else {
        ESP_LOGE(TAG, "Did not acquire i2c lock.");
    }
	
	return err;
}


/**
 * @brief Reads from the I2C.
 * @param data_rd Pointer to array to be filled.
 * @param size Number of uint8_t's to read.
 * @param uint8_t address to the sensor.
 * @param i2c_port_t Either port 0 or 1.
 * @return Error or ESP_OK.
 */
esp_err_t Configuration::read(uint8_t * data_rd, size_t size, uint8_t addr, i2c_port_t port_num){
  
    if (size == 0) {
            return ESP_OK;
    }
    esp_err_t ret = ESP_OK;

    bool res;
    if(port_num == I2C_NUM_0)
    {
        res = xSemaphoreTakeRecursive(i2c_semaphore, pdMS_TO_TICKS(300)) == pdPASS;
    } else {
        res = xSemaphoreTakeRecursive(i2c_back_semaphore, pdMS_TO_TICKS(300)) == pdPASS;
    }

    if(res)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | READ_BIT, ACK_CHECK_EN);
        if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
        }
        i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(port_num, cmd, 1000 / portTICK_RATE_MS);
        if(addr == 0x28 && ret == ESP_FAIL){
            ret = ESP_OK; // BNO does not ack. if he is booting up.
        }
        i2c_cmd_link_delete(cmd);

        if(port_num == I2C_NUM_0)
        {
            xSemaphoreGiveRecursive(i2c_semaphore);
        } else {
            xSemaphoreGiveRecursive(i2c_back_semaphore);
        }
    } else {
        ESP_LOGE(TAG, "Did not acquire i2c lock.");
    }
  
	return ret;
}

//Only used by do_i2cdetect_cmd()
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