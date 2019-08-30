/*
//Compiling main.cpp file
#include "MS5837.h"

extern "C" void app_main()
{
	turn_on_main_circuit();

	MS5837 ms5837 = MS5837();
	ms5837.init();
	ms5837.setModel(MS5837::MS5837_02BA);
	ms5837.setFluidDensity(997);

	while(1){
		ms5837.read_vals();
		vTaskDelay(20 / portTICK_PERIOD_MS);
		printf("Pressure: %.2f\n", ms5837.pressure());
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

}

*/

#include "MS5837.h"
#include "math.h"

const uint8_t MS5837_ADDR = 0x76;
const uint8_t MS5837_RESET = 0x1E;
const uint8_t MS5837_ADC_READ = 0x00;
const uint8_t MS5837_PROM_READ = 0xA0;
const int MS5837_CONVERT_D1_8192 = 0x4A;
const int MS5837_CONVERT_D2_8192 = 0x5A;


const float MS5837::Pa = 100.0f;
const float MS5837::bar = 0.001f;
const float MS5837::mbar = 1.0f;

const uint8_t MS5837::MS5837_30BA = 0;
const uint8_t MS5837::MS5837_02BA = 1;

#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

MS5837::MS5837() {
	fluidDensity = 1029;
}

bool MS5837::init() {
	// Reset the MS5837, per datasheet
	port_num = I2C_NUM_0;

	esp_err_t err = (i2c_master_init_IDF(port_num));
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to initialize I2C connection.");
	}


	uint8_t t[1];
	t[0] = MS5837_RESET;
	err = write(t, 1);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to write to I2C.");
	}

	// Wait for reset to complete
	vTaskDelay(10 / portTICK_PERIOD_MS);


	// Read calibration values and CRC
	uint8_t tmp[2];
	for (uint8_t i=0;i<7;i++){
		this->readReg_IDF(MS5837_PROM_READ+i*2, tmp, 2);
		C[i] = ((uint16_t) tmp[0] << 8) | tmp[1];
	}

	// Verify that data is correct with CRC
	uint8_t crcRead = C[0] >> 12;
	uint8_t crcCalculated = crc4(C);

	if ( crcCalculated == crcRead ) {
		return true; // Initialization success
	}

	return false; // CRC fail
}

void MS5837::setModel(uint8_t model) {
	_model = model;
}

void MS5837::setFluidDensity(float density) {
	fluidDensity = density;
}

void MS5837::read_vals() {
	// Request D1 conversion
	uint8_t t[1];
	t[0] = MS5837_CONVERT_D1_8192;
	esp_err_t err = write(t, 1);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to write to I2C.");
	}

	vTaskDelay(20 / portTICK_PERIOD_MS); // Max conversion time per datasheet

	uint8_t tmp[3];
	readReg_IDF(MS5837_ADC_READ, tmp, 3);

	D1 = 0;
	D1 = tmp[0];
	D1 = (D1 << 8) | tmp[1];
	D1 = (D1 << 8) | tmp[2];

	// Request D2 conversion
	t[0] = MS5837_CONVERT_D2_8192;
	err = write(t, 1);
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to write to I2C.");
	}

	vTaskDelay(20 / portTICK_PERIOD_MS); // Max conversion time per datasheet

	readReg_IDF(MS5837_ADC_READ, tmp, 3);

	D2 = 0;
	D2 = tmp[0];
	D2 = (D2 << 8) | tmp[1];
	D2 = (D2 << 8) | tmp[2];

	calculate();
}

void MS5837::calculate() {
	// Given C1-C6 and D1, D2, calculated TEMP and P
	// Do conversion first and then second order temp compensation

	int32_t dT = 0;
	int64_t SENS = 0;
	int64_t OFF = 0;
	int32_t SENSi = 0;
	int32_t OFFi = 0;
	int32_t Ti = 0;
	int64_t OFF2 = 0;
	int64_t SENS2 = 0;

	// Terms called
	dT = D2-uint32_t(C[5])*256l;
	if ( _model == MS5837_02BA ) {
		SENS = int64_t(C[1])*65536l+(int64_t(C[3])*dT)/128l;
		OFF = int64_t(C[2])*131072l+(int64_t(C[4])*dT)/64l;
		P = (D1*SENS/(2097152l)-OFF)/(32768l);
	} else {
		SENS = int64_t(C[1])*32768l+(int64_t(C[3])*dT)/256l;
		OFF = int64_t(C[2])*65536l+(int64_t(C[4])*dT)/128l;
		P = (D1*SENS/(2097152l)-OFF)/(8192l);
	}

	// Temp conversion
	TEMP = 2000l+int64_t(dT)*C[6]/8388608LL;

	//Second order compensation
	if ( _model == MS5837_02BA ) {
		if((TEMP/100)<20){         //Low temp
			Ti = (11*int64_t(dT)*int64_t(dT))/(34359738368LL);
			OFFi = (31*(TEMP-2000)*(TEMP-2000))/8;
			SENSi = (63*(TEMP-2000)*(TEMP-2000))/32;
		}
	} else {
		if((TEMP/100)<20){         //Low temp
			Ti = (3*int64_t(dT)*int64_t(dT))/(8589934592LL);
			OFFi = (3*(TEMP-2000)*(TEMP-2000))/2;
			SENSi = (5*(TEMP-2000)*(TEMP-2000))/8;
			if((TEMP/100)<-15){    //Very low temp
				OFFi = OFFi+7*(TEMP+1500l)*(TEMP+1500l);
				SENSi = SENSi+4*(TEMP+1500l)*(TEMP+1500l);
			}
		}
		else if((TEMP/100)>=20){    //High temp
			Ti = 2*(dT*dT)/(137438953472LL);
			OFFi = (1*(TEMP-2000)*(TEMP-2000))/16;
			SENSi = 0;
		}
	}

	OFF2 = OFF-OFFi;           //Calculate pressure and temp second order
	SENS2 = SENS-SENSi;

	if ( _model == MS5837_02BA ) {
		TEMP = (TEMP-Ti);
		P = (((D1*SENS2)/2097152l-OFF2)/32768l)/100;
	} else {
		TEMP = (TEMP-Ti);
		P = (((D1*SENS2)/2097152l-OFF2)/8192l)/10;
	}
}

float MS5837::pressure(float conversion) {
	return P*conversion;
}

float MS5837::temperature() {
	return TEMP/100.0f;
}

float MS5837::depth() {
	return (pressure(MS5837::Pa)-101300)/(fluidDensity*9.80665);
}

float MS5837::altitude() {
	return (1-pow((pressure()/1013.25),.190284))*145366.45*.3048;
}


uint8_t MS5837::crc4(uint16_t n_prom[]) {
	uint16_t n_rem = 0;

	n_prom[0] = ((n_prom[0]) & 0x0FFF);
	n_prom[7] = 0;

	for ( uint8_t i = 0 ; i < 16; i++ ) {
		if ( i%2 == 1 ) {
			n_rem ^= (uint16_t)((n_prom[i>>1]) & 0x00FF);
		} else {
			n_rem ^= (uint16_t)(n_prom[i>>1] >> 8);
		}
		for ( uint8_t n_bit = 8 ; n_bit > 0 ; n_bit-- ) {
			if ( n_rem & 0x8000 ) {
				n_rem = (n_rem << 1) ^ 0x3000;
			} else {
				n_rem = (n_rem << 1);
			}
		}
	}

	n_rem = ((n_rem >> 12) & 0x000F);

	return n_rem ^ 0x00;
}


/**
 * @brief Write a package to the I2C bus.
 * @param data_wr Payload to write.
 * @param size Size of the payload.
 * @return
 */
esp_err_t MS5837::write(uint8_t * data_wr, size_t size){
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (MS5837_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
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
esp_err_t MS5837::read(uint8_t * data_rd, size_t size){
	if (size == 0) {
	        return ESP_OK;
	}
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (MS5837_ADDR << 1) | READ_BIT, ACK_CHECK_EN);
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
void MS5837::readReg_IDF(uint8_t reg, uint8_t * data, size_t size){
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
void MS5837::writeReg_IDF(uint8_t reg, uint8_t * data, size_t size){
	if(size < 1){
		return;
	}
	//Populate new array with the Register that should be written and the data that should be written.
	uint8_t t[size+1];
	t[0] = reg; //Jump to REG and write from there on. If we want to write only one register, data has only one entry.
	for(int i = 0;i<size;i++){
		t[i+1] = data[i];
	}
	esp_err_t err = write(t, size+1); //Write reg+data -> 1+size
	if(err != ESP_OK){
		ESP_LOGE(TAG, "Failed to write to I2C.");
	}
}
