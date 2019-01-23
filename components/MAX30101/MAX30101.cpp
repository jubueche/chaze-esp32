/**
 * MAX30101
 * High-Sensitivity Pulse Oximeter and
 * Heart-Rate Sensor for Wearable Health
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"


#include "MAX30101.h"
#include "Configuration.h"
#include "Wire.h"



/* Status */
#define REG_INT_MSB        0x00 /* Interrupt Status 1 */
#define REG_INT_LSB        0x01 /* Interrupt Status 2 */
#define REG_INT_ENB_MSB    0x02 /* Interrupt Enable 1 */
#define REG_INT_ENB_LSB    0x03 /* Interrupt Enable 2 */
/* FIFO */
#define REG_FIFO_WR_PTR    0x04 /* FIFO Write Pointer */
#define REG_OVF_COUNTER    0x05 /* Overflow Counter */
#define REG_FIFO_RD_PTR    0x06 /* FIFO Read Pointer */
#define REG_FIFO_DATA      0x07 /* FIFO Data Register */
/* Configuration */
#define REG_FIFO_CONFIG    0x08 /* FIFO Configuration */
#define REG_MODE_CONFIG    0x09 /* Mode Configuration */
#define REG_SPO2_CONFIG    0x0A /* SpO2 Configuration */
/* reserved                0x0B */
#define REG_LED1_PA        0x0C /* LED Pulse Amplitude 1 */
#define REG_LED2_PA        0x0D /* LED Pulse Amplitude 2 */
#define REG_LED3_PA        0x0E /* LED Pulse Amplitude 3 */
#define REG_LED4_PA		   0x0F /* LED Pulse Amplitude 4 */

// #define REG_PILOT_PA       0x10 /* Deprecated */
#define REG_SLOT_MSB       0x11 /* Multi-LED Mode Control Registers 2, 1 */
#define REG_SLOT_LSB       0x12 /* Multi-LED Mode Control Registers 4, 3 */
/* DIE Temperature */
#define REG_TEMP_INT       0x1F /* Die Temperature Integer */
#define REG_TEMP_FRAC      0x20 /* Die Temperature Fraction */
#define REG_TEMP_EN        0x21 /* Die Temperature Config */

/* Part ID */
#define REG_REV_ID         0xFE /* Revision ID */
#define REG_PART_ID        0xFF /* Part ID: 0x15 */
/* Depth of FIFO */
#define FIFO_DEPTH         32

#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

/*
 * Masks for writing to the registers:
 */
#define INT_ENABLE_MASK 0x1FFD
#define FIFO_MASK 0xE0
#define MODE_CONFIG_MASK 0x38
#define SPO2_CONFIG_MASK 0x80
#define SLOT_MASK 0x8888
#define TEMP_EN_MASK 0xFE


#define USE_IDF

/*
 * Commands
 */


#define RESET 0x40 //B(01000000)

const char * TAG = "Chaze";

static xQueueHandle gpio_evt_queue = NULL;
/*
 * @brief
 * Constructor of the class.
 * @params: Source data (SDA), Source Clock (SCK), address of the chip.
 */
MAX30101::MAX30101(i2c_port_t I2C_MASTER_NUM) {
	fifo_full_interrupt = false;
	fifo_newdata_interrupt = false;
	port_num = I2C_MASTER_NUM;

	if(port_num == I2C_NUM_1){
		MAXI2C = Wire;
	} else{
		MAXI2C = Wire1;
	}
	MAXI2C.begin(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
#ifdef USE_IDF
	ESP_ERROR_CHECK(i2c_master_init_IDF());
#endif
}

/*
 * @brief: High level implementation using Wire.h Read Register from Maxim sensor.
 * @params: addr: Register address, data: pointer to array to store data, size: number of bytes to read
 */
void MAX30101::readReg(uint8_t addr, uint8_t * data, size_t size){
	if (size == 0){
		return;
	}
	MAXI2C.beginTransmission(MAXIM_I2C_ADRS);
	MAXI2C.write(addr); //Set pointer to Register adress
	MAXI2C.endTransmission();

	MAXI2C.requestFrom(MAXIM_I2C_ADRS,size);
	for(int i = 0;i < size; i++){
		data[i] = MAXI2C.read();
	}
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}


static void gpio_task_example(void* arg)
{
	MAX30101 *max = (MAX30101 *) arg;
    uint32_t io_num;
    uint16_t interrupt_status, config;
    float temp;
    uint8_t temp_int, temp_frac;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            interrupt_status = max->getIntStatus();
            config = max->getIntEnable();

            if(interrupt_status & INT_ST_A_FULL){
            	max->fifo_full_interrupt = true; //Set the volatile flag for the main thread
            }
            if(interrupt_status & INT_ST_PPG_RGY){
            	printf("New FIFO data ready.\n");
            }
            if(interrupt_status & INT_ST_ALC_OVF){
            	printf("Ambient light cancellation overflow.\n");
            }
            if(interrupt_status & INT_ST_DIE_TEMP_RDY){
            	temp_int = max->getTEMP_INT();
				temp_frac = max->getTEMP_FRAC();
				temp = ((float)temp_int)+(((float)temp_frac)/16.0);
				printf("Temp: %.2f\n", temp);

            }
            if(interrupt_status & INT_ST_PWR_RDY){
            	printf("Power ready.\n");
            }
        }
    }
}

