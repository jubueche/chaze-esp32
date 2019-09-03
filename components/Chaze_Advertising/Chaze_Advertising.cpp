#include "Chaze_Advertising.h"
#include "Configuration.h"
#include "esp_log.h"

const char * TAG_Adv = "Chaze-Advertising";

//! Maybe make reusable in config
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
    ESP_LOGI(TAG_Adv, "Start advertising.");
    bno_adv = BNO055();
    am_interrupt = false;
    attach_am_interrupt();
    while(config.STATE == ADVERTISING)
    {
        ESP_LOGI(TAG_Adv, "Advertising...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("Level is %d\n", gpio_get_level(GPIO_BNO_INT));
        if(am_interrupt){
            printf("Interrupt.\n");
            am_interrupt = false;
            bno_adv.resetInterrupts();
            gpio_reset_pin(GPIO_BNO_INT);
            config.STATE = RECORD;
            break;
        }
    }

    //Free the objects and detach interrupt
    config.detach_bno_int();


}

void attach_am_interrupt()
{
    if(!bno_adv.begin())
    {
        ESP_LOGE(TAG_Adv, "Error initializing BNO055.");
    }
    bno_adv.enableAnyMotion(ANY_MOTION_THRESHOLD, ANY_MOTION_DURATION);
    bno_adv.enableInterruptsOnXYZ(ENABLE, ENABLE, ENABLE);
    bno_adv.setExtCrystalUse(true);

    // Initialize function pointers
    void (*task_pointer)(void *) = &gpio_bno_task;
    void (*isr_pointer) (void *) = &gpio_isr_handler;
    
    config.attach_bno_int(task_pointer, isr_pointer);

    ESP_LOGI(TAG_Adv, "Attached BNO interrupt.");

}