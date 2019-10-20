#include "Chaze_Advertising.h"
#include "Configuration.h"
#include "esp_log.h"

const char * TAG_Adv = "Chaze-Advertising";

Chaze_ble * ble = NULL;

volatile bool am_interrupt;
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
            am_interrupt = true;
        }
    }
}

void advertise()
{

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
        ESP_LOGI(TAG_Adv, "Created wifi_synch_semaphore");
	}

    // Inilialize nvs
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
        ESP_LOGI(TAG_Adv, "Created BLE object.");

    }

    ESP_LOGI(TAG_Adv, "Not initialized. Starting BLE.");
    ble->initialize_connection();

    ESP_LOGI(TAG_Adv, "Start advertising.");
    ble->advertise();

    am_interrupt = false;
    attach_am_interrupt(&bno_adv);
    unsigned long blue_led_timer = millis();
    unsigned long timeout_timer = millis();

    if(wifi_synch_task_handle == NULL && global_ft->get_number_of_unsynched_trainings() > 0)
    {
        if (xTaskCreate(&synch_via_wifi, "synch_via_wifi", 1024 * 8, NULL, 5, &wifi_synch_task_handle) != pdPASS )
            {
                ESP_LOGE(TAG_Adv, "Failed to create  wifi_synch-task.");
                config.STATE = DEEPSLEEP;
                return;
            }
    } else if (config.wifi_synch_task_suspended && global_ft->get_number_of_unsynched_trainings() > 0)
    {
        ESP_LOGI(TAG_Adv, "Resuming the task");
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

        if(config.ble_connected)
        {
            // Switch to connected state
            ESP_LOGI(TAG_Adv, "Connected");
            config.flicker_led(GPIO_BLUE);
            config.STATE = CONNECTED;
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
            ESP_LOGI(TAG_Adv, "Advertising timeout. Going to sleep.");
            return;
        }

        if(config.wifi_connected)
        {
            ESP_LOGI(TAG_Adv, "WiFi is connected. Returning from advertise().");
            return;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


void clean_up(BNO055 *bno_adv)
{
    if(config.wifi_synch_semaphore == NULL)
    {
        ESP_LOGI(TAG_Adv, "Sem. is NULL");
    } else
    {
        for(;;)
        {
            // Wait until wifi_synch task terminated. If this task terminated. The other must have terminated.
            if(xSemaphoreTake(config.wifi_synch_semaphore, pdMS_TO_TICKS(1000000)) == pdTRUE)
            {
                ESP_LOGI(TAG_Adv, "Took the WiFi semaphore. WiFi-Synch task terminated.");
                // Suspend the task
                vTaskSuspend(wifi_synch_task_handle);
                config.wifi_synch_task_suspended = true;
                break;
            }
            vTaskDelay(80);
        }   
    }

    // Should call destructors, detach interrupts
    config.detach_bno_int();
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

    ESP_LOGI(TAG_Adv, "Attached BNO interrupt.");

}