//If we get an interrupt that signals us that new data has arrived, we read the data
void MAX30101::read_fifo(uint32_t * data, uint8_t n){
	/*
	 * LED1 is the red LED
	 * LED2 is the IR
	 * LED3 is the green LED
	 * If the mode is 0x02 (HR) only the Red LED is on and one sample consists of 3 bytes.
	 * Read like this: LED1[23:16]<-ReadFIFO, LED1[15:8]<-ReadFIFO, LED1[7:0]<-ReadFIFO.
	 */
	uint8_t num_available_samples =FIFO_DEPTH - (this->getFIFO_CONFIG() & 0x0F);
	uint8_t current_mode = this->getMODE_CONFIG();
	uint8_t rd_ptr, wr_ptr;

	rd_ptr = this->getFIFO_RD_PTR();
	wr_ptr = this->getFIFO_WR_PTR();

	printf("Read Ptr: %d  Write Ptr: %d\n", rd_ptr, wr_ptr);
	printf("Num. of avail. points: %d\n", num_available_samples);
	if(num_available_samples < n){
		printf("ERROR: Requested more samples than available.\n");
		return;
	}


	if(current_mode == SPO2_MODE){

		//SLOT1: Red LED
		//SLOT2: IF
		//1 Sample = 3*Channel 1 + 3* Channel 2 --> 3*NUM_CHANNELS*N

		uint8_t tmp[6*n];
		this->get_n_FIFO_DATA(tmp,6*n);
		for(int i=0;i<6*n;i++){
			printf("Data: %d\n", tmp[i]);
		}
		printf("\n");
	} else if(current_mode == HEART_RATE_MODE){
		//SLOT1: Red LED
		// 1 Sample = 3*Channel 1 --> 3*1*N
		uint8_t tmp[3*n];
		this->get_n_FIFO_DATA(tmp,3*n);
		for(int i=0;i<3*n;i++){
			printf("Data: %d\n", tmp[i]);
		}
		printf("\n");

	}
}


void MAX30101::read_n(uint32_t *data, uint8_t n){
	//Set the read and write pointer to 0
	this->setFIFO_RD_PTR(0);
	this->setFIFO_WR_PTR(0);
	this->setIntEnable(0);
	fifo_full_interrupt = false;

	if(mode == SPO2_MODE){
		/*
		 * 1) Set mode to SPO2 mode and set A_FULL_EN.
		 * 2) Enable DIE_TEMP_RDY_INT and set TEMP_EN.
		 * 3) ISR reads Temp data and clears interrupt register by reading frac or Int status 1.
		 * 4) Interrupt is generated that the FIFO is almost full.
		 * 5) Read n FIFO data points, update the read pointer.
		 */
		this->setMODE_CONFIG(SPO2_MODE);
		this->setIntEnable(INT_A_FULL_EN | INT_DIE_TEMP_RDY_EN);
		this->setTEMP_EN();

		for(;;){
			if(fifo_full_interrupt){
				//Read the n samples from the FIFO. Note: The mode is SPO2_MODE so one sample = 6 Bytes (3 for red LED, 3 for IF)
				read_fifo(data, n);
				fifo_full_interrupt = false;
				break;
			}
		}
	}
	else if(mode == HEART_RATE_MODE){
		/*
		 * 1) Set mode to HR mode and set A_FULL_EN.
		 * 2) Interrupt will be generated.
		 * 3) Read the data.
		 */
		this->setMODE_CONFIG(HEART_RATE_MODE);
		this->setIntEnable(INT_A_FULL_EN);

		for(;;){
			if(fifo_full_interrupt){
				//Read the n samples from the FIFO. Note: The mode is SPO2_MODE so one sample = 6 Bytes (3 for red LED, 3 for IF)
				read_fifo(data, n);
				fifo_full_interrupt = false;
				break;
			}
		}
	}
	else{

	}
}

