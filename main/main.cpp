#include "MAX30101.h"
#include "Configuration.h"
#include "time.h"

QueueHandle_t queueDC;
QueueHandle_t queueRead;


/*
 * Timing
 */
// 	uint64_t t1 = (uint64_t) esp_timer_get_time();
//  printf("%lld \n", t2-t1);
//  read_fifo(data, n);

struct array_message {
	int32_t data[32];
} array_message ;

struct butter_message {
	float data[32];
} butter_message ;


void median_filter(void * pvParameters)
{
	uint8_t n = 32;
	struct butter_message * message = &butter_message;
	    for( ;; )
	    {
	    	 if(xQueueReceive( queueDC, &( message ), ( TickType_t ) 10 ) )
			{
	    		 if(message->data != NULL){
					for(int i=0;i<n;i++){
						printf("%.02f\n", message->data[i]);
					}
	    		 }
			}
	    }
}


void dc_remove( void * pvParameters )
{
    int32_t num_samples_total = ( int32_t ) pvParameters;
    uint8_t n = 32;
    float y[32];
    float alpha = 0.90;
    float w_n = 1117667;
    float w_old = w_n;
    float butter_out[32];
    float _min = 0;
    float _max = 0;
    float butter_b = 0.124700013610146; //Corresponds to 88.8 Hz, fc = 4Hz, calc using butter(1,fc/(fs/2)), fc = cutoff freq, fs = sampl. freq
    float butter_a = 0.750599972779708;

    struct array_message * message = &array_message;
    for( ;; )
    {
    	 if(xQueueReceive( queueRead, &( message ), ( TickType_t ) 10 ) )
		{
    		 if(message->data != NULL){
    			 y[0] = w_n-w_old;
    			 butter_out[0] = y[0];
				for(int i=1;i<n;i++){
					w_old = w_n;
					w_n = (float) message->data[i] + alpha*w_old;
					y[i] = w_n-w_old;


					if(i > 1){
						//Apply the median filter with size 3
						_min = min(y[i-2],min(y[i-1],y[i]));
						_max = max(y[i-2],max(y[i-1],y[i]));
						y[i-1] = (y[i-2]+y[i-1]+y[i])- _min - _max;

						//Apply butterworth filter
						butter_out[i-1] =  (butter_b * (float) y[i-1]) + (butter_a * butter_out[i-2]);
					}
				}

				_min = min(y[n-3],min(y[n-2],y[n-1]));
				_max = max(y[n-3],max(y[n-2],y[n-1]));
				y[n-1] = (y[n-3]+y[n-2]+y[n-1])- _min - _max;
				butter_out[n-1] =  (butter_b * (float) y[n-1]) + (butter_a * butter_out[n-2]);

				struct butter_message *message_out;
				message_out = &butter_message;
				memcpy(message_out->data, butter_out, 32*sizeof(float));

				//Write data to QueueRead
				if(xQueueSend( queueDC,
						&message_out,
						( TickType_t ) 10 ) != pdPASS){
					printf("Failed to push to Queue.");
				}
    		 }
		}
    	vTaskDelay(30);
    }
}


extern "C" void app_main()
{

	/* TODO:
	 * - Implement read_hr()
	 * - Implement read_spo2()
	 * - Implement temperature compensation -> Change read_n_* functions to record the temp when triggering interrupt.
	 * - Safe error handling
	 */
	MAX30101 max30101(I2C_NUM_1);


	/*IMPORTANT: Check Table 11 and 12 if SR vs. PW is ok. @ https://datasheets.maximintegrated.com/en/ds/MAX30101.pdf */
	maxim_config_t ex1 = {.PA1 = 0x0A, .PA2 = 0x1F, .PA3 = 0x1F, .PA4 = 0x1F,
							  .MODE = HEART_RATE_MODE, .SMP_AVE = SMP_AVE_2, .FIFO_ROLL = FIFO_ROLL_DIS, .FIFO_A_FULL = FIFO_A_FULL_0,
							  .SPO2_ADC_RGE = ADC_16384, .SPO2_SR = SR_800, .LED_PW = PW_411};

	uint8_t n = 32;
	uint8_t rounds = 28;
	max30101.init(&ex1);

	//Dont forget to free
	uint32_t red[32];

	uint32_t num_samples = n*rounds;

	queueRead = xQueueCreate(rounds, sizeof(struct array_message *));
	queueDC = xQueueCreate(rounds, sizeof(struct array_message *));


	//Create task
	BaseType_t xReturned;
	TaskHandle_t dc_remov_handle = NULL;
	TaskHandle_t median_filter_handle = NULL;

	/* Create the task, storing the handle. */
	xReturned = xTaskCreate(
					dc_remove,       /* Function that implements the task. */
					"dc_remov",          /* Text name for the task. */
					8*1024,      /* Stack size in words, not bytes. */
					( void * ) num_samples,    /* Parameter passed into the task. */
					tskIDLE_PRIORITY,/* Priority at which the task is created. */
					&dc_remov_handle );      /* Used to pass out the created task's handle. */

	xReturned = xTaskCreate(
							median_filter,
							"median_filter",
							8*1024,
							(void*) num_samples,
							tskIDLE_PRIORITY,
							&median_filter_handle);


	while(1){
		printf("Start writing to Queue in 1s...\n");
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		//Corresponds to 15*32 = 480 ~9.40s at 50 Hz sampling frequency
		uint64_t t1 = (uint64_t) esp_timer_get_time();

		for(int i=0;i<rounds;i++){


			max30101.read_n(red, 32);



			struct array_message *message;
			message = &array_message;
			memcpy(message->data, red, 32*sizeof(uint32_t));

			//Write data to QueueRead
			if(xQueueSend( queueRead,
					&message,
					( TickType_t ) 10 ) != pdPASS){
				printf("Failed to push to Queue.");
			}

		}
		uint64_t t2 = (uint64_t) esp_timer_get_time();
		vTaskDelay(20);
		printf("%lld \n", t2-t1);

		while(1){
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}
		//Put the data in the first Queue
		for(int i=0;i<32;i++){
			printf("%d\n",red[i]);
		}
		//vTaskDelay(30);
	}

}





