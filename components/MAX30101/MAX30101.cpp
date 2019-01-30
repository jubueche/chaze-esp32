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

const char * TAG = "Chaze-MAX30101";

/**
 * @brief Default constructor.
 */
MAX30101::MAX30101(){ }

/**
 * @brief Constructor.
 * @param args Holding the configuration information.
 */
MAX30101::MAX30101(maxim_config_t * args) {

	this->port_num = I2C_NUM_1;

	esp_err_t err = (this->i2c_master_init_IDF());
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to initialize I2C connection.");
	}

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
	this->getIntStatus();

}

/**
 * @brief Report the most recent red value.
 * @return Most recent red value.
 */
uint32_t MAX30101::getRed(void)
{
  //Check the sensor for new data for 250ms
  if(this->safeCheck(250000))
    return (this->cbuff.red[this->cbuff.head]);
  else
    return(0); //Sensor failed to find new data
}

/**
 * @brief Report the most recent IR value.
 * @return Most recent IR value.
 */
uint32_t MAX30101::getIR(void)
{
  //Check the sensor for new data for 250ms
  if(this->safeCheck(250000))
    return (this->cbuff.IR[this->cbuff.head]);
  else
    return(0); //Sensor failed to find new data
}

/**
 * @brief Report the most recent green value.
 * @return Most recent green value.
 */
uint32_t MAX30101::getGreen(void)
{
  //Check the sensor for new data for 250ms = 250000 us
  if(this->safeCheck(250000))
    return (this->cbuff.green[this->cbuff.head]);
  else
    return(0); //Sensor failed to find new data
}

/**
 * @brief Get the element, which was recently added to the circular buffer.
 * @return Red value.
 */
uint32_t MAX30101::getFIFORed(void)
{
  return (this->cbuff.red[this->cbuff.tail]);
}

/**
 * @brief Get the element, which was recently added to the circular buffer.
 * @return IR value.
 */
uint32_t MAX30101::getFIFOIR(void)
{
  return (this->cbuff.IR[this->cbuff.tail]);
}

/**
 * @brief Get the element, which was recently added to the circular buffer.
 * @return Green value.
 */
uint32_t MAX30101::getFIFOGreen(void)
{
  return (this->cbuff.green[this->cbuff.tail]);
}

/**
 * @brief If we have just read a value from the circular buffer, we have to call nextSample().
 * This advances ths tail pointer only if there is still data in the buffer.
 */
void MAX30101::nextSample(void)
{
  if(available()) //Only advance the tail if new data is available
  {
	  this->cbuff.tail++;
	  this->cbuff.tail %= STORAGE_SIZE; //Wrap condition
  }
}

/**
 * @brief Checks whether there is unread data in the circular buffer.
 * @return Number of available samples in the buffer.
 */
uint8_t MAX30101::available(void)
{
  int8_t numberOfSamples = this->cbuff.head - this->cbuff.tail;
  if (numberOfSamples < 0) numberOfSamples += STORAGE_SIZE;

  return numberOfSamples;
}

/**
 * @brief Checks for a given amount of time if there is new data in the FIFO of the sensor.
 * @param maxTimeToCheck Parameter to avoid looping forever if the sensor is not responding. Typically set to 250ms.
 * @return Boolean indicating success or failure.
 */
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

/**
 * @brief Checks if new data is available in the FIFO and, depending on the mode, reads numberOfSamples*numLED's*3 bytes
 * and writes them to the circular buffer. It also advances the head pointer of the circ. buffer.
 * @return Number of samples read. Remember, one sample comprises either 3,6 or 9 bytes depending on the number of LED's.
 */
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

/**
 *
 * @return Current mode.
 */
uint8_t MAX30101::get_mode(void){
	return this->getMODE_CONFIG() & 0x07;
}

/**
 * @brief Resets the pointers of the FIFO. Called at startup.
 */
void MAX30101::reset_FIFO(void){
	this->setFIFO_RD_PTR(0);
	this->setFIFO_WR_PTR(0);
}

/**
 * @brief It is important to set the mode prior to setting the slots.The slots determine in which order on sample
 * is filled with data. The order of the slots bitwise is: Slot2 - Slot1 - Slot 4 - Slot3.
 */
void MAX30101::set_slots(void){
	uint16_t HR_SLOT_CONFIG = ((uint16_t)SLOT_NONE << 12) | ((uint16_t)SLOT_IR << 8) | ((uint16_t)SLOT_NONE << 4) | SLOT_NONE;
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
/**
 * @brief Initialize I2C.
 * @return Error message or ESP_OK.
 */
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

/**
 * @brief Write a package to the I2C bus.
 * @param data_wr Payload to write.
 * @param size Size of the payload.
 * @return
 */
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

/**
 * @brief Reads from the MAX30101.
 * @param data_rd Pointer to array to be filled.
 * @param size Number of uint8_t's to read.
 * @return Error or ESP_OK.
 */
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

/**
 * @brief Read certain register from the MAX30101.
 * @param reg Register address.
 * @param data Data to hold the read data
 * @param size Number of uint8_t's expected.
 */
void MAX30101::readReg_IDF(uint8_t reg, uint8_t * data, size_t size){
	if(size < 1){
		return;
	}
	uint8_t t[1] = {reg};
	esp_err_t err = write(t, 1);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to write to I2C");
	}
	vTaskDelay(30 / portTICK_RATE_MS);
	err = read(data,size);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to read from I2C");

	}
}

/**
 * @brief Writes to a register.
 * @param reg Address of the register.
 * @param data Data to write in uint8_t.
 * @param size Number of uin8_t's to write.
 */
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
	esp_err_t err = write(t, size+1); //Write reg+data -> 1+size
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to write to I2C.");
	}
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

int32_t MAX30101::get_sampling_freq(void){
	uint8_t spo2_config = this->getSPO2_CONFIG();
	uint8_t sr = spo2_config & 0x1C;
	uint8_t averaging = this->getFIFO_CONFIG() &  0xE0;
	float factor;
	if(averaging == SMP_AVE_NO){
		factor = 1;
	} else if(averaging == SMP_AVE_2){
		factor = 2;
	} else if(averaging == SMP_AVE_4){
		factor = 4;
	} else if(averaging == SMP_AVE_8){
		factor = 8;
	} else if(averaging == SMP_AVE_16){
		factor = 16;
	} else{
		factor = 32;
	}
	if(sr == SR_100){
		return (int32_t) 100/factor;
	} else if(sr == SR_200){
		return (int32_t) 200/factor;
	} else if(sr == SR_400){
		return (int32_t) 400/factor;
	} else if(sr == SR_800){
		return (int32_t) 800/factor;
	} else if(sr == SR_1600){
		return (int32_t) 1600/factor;
	} else{
		return (int32_t) 3200/factor;
	}
}


