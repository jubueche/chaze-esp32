#include <stdio.h>
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "string.h"


#include "MAX30101.h"
#include "Configuration.h"

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



#define RESET	 0x40 //B(01000000)
#define SHUTDOWN 0x80 //B(10000000)
#define RESUME	 0x00

const char * TAG = "Chaze";

static xQueueHandle gpio_evt_queue = NULL;

MAX30101::MAX30101(){ }
/*
 * @brief
 * Constructor of the class.
 * @params: Source data (SDA), Source Clock (SCK), address of the chip.
 */
MAX30101::MAX30101(maxim_config_t * args) {

	this->port_num = I2C_NUM_1;

	ESP_ERROR_CHECK(this->i2c_master_init_IDF());

	this->reset();
	this->setMODE_CONFIG(args->MODE);

	uint8_t fifo_config = (args->SMP_AVE) | (args->FIFO_ROLL) | (FIFO_A_FULL_0);
	this->setFIFO_CONFIG(fifo_config);

	//Set pulse amplitude of every LED
	this->setLED1_PA(args->PA1);
	this->setLED2_PA(args->PA2);
	this->setLED3_PA(args->PA3);
	this->setLED4_PA(args->PA4);

	set_slots();

	//Set LED pulse width
	uint8_t spo2_config = (args->SPO2_ADC_RGE | args->SPO2_SR | args->LED_PW);
	this->setSPO2_CONFIG(spo2_config);

	this->reset_FIFO();

}


//Report the most recent red value
uint32_t MAX30101::getRed(void)
{
  //Check the sensor for new data for 250ms
  if(this->safeCheck(250000))
    return (this->cbuff.red[this->cbuff.head]);
  else
    return(0); //Sensor failed to find new data
}

//Report the most recent IR value
uint32_t MAX30101::getIR(void)
{
  //Check the sensor for new data for 250ms
  if(this->safeCheck(250000))
    return (this->cbuff.IR[this->cbuff.head]);
  else
    return(0); //Sensor failed to find new data
}

//Report the most recent Green value
uint32_t MAX30101::getGreen(void)
{
  //Check the sensor for new data for 250ms = 250000 us
  if(this->safeCheck(250000))
    return (this->cbuff.green[this->cbuff.head]);
  else
    return(0); //Sensor failed to find new data
}

//Report the next Red value in the FIFO
uint32_t MAX30101::getFIFORed(void)
{
  return (this->cbuff.red[this->cbuff.tail]);
}

//Report the next IR value in the FIFO
uint32_t MAX30101::getFIFOIR(void)
{
  return (this->cbuff.IR[this->cbuff.tail]);
}

//Report the next Green value in the FIFO
uint32_t MAX30101::getFIFOGreen(void)
{
  return (this->cbuff.green[this->cbuff.tail]);
}

//Advance the tail
void MAX30101::nextSample(void)
{
  if(available()) //Only advance the tail if new data is available
  {
	  this->cbuff.tail++;
	  this->cbuff.tail %= STORAGE_SIZE; //Wrap condition
  }
}

uint8_t MAX30101::available(void)
{
  int8_t numberOfSamples = this->cbuff.head - this->cbuff.tail;
  if (numberOfSamples < 0) numberOfSamples += STORAGE_SIZE;

  return numberOfSamples;
}

bool MAX30101::safeCheck(uint64_t maxTimeToCheck)
{
	uint64_t t1 = (uint64_t) esp_timer_get_time();
	uint64_t t2;
	uint16_t ret;
  while(1)
  {
	t2 = (uint64_t) esp_timer_get_time();
	if(t2 - t1 > maxTimeToCheck){
		return false;
	}
	ret = this->check();
	if(ret != 0){
		return true;
	}
	vTaskDelay(1);
	}
}


