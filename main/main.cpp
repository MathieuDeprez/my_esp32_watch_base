#include "main.h"
#define TAG "main"

SemaphoreHandle_t i2c_0_semaphore;
uint32_t timer_last_touch = esp_timer_get_time() / 1000;
uint32_t timer_turn_off_screen = 60000;
extern "C"
{
    void app_main();
}

void app_main()
{
    printf("begin !\n");

    i2c_0_semaphore = xSemaphoreCreateMutex();

    gpio_install_isr_service(0);

    WatchPower::init();
    WatchTft::init();
    WatchBma::init();
    printf("*** INIT DONE ***\n");
    vTaskDelay(4000);

    while (1)
    {
        if ((esp_timer_get_time() / 1000) - timer_last_touch > timer_turn_off_screen && WatchTft::screen_en)
        {
            WatchTft::turn_off_screen();
        }
        BaseType_t err = xSemaphoreTake(i2c_0_semaphore, 500);
        if (err != pdTRUE)
        {
            printf("can't take i2c_sem from main\n");
            continue;
        }

        float current_Batt = axpxx_getBattDischargeCurrent();
        float current_Vbus = axpxx_getVbusCurrent();
        xSemaphoreGive(i2c_0_semaphore);

        WatchTft::add_chart_power_value((lv_coord_t)current_Batt, (lv_coord_t)current_Vbus);
        vTaskDelay(1000);
    }
}