void MAX30101::init_interrupt(){
	/*
	 * Add hardware interrupt handler.
	 */
	gpio_evt_queue = xQueueCreate(10, sizeof(gpio_num_t));
	xTaskCreate(gpio_task_example, "gpio_task_example", 2048, this, 10, NULL);
	gpio_config_t gpioConfig;
	gpioConfig.pin_bit_mask = GPIO_SEL_15;
	gpioConfig.mode = GPIO_MODE_INPUT;
	gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
	gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpioConfig.intr_type = GPIO_INTR_NEGEDGE;
	gpio_config(&gpioConfig);
	ESP_ERROR_CHECK (gpio_install_isr_service(0));
	ESP_ERROR_CHECK (gpio_isr_handler_add(gpio_num_t_INT_PIN, gpio_isr_handler, NULL));

	this->setIntEnable(0);
}

void MAX30101::disable_interrupts(void){
	gpio_intr_disable(gpio_num_t_INT_PIN);
	//vTaskDelete(gpio_task_example);
}


//Params: adc_range = MAX30101_RANGE16384, sample_rate, pulse_width, led_current,slot_multi
void MAX30101::init(maxim_config_t * args){

	printf("Reset...\n");
	this->reset();

	this->setMODE_CONFIG(args->MODE);
	mode = args->MODE;
	printf("Set mode configuration to: ");
	I2B(this->getMODE_CONFIG());

	printf("Setting FIFO flags to: ");
	uint8_t fifo_config = (args->SMP_AVE) | (args->FIFO_ROLL) | (args->FIFO_A_FULL);
	this->setFIFO_CONFIG(fifo_config);
	I2B(this->getFIFO_CONFIG());

	printf("Setting pulse amplitudes of the LEDS(1-4): ");
	//Set pulse amplitude of every LED
	this->setLED1_PA(args->PA1);
	if(args->MODE != HEART_RATE_MODE){
		this->setLED2_PA(args->PA2);
		if(args->MODE == MULTI_LED_MODE){
			this->setLED3_PA(args->PA3);
			this->setLED4_PA(args->PA4);
		}
	}

	uint16_t HR_SLOT_CONFIG = ((uint16_t)SLOT_NONE << 12) | ((uint16_t)SLOT_RED << 8) | ((uint16_t)SLOT_NONE << 4) | SLOT_NONE;
	uint16_t SPO2_SLOT_CONFIG = ((uint16_t)SLOT_IR << 12) | ((uint16_t)SLOT_RED << 8) | ((uint16_t)SLOT_NONE << 4) | SLOT_NONE;
	uint16_t MULTI_SLOT_CONFIG = ((uint16_t)SLOT_IR << 12) | ((uint16_t)SLOT_RED << 8) | ((uint16_t)SLOT_NONE << 4) | SLOT_GREEN;

	printf("Setting slot configurations...: ");
	if(args->MODE == HEART_RATE_MODE){
		this->setSLOT(HR_SLOT_CONFIG);
	} else if(args->MODE == SPO2_MODE){
		this->setSLOT(SPO2_SLOT_CONFIG);
	} else{
		this->setSLOT(MULTI_SLOT_CONFIG);
	}
	I2B(this->getSLOT());

	//Set LED pulse width
	if(args->MODE != HEART_RATE_MODE){
		printf("Setting SPO2 configs...: ");
		uint8_t spo2_config = (args->SPO2_ADC_RGE | args->SPO2_SR | args->LED_PW);
		this->setSPO2_CONFIG(spo2_config);
		I2B(this->getSPO2_CONFIG());
	}

	//Initialize the interrupts
	printf("Setting interrupts...: ");
	this->init_interrupt();

}


esp_err_t MAX30101::i2c_master_init_IDF()
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

esp_err_t MAX30101::write(uint8_t * data_wr, size_t size){
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (MAXIM_I2C_ADRS << 1) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	esp_err_t err = i2c_master_cmd_begin(port_num, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return err;
}

esp_err_t MAX30101::read(uint8_t * data_rd, size_t size){
	if (size == 0) {
	        return ESP_OK;
	}
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (MAXIM_I2C_ADRS << 1) | READ_BIT, ACK_CHECK_EN);
	if (size > 1) {
		i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
	}
	i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(port_num, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

void MAX30101::readReg_IDF(uint8_t reg, uint8_t * data, size_t size){
	if(size < 1){
		return;
	}
	uint8_t t[1] = {reg};
	ESP_ERROR_CHECK(write(t, 1));
	vTaskDelay(30 / portTICK_RATE_MS);
	ESP_ERROR_CHECK(read(data,size));
}

void MAX30101::writeReg_IDF(uint8_t reg, uint8_t * data, size_t size){
	if(size < 1){
		return;
	}
	//Populate new array with the Register that should be written and the data that should be written.
	uint8_t * t = (uint8_t *) malloc(1 + size);
	if(t == NULL){
		ESP_LOGE(TAG, "Could not allocate space");
		return;
	}
	t[0] = reg; //Jump to REG and write from there on. If we want to write only one register, data has only one entry.
	for(int i = 0;i<size;i++){
		t[i+1] = data[i];
	}
	ESP_ERROR_CHECK(write(t, size+1)); //Write reg+data -> 1+size
	free(t);
}

void MAX30101::reset(){
	//Reset the board
	this->setMODE_CONFIG(RESET);
}

uint8_t MAX30101::readID(){
	uint8_t data;
	readReg_IDF(REG_PART_ID, &data, 1);
	return data;
}

uint8_t MAX30101::revID(){
	uint8_t data;
	readReg_IDF(REG_REV_ID, &data, 1);
	return data;
}

uint16_t MAX30101::getIntStatus(void)
{
    uint8_t res[2];
    uint16_t value;
    readReg_IDF(REG_INT_MSB, res, 2);
    value = (res[0] << 8) | res[1];
    return(value);
}

uint8_t MAX30101::getIntStatus1(void)
{
	uint8_t res[1];
	readReg_IDF(0x01, res, 1);
	return res[0];
}

uint16_t MAX30101::getIntEnable(void)
{
    uint8_t res[2];
    uint16_t value;
    readReg_IDF(REG_INT_ENB_MSB, res, 2);
    value = (res[0] << 8) | res[1];
    return(value);
}

void MAX30101::setIntEnable(uint16_t mask)
{
	uint16_t curr = this->getIntEnable();
	curr = curr & INT_ENABLE_MASK; //B(00011111 11111101)
	mask = mask | curr;
    uint8_t res[2];
    res[0] = (mask >> 8) & 0xFF; //If we have 00000000 00000010 we write 00000000 to 0x02 and 00..10 to 0x03
    res[1] = (mask & 0xFF);
    writeReg_IDF(REG_INT_ENB_MSB, res, 2);
}

/*
 * TEST from here:
 */
uint8_t MAX30101::getFIFO_WR_PTR(void)
{
    uint8_t data;
    readReg_IDF(REG_FIFO_WR_PTR, &data, 1);
    return(data);
}

void MAX30101::setFIFO_WR_PTR(uint8_t data)
{
	uint8_t curr = this->getFIFO_WR_PTR();
	curr = curr & FIFO_MASK;
	data = data | curr;
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_FIFO_WR_PTR, res, 1);
}

uint8_t MAX30101::getOVF_COUNTER(void)
{
    uint8_t data;
    readReg_IDF(REG_OVF_COUNTER, &data, 1);
    return(data);
}

void MAX30101::setOVF_COUNTER(uint8_t data)
{
	uint8_t curr = this->getOVF_COUNTER();
	curr = curr & FIFO_MASK;
	data = data | curr;
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_OVF_COUNTER, res, 1);
}

uint8_t MAX30101::getFIFO_RD_PTR(void)
{
    uint8_t data;
    readReg_IDF(REG_FIFO_RD_PTR, &data, 1) ;
    return(data);
}

void MAX30101::setFIFO_RD_PTR(uint8_t data)
{
	uint8_t curr = this->getFIFO_RD_PTR();
	curr = curr & FIFO_MASK;
	data = data | curr;
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_FIFO_RD_PTR, res, 1);
}

uint8_t MAX30101::getFIFO_DATA(void)
{
    uint8_t data;
    readReg_IDF(REG_FIFO_DATA, &data, 1) ;
    return(data);
}

