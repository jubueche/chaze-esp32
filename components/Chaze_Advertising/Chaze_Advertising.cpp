#include "Chaze_Advertising.h"
#include "Configuration.h"
#include "esp_log.h"

const char * TAG_Adv = "Chaze-Advertising";

Chaze_ble * ble = NULL;

volatile bool am_interrupt;
volatile bool rising_interrupt;
TaskHandle_t wifi_synch_task_handle = NULL;

void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(config.gpio_evt_queue, &gpio_num, NULL);
}

void gpio_bno_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(config.gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if(io_num == GPIO_BNO_INT_NUM){
				am_interrupt = true;
			} else{
				if(io_num == GPIO_BUTTON_NUM){
					rising_interrupt = true;
				}
			}
        }
    }
}

void advertise()
{
    
    // ESP_LOGE(TAG_Adv, "WARNING WARNING Deleteing all trainings.");
    // int num_trainings = global_ft->meta_total_number_of_trainings();
    // ESP_LOGI(TAG_Adv, "Number of trainings is %d", num_trainings);
    // for(int i=0;i<num_trainings;i++)
    // {
    //     if(global_ft->meta_get_training_size(i) == 262144 || global_ft->meta_get_training_size(i) == 0 || true)
    //     {
    //         global_ft->init_delete_training(i);
    //     }
    //     ESP_LOGI(TAG_Adv, "Size of training %d is %d. Synched?: %d Deleted?: %d" ,i,global_ft->meta_get_training_size(i),global_ft->meta_is_synced(i),global_ft->meta_is_deleted(i));
    // }


    if(config.wifi_connected)
    {
        if(ble != NULL)
        {
            ble->~Chaze_ble();
            ble = NULL;
        }
    }

    if(config.wifi_synch_semaphore == NULL)
	{
		config.wifi_synch_semaphore = xSemaphoreCreateMutex();
		xSemaphoreGive(config.wifi_synch_semaphore);
        if(DEBUG) ESP_LOGI(TAG_Adv, "Created wifi_synch_semaphore");
	}

    // Inilialize nvs
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG_Adv, "Attempting to erase flash!");
		if(nvs_flash_erase() == ESP_OK){
			if(nvs_flash_init() != ESP_OK){
				ESP_LOGE(TAG_Adv, "Failed to init NVS flash.");
				config.STATE = DEEPSLEEP;
                return;
			}
		} else{
			ESP_LOGE(TAG_Adv, "Failed to erase flash.");
            config.STATE = DEEPSLEEP;
            return;
		}
	}

    BNO055 bno_adv = BNO055();

    if(ble == NULL)
    {
        ble = new Chaze_ble();
        if(DEBUG) ESP_LOGI(TAG_Adv, "Created BLE object.");
        if(DEBUG) ESP_LOGI(TAG_Adv, "Not initialized. Starting BLE.");
        ble->initialize_connection();
    }

    if(DEBUG) ESP_LOGI(TAG_Adv, "Start advertising.");
    ble->advertise();

    am_interrupt = false;
    rising_interrupt = false;
    attach_am_interrupt(&bno_adv);
    config.attach_btn_int(&gpio_bno_task, &gpio_isr_handler);
    if(DEBUG) ESP_LOGI(TAG_Adv, "Attached Button interrupt");
    unsigned long blue_led_timer = millis();
    unsigned long timeout_timer = millis();

    if(wifi_synch_task_handle == NULL && global_ft->meta_number_of_unsynced_trainings() > 0)
    {
        if (xTaskCreate(&synch_via_wifi, "synch_via_wifi", 1024 * 8, NULL, 5, &wifi_synch_task_handle) != pdPASS )
            {
                ESP_LOGE(TAG_Adv, "Failed to create  wifi_synch-task.");
                config.STATE = DEEPSLEEP;
                return;
            }
    } else if (config.wifi_synch_task_suspended && global_ft->meta_number_of_unsynced_trainings() > 0)
    {
        if(DEBUG) ESP_LOGI(TAG_Adv, "Resuming the task");
        vTaskResume(wifi_synch_task_handle);
        config.wifi_synch_task_suspended = false;
    }


    while(config.STATE == ADVERTISING)
    {
        if(am_interrupt){
            printf("Interrupt.\n");
            am_interrupt = false;
            bno_adv.resetInterrupts();
            config.STATE = RECORD;
            clean_up(&bno_adv);
            gpio_reset_pin(GPIO_BNO_INT);
            break;
        }

        if(rising_interrupt){
			if(DEBUG) ESP_LOGI(TAG_Adv, "Interrupt");
			rising_interrupt = false;
			long timer = millis();

			while(gpio_get_level(GPIO_BUTTON))
			{
				if(DEBUG) ESP_LOGI(TAG_Adv, "Waiting for release...");
				if (millis() - timer > TIMEOUT_BUTTON_PUSHED_TO_ADVERTISING && millis() - timer < TIMEOUT_BUTTON_PUSHED_TO_OFF)
				{
                    gpio_set_level(GPIO_LED_GREEN, 1);
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    gpio_set_level(GPIO_LED_GREEN, 0);
				}
                if (millis() - timer > TIMEOUT_BUTTON_PUSHED_TO_OFF)
                {
                    gpio_set_level(GPIO_LED_RED, 1);
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    gpio_set_level(GPIO_LED_RED, 0);
                }
			}
            if(millis() - timer > TIMEOUT_BUTTON_PUSHED_TO_OFF)
            {
                config.STATE = DEEPSLEEP;
                clean_up(&bno_adv);
                gpio_set_level(GPIO_VIB, 1);
                vTaskDelay(80 / portTICK_PERIOD_MS);
                gpio_set_level(GPIO_VIB, 0);
                return;
            } else if (millis() - timer > TIMEOUT_BUTTON_PUSHED_TO_ADVERTISING)
            {
                config.STATE = RECORD;
                clean_up(&bno_adv);
                gpio_set_level(GPIO_VIB, 1);
                vTaskDelay(80 / portTICK_PERIOD_MS);
                gpio_set_level(GPIO_VIB, 0);
                return;
            }
		}

        if(config.ble_connected)
        {
            // Switch to connected state
            if(DEBUG) ESP_LOGI(TAG_Adv, "Connected");
            config.flicker_led(GPIO_BLUE);
            config.STATE = CONNECTED;
            config.detach_bno_int();
            config.detach_btn_int();
            return;
        }

        if(millis() - blue_led_timer > LED_BLUE_TIMEOUT){
            blue_led_timer = millis();
            gpio_set_level(GPIO_BLUE, 1);
            vTaskDelay(80 /portTICK_PERIOD_MS);
            gpio_set_level(GPIO_BLUE, 0);
        }

        if(millis() - timeout_timer > TIMEOUT_AFTER_ADVERTISING)
        {
            config.STATE = DEEPSLEEP;
            clean_up(&bno_adv);
            if(DEBUG) ESP_LOGI(TAG_Adv, "Advertising timeout. Going to sleep.");
            return;
        }

        if(config.wifi_connected)
        {
            if(DEBUG) ESP_LOGI(TAG_Adv, "WiFi is connected. Returning from advertise().");
            config.detach_bno_int();
            config.detach_btn_int();
            return;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


void clean_up(BNO055 *bno_adv)
{

    if(config.wifi_setup_once)
    {
        esp_err_t wifi_stop_res = esp_wifi_stop();
        if(wifi_stop_res == ESP_OK){
            if(esp_wifi_deinit() != ESP_OK){
                ESP_LOGE(TAG_Adv, "Failed to deinit WiFi");
            } else 	if(DEBUG) ESP_LOGI(TAG_Adv, "Deinited WiFi.");
        } else {
            ESP_LOGE(TAG_Adv, "Could not stop WiFi: %s", esp_err_to_name(wifi_stop_res));
        }
    }
    config.wifi_connected = false;

    // Should call destructors, detach interrupts
    config.detach_bno_int();
    config.detach_btn_int();
    ble->~Chaze_ble();
    bno_adv->~BNO055();
    nvs_flash_deinit();
}

void attach_am_interrupt(BNO055 *bno_adv)
{
    if(!bno_adv->begin())
    {
        ESP_LOGE(TAG_Adv, "Error initializing BNO055.");
    }
    bno_adv->enableAnyMotion(ANY_MOTION_THRESHOLD, ANY_MOTION_DURATION);
    bno_adv->enableInterruptsOnXYZ(ENABLE, ENABLE, ENABLE);
    bno_adv->setExtCrystalUse(true);

    // Initialize function pointers
    void (*task_pointer)(void *) = &gpio_bno_task;
    void (*isr_pointer) (void *) = &gpio_isr_handler;
    
    config.attach_bno_int(task_pointer, isr_pointer);

    if(DEBUG) ESP_LOGI(TAG_Adv, "Attached BNO interrupt.");

}