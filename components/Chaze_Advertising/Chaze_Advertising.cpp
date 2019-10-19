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

    if(config.wifi_connected)
    {
        //Deallocate ble if not already deallocated
        if(ble != NULL)
        {
            ble->~Chaze_ble();
            ble = NULL;
        }
        //! Add a timer and deinit wifi if timer elapses for safety
        while (config.wifi_connected)
        {
            ESP_LOGI(TAG_Adv, "Waiting for WiFi to be done.");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    if(config.wifi_synch_semaphore != NULL)
    {
        // wifi_synch task has been created and we need to try and obtain the lock
        for(;;)
        {
            if(xSemaphoreTake(config.wifi_synch_semaphore, 300) == pdTRUE){
                ESP_LOGI(TAG_Adv, "Acquired wifi_synch_semaphore.");
                break;
            }
            vTaskDelay(80 / portTICK_PERIOD_MS);
        }
    }
    // else: We can proceed normally, creating the BLE object and spawning the task


    BNO055 bno_adv = BNO055();

    if(ble == NULL)
    {
        ESP_LOGI(TAG_Adv, "Creating BLE object.");
        ble = new Chaze_ble();
    }

    // Setup BLE
    if(!config.ble_old_device_connected)
    {
        ESP_LOGI(TAG_Adv, "Starting BLE.");
        ble->initialize_connection();
        config.ble_old_device_connected = true;
    }

    ESP_LOGI(TAG_Adv, "Start advertising.");
    ble->advertise();

    am_interrupt = false;
    attach_am_interrupt(&bno_adv);
    unsigned long blue_led_timer = millis();
    unsigned long timeout_timer = millis();

    if(wifi_synch_task_handle == NULL)
    {
        if (xTaskCreate(&synch_via_wifi, "synch_via_wifi", 1024 * 8, NULL, 5, &wifi_synch_task_handle) != pdPASS )
            {
                ESP_LOGE(TAG_Adv, "Failed to create  wifi_synch-task.");
                config.STATE = DEEPSLEEP;
                config.ble_old_device_connected = false;
                return;
            }
    } else {
        ESP_LOGI(TAG_Adv, "Synch via WiFi task already running.");
        if(config.wifi_synch_task_suspended)
        {
            vTaskResume(wifi_synch_task_handle);
            config.wifi_synch_task_suspended = false;
        }
    }

    while(config.STATE == ADVERTISING && !config.wifi_connected)
    {
        ESP_LOGI(TAG_Adv, "WiFi not connected and advertising.");
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
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // If we are connected to WiFi and we are in advertising mode, we release the semaphore and return.
    // After the return, advertise() is called again, releasing the BLE resources and waiting for the lock,
    // which will be released by the wifi_synch task if the synch is done, either successfully or not.
    if(config.wifi_connected)
    {
        xSemaphoreGive(config.wifi_synch_semaphore);
    }
}


void clean_up(BNO055 *bno_adv)
{
    config.ble_old_device_connected = false;
    if(config.wifi_synch_semaphore == NULL)
    {
        ESP_LOGI(TAG_Adv, "Sem. is NULL");
    } else
    {
        for(;;)
        {
            // Wait until wifi_synch task terminated. If this task terminated. The other must have terminated.
            if(xSemaphoreTake(config.wifi_synch_semaphore, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                ESP_LOGI(TAG_Adv, "Took the WiFi semaphore. WiFi-Synch task terminated.");
                break;
            }
            vTaskDelay(80 / portTICK_PERIOD_MS);
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