void MAX30101::get_n_FIFO_DATA(uint8_t * data, size_t n){
	readReg_IDF(REG_FIFO_DATA, data, n);
}


void MAX30101::setFIFO_DATA(uint8_t data)
{
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_FIFO_DATA, res, 1);
}


uint8_t MAX30101::getFIFO_CONFIG(void)
{
	size_t size = 1;
	uint8_t data;
    readReg_IDF(REG_FIFO_CONFIG, &data, size);
    return(data);
}

void MAX30101::setFIFO_CONFIG(uint8_t data)
{
    uint8_t res[1] ;
    res[0] = data ;
    writeReg_IDF(REG_FIFO_CONFIG, res, 1) ;
}


uint8_t MAX30101::getMODE_CONFIG(void)
{
    uint8_t data;
    readReg_IDF(REG_MODE_CONFIG, &data, 1);
    return(data);
}

void MAX30101::setMODE_CONFIG(uint8_t data)
{
	uint8_t curr = this->getMODE_CONFIG();
	curr = curr & MODE_CONFIG_MASK;
	data = data | curr;
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_MODE_CONFIG, res, 1);
}

uint8_t MAX30101::getSPO2_CONFIG(void)
{
    uint8_t data;
    readReg_IDF(REG_SPO2_CONFIG, &data, 1);
    return(data);
}

void MAX30101::setSPO2_CONFIG(uint8_t data)
{
	uint8_t curr = this->getSPO2_CONFIG();
	curr = curr & SPO2_CONFIG_MASK;
	data = data | curr;
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_SPO2_CONFIG, res, 1);
}

uint8_t MAX30101::getLED1_PA(void)
{
    uint8_t data;
    readReg_IDF(REG_LED1_PA, &data, 1);
    return(data);
}

void MAX30101::setLED1_PA(uint8_t data)
{
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_LED1_PA, res, 1);
}

uint8_t MAX30101::getLED2_PA(void)
{
    uint8_t data;
    readReg_IDF(REG_LED2_PA, &data, 1);
    return(data);
}

void MAX30101::setLED2_PA(uint8_t data)
{
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_LED2_PA, res, 1);
}

uint8_t MAX30101::getLED3_PA(void)
{
    uint8_t data;
    readReg_IDF(REG_LED3_PA, &data, 1);
    return(data);
}

void MAX30101::setLED3_PA(uint8_t data)
{
    uint8_t res[1];
    res[0] =  data;
    writeReg_IDF(REG_LED3_PA, res, 1);
}

uint8_t MAX30101::getLED4_PA(void)
{
    uint8_t data;
    readReg_IDF(REG_LED4_PA, &data, 1);
    return(data);
}

void MAX30101::setLED4_PA(uint8_t data)
{
    uint8_t res[1];
    res[0] =  data;
    writeReg_IDF(REG_LED4_PA, res, 1);
}


uint16_t MAX30101::getSLOT(void)
{
    uint8_t res[2];
    uint16_t data;
    readReg_IDF(REG_SLOT_MSB, res, 2);
    data = (res[0] << 8) | res[1];
    return(data);
}

void MAX30101::setSLOT(uint16_t data)
{
	uint16_t curr = this->getSLOT();
	curr = curr | SLOT_MASK;
	data = data | curr;
    uint8_t res[2];
    res[0] = (data >> 8) & 0xFF;
    res[1] = data;
    writeReg_IDF(REG_SLOT_MSB, res, 2);
}

uint8_t MAX30101::getTEMP_INT(void)
{
    uint8_t data;
    readReg_IDF(REG_TEMP_INT, &data, 1);
    return(data);
}

uint8_t MAX30101::getTEMP_FRAC(void)
{
    uint8_t data;
    readReg_IDF(REG_TEMP_FRAC, &data, 1);
    return(data);
}

uint8_t  MAX30101::getTEMP_EN(void)
{
    uint8_t data;
    readReg_IDF(REG_TEMP_EN, &data, 1);
    return(data);
}

void  MAX30101::setTEMP_EN(void)
{
	uint8_t data = 1;
	uint8_t curr = this->getTEMP_EN();
	curr = curr & TEMP_EN_MASK;
	data = data | curr;
    uint8_t res[1];
    res[0] = data;
    writeReg_IDF(REG_TEMP_EN, res, 1);
}
