#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include "time.h"

/*
 * Section: GPIO
 */

#define GPIO_MAIN_CIRCUIT 26
#define GPIO_OUTPUT_MC  (1ULL<<GPIO_MAIN_CIRCUIT)

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


#define B(x) S_to_binary_(#x)
#define I2B(x) to_bin_string(x)



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
