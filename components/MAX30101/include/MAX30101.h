#ifndef _MAX30101_H_
#define _MAX30101_H_

#include "driver/i2c.h"
#include "Wire.h"
#include "Configuration.h"

/**
 * MAX30101
 * High-Sensitivity Pulse Oximeter and
 * Heart-Rate Sensor for Wearable Health
 */

#include "stdint.h"

#define FIFO_DEPTH 32

/*
 * Averaging:
 */
#define SMP_AVE_NO 0x00  //B(00000000)
#define SMP_AVE_2  0x20  //B(00100000)
#define SMP_AVE_4  0x40  //B(01000000)
#define SMP_AVE_8  0x60  //B(01100000)
#define SMP_AVE_16 0x80  //B(10000000)
#define SMP_AVE_32 0xA0  //B(10100000)


#define FIFO_ROLL_EN  0x10 //B(00010000)
#define FIFO_ROLL_DIS 0x00 //B(00000000)

#define FIFO_A_FULL_0 0x00
#define FIFO_A_FULL_1 0x01
#define FIFO_A_FULL_2 0x02
#define FIFO_A_FULL_3 0x03
#define FIFO_A_FULL_4 0x04
#define FIFO_A_FULL_5 0x05

#define INT_PIN 15

/*
 * LED Pulse Widths. See table 7
 */
#define PW_69  0x00 //B(00000000) ADC Res.: 15
#define PW_118 0x01 //B(00000001) ADC Res.: 16
#define PW_215 0x02 //B(00000010) ADC Res.: 17
#define PW_411 0x03 //B(00000011) ADC Res.: 18

/*
 * Peak Amplitudes (current) of LED's. Number indicates mA. E.g. 0_2 = 0.2mA
 * For more information see Datasheet, Table 8.
 */
#define PA_0_0  0x00 //B(00000000)
#define PA_0_2  0x01 //B(00000001)
#define PA_3_0  0x0F //B(00001111)
#define PA_6_2  0x1F //B(00011111)
#define PA_12_6 0x3F //B(00111111)
#define PA_25_4 0x7F //B(01111111)
#define PA_51   0xFF //B(11111111)

/*
 * Slots. See
 */
#define SLOT_NONE  0x0 //B(0000)
#define SLOT_RED   0x1 //B(0001)
#define SLOT_IR    0x2 //B(0010)
#define SLOT_GREEN 0x3 //B(0011)

/*
 * Sample Rates. See Table 11 and 12 for SR vs. Pulse Width for allowed combinations
 */
#define SR_50   0x00 //B(00000000)
#define SR_100  0x04 //B(00000100)
#define SR_200  0x08 //B(00001000)
#define SR_400  0x0C //B(00001100)
#define SR_800  0x10 //B(00010000)
#define SR_1000 0x14 //B(00010100)
#define SR_1600 0x18 //B(00011000)
#define SR_3200 0x1C //B(00011100)

/*
 * ADC scales
 */
#define ADC_2048  0x00 //B(00000000)
#define ADC_4096  0x20 //B(00100000)
#define ADC_8192  0x40 //B(01000000)
#define ADC_16384 0x60 //B(01100000)

/*
 * Interrupt flags.
 */
#define INT_A_FULL_EN       0x8000 //B(1000000000000000)
#define INT_PPG_RDY_EN      0x4000 //B(0100000000000000)
#define INT_ALC_OVF_EN      0x2000 //B(0010000000000000)
#define INT_DIE_TEMP_RDY_EN 0x0002 //B(0000000000000010)

/*
 * Interrupt status masks
 */
#define INT_ST_A_FULL       0x8000 /* FIFO Almost Full Flag */
#define INT_ST_PPG_RGY      0x4000 /* New FIFO Data Ready */
#define INT_ST_ALC_OVF      0x2000 /* Ambient Light Cancellation Overflow */
#define INT_ST_PWR_RDY      0x0100 /* Power Ready Flag */
#define INT_ST_DIE_TEMP_RDY 0x0002 /* Internal Temperature Ready Flag */

/*
 * Modes
 */
#define HEART_RATE_MODE 0x02 //B(010)
#define MULTI_LED_MODE  0x07 //B(111)
#define SPO2_MODE       0x03 //B(011)

/*
 * Interrupt flag set timeout.
 */
#define INT_TIMEOUT 100