uint16_t MAX30101::check(void)
{
  //Read register FIDO_DATA in (3-byte * number of active LED) chunks
  //Until FIFO_RD_PTR = FIFO_WR_PTR

  int8_t readPointer = this->getFIFO_RD_PTR();
  int8_t writePointer = this->getFIFO_WR_PTR();
  //printf("RD: %d WR: %d\n", readPointer, writePointer);
  uint8_t active_leds;

  if(this->get_mode() == HEART_RATE_MODE){
	  active_leds = 1;
  } else if(this->get_mode() == SPO2_MODE){
	  active_leds = 2;
  } else{
	  active_leds = 3;
  }

  int32_t numberOfSamples = 0;

  //Do we have new data?
  if (readPointer != writePointer)
  {
    //Calculate the number of readings we need to get from sensor
    numberOfSamples = writePointer - readPointer;
    if (numberOfSamples < 0) numberOfSamples += FIFO_DEPTH; //Wrap condition

    //We now have the number of readings, now calc bytes to read
    int32_t bytesLeftToRead = numberOfSamples * active_leds * 3;
    int32_t counter = 0;
    uint8_t tmp[bytesLeftToRead];

    this->get_n_FIFO_DATA(tmp, bytesLeftToRead);

    while(counter < bytesLeftToRead)
    {

    	this->cbuff.head++; //Advance the head of the storage struct
    	this->cbuff.head %= STORAGE_SIZE; //Wrap condition

        uint8_t temp[sizeof(uint32_t)]; //Array of 4 bytes that we will convert into long
        uint32_t tempLong;

        //Burst read three bytes - RED
        temp[3] = 0;
        temp[2] = tmp[counter];
        temp[1] = tmp[counter+1];
        temp[0] = tmp[counter+2];

        //Convert array to long
        memcpy(&tempLong, temp, sizeof(tempLong));

		tempLong &= 0x3FFFF; //Zero out all but 18 bits
		//printf("%d\n", tempLong);

		this->cbuff.red[this->cbuff.head] = tempLong; //Store this reading into the cbuff array
        counter += 3;

        if (active_leds > 1)
        {
          //Burst read three more bytes - IR
        	temp[3] = 0;
			temp[2] = tmp[counter];
			temp[1] = tmp[counter+1];
			temp[0] = tmp[counter+2];

          //Convert array to long
          memcpy(&tempLong, temp, sizeof(tempLong));

		  tempLong &= 0x3FFFF; //Zero out all but 18 bits

		  this->cbuff.IR[this->cbuff.head] = tempLong;
		  counter += 3;
        }

        if (active_leds > 2)
        {
          //Burst read three more bytes - Green
        	temp[3] = 0;
			temp[2] = tmp[counter];
			temp[1] = tmp[counter+1];
			temp[0] = tmp[counter+2];

          //Convert array to long
          memcpy(&tempLong, temp, sizeof(tempLong));

		  tempLong &= 0x3FFFF; //Zero out all but 18 bits

		  this->cbuff.green[this->cbuff.head] = tempLong;
          counter += 3;
        }

      }

  } //End if readPtr != writePtr

  return numberOfSamples;
}

uint8_t MAX30101::get_mode(void){
	return this->getMODE_CONFIG() & 0x07;
}


void MAX30101::reset_FIFO(void){
	this->setFIFO_RD_PTR(0);
	this->setFIFO_WR_PTR(0);
}

/*
 * Need to set mode before calling this function.
 */
void MAX30101::set_slots(void){
	uint16_t HR_SLOT_CONFIG = ((uint16_t)SLOT_NONE << 12) | ((uint16_t)SLOT_RED << 8) | ((uint16_t)SLOT_NONE << 4) | SLOT_NONE;
	uint16_t SPO2_SLOT_CONFIG = ((uint16_t)SLOT_IR << 12) | ((uint16_t)SLOT_RED << 8) | ((uint16_t)SLOT_NONE << 4) | SLOT_NONE;
	uint16_t MULTI_SLOT_CONFIG = ((uint16_t)SLOT_IR << 12) | ((uint16_t)SLOT_RED << 8) | ((uint16_t)SLOT_NONE << 4) | SLOT_GREEN;

	if(this->get_mode() == HEART_RATE_MODE){
		this->setSLOT(HR_SLOT_CONFIG);
	} else if(this->get_mode() == SPO2_MODE){
		this->setSLOT(SPO2_SLOT_CONFIG);
	} else{
		this->setSLOT(MULTI_SLOT_CONFIG);
	}
}

esp_err_t MAX30101::i2c_master_init_IDF(void)
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

void MAX30101::reset(void){
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



/*
    uint64_t t1, t2;

 	while(1){

	uint8_t samplesTaken = 0;
	t1 = (uint64_t) esp_timer_get_time();

	while(samplesTaken < 10)
	{
	t1 = (uint64_t) esp_timer_get_time();
	max30101.check(); //Check the sensor, read up to 3 samples
	t2 = (uint64_t) esp_timer_get_time();
		while (max30101.available()) //do we have new data?
		{
		  samplesTaken++;
		  max30101.getFIFORed();
		  max30101.nextSample(); //We're finished with this sample so move to next sample
		}
	}

	t2 = (uint64_t) esp_timer_get_time();
	printf("samples[%d]\n Hz [%.02f]", samplesTaken, (float)samplesTaken / ((t2 - t1) / 1000000.0));
	}


 */