//Hashtag perfect memory allocation :)
typedef struct {
	uint8_t PA1;
	uint8_t PA2;
	uint8_t PA3;
	uint8_t PA4;
	uint8_t MODE;
	uint8_t SMP_AVE;
	uint8_t FIFO_ROLL;
	uint8_t FIFO_A_FULL;
	uint8_t SPO2_ADC_RGE;
	uint8_t SPO2_SR;
	uint8_t LED_PW;
} maxim_config_t;




class MAX30101
{
public:
	//7-bit slave address
	static const uint8_t MAXIM_I2C_ADRS   = 0x57;
	//8-bit write address
	static const uint8_t MAXIM_I2C_W_ADRS = 0xAE;
	//8-bit read address
	static const uint8_t MAXIM_I2C_R_ADRS = 0xAF;
	//Max # Bytes in FIFO
	static const uint16_t MAXIM_FIFO_BYTES = 288;
	//# of bytes per LED channel
	static const uint8_t MAXIM_BYTES_PER_CH = 3;


	/*
	 * Fields:
	 */
	i2c_port_t port_num;
	TwoWire MAXI2C;
	gpio_num_t max30101_gpio_num_t_INT_PIN = GPIO_NUM_15;
	volatile bool fifo_full_interrupt;
	volatile bool fifo_newdata_interrupt;
	uint8_t mode;
	uint8_t adc_shift;
	float temp;
	/*
	 * Constructor:
	 */
	MAX30101(i2c_port_t);

	/*
	 * Helper functions:
	 */
	void readReg(uint8_t, uint8_t *, size_t);
	void readReg_IDF(uint8_t, uint8_t *, size_t);
	void writeReg_IDF(uint8_t, uint8_t *, size_t);
	esp_err_t i2c_master_init_IDF(void);
	esp_err_t read(uint8_t *, size_t);
	esp_err_t write(uint8_t *, size_t);

	/*
	 * Relevant class functions:
	 */

	//Board information:
	uint8_t readID(void);
	uint8_t revID(void);

	//Interrupts:
	uint16_t getIntStatus(void);
	uint8_t getIntStatus1(void);
	uint16_t getIntEnable(void);
	void setIntEnable(uint16_t);

	//FIFO Overflow:
	uint8_t getOVF_COUNTER(void);
	void setOVF_COUNTER(uint8_t);

	//FIFO:
	uint8_t getFIFO_WR_PTR(void);
	void setFIFO_WR_PTR(uint8_t);
	uint8_t getFIFO_RD_PTR(void);
	void setFIFO_RD_PTR(uint8_t);
	uint8_t getFIFO_DATA(void);
	void setFIFO_DATA(uint8_t);
	void get_n_FIFO_DATA(uint8_t*,size_t);
	uint8_t getFIFO_CONFIG(void);
	void setFIFO_CONFIG(uint8_t);

	//Op-Modes:
	uint8_t getMODE_CONFIG(void);
	void setMODE_CONFIG(uint8_t);

	//SPO2 sensor:
	uint8_t getSPO2_CONFIG(void);
	void setSPO2_CONFIG(uint8_t);

	//LED's:
	uint8_t getLED1_PA(void);
	void setLED1_PA(uint8_t);
	uint8_t getLED2_PA(void);
	void setLED2_PA(uint8_t);
	uint8_t getLED3_PA(void);
	void setLED3_PA(uint8_t);
	uint8_t getLED4_PA(void);
	void setLED4_PA(uint8_t);

	uint16_t getSLOT(void);
	void setSLOT(uint16_t);

	uint8_t getTEMP_INT(void);
	uint8_t getTEMP_FRAC(void);
	uint8_t getTEMP_EN(void);
	void setTEMP_EN(void);

	void reset(void);
	void init(maxim_config_t *);
	void set_slots(void);
	void read_fifo(uint32_t *, uint8_t);
	void read_n(uint32_t *, uint8_t);
	void get_values(uint32_t *, uint8_t, uint8_t *, uint8_t);

	void read_n_hr(uint32_t *, uint8_t);
	void read_n_hr(uint32_t *, uint8_t, maxim_config_t *);
	void read_n_spo2(uint32_t *, uint32_t *, uint8_t);
	void read_n_spo2(uint32_t *, uint32_t *, uint8_t, maxim_config_t *);
	void read_n_multi(uint32_t *, uint32_t *, uint32_t *, uint8_t);
	void read_n_multi(uint32_t *, uint32_t *, uint32_t *, uint8_t, maxim_config_t *);
	float get_temp(void);

	void shutdown(void);
	void resume(void);
	void init_interrupt(void);
};


#endif /* _MAX30101_H_ */
