#include "main.h"
#define TAG "main"

extern "C"
{
    void app_main();
}

void app_main()
{
    printf("begin !\n");

    gpio_install_isr_service(0);

    watchPower.init();
    watchTft.init();

    vTaskDelay(4000);

    while (1)
    {

        float current_Batt = axpxx_getBattDischargeCurrent();
        float current_Vbus = axpxx_getVbusCurrent();
        printf("Current, batt:%.2f, vBus:%.2f\n",
               current_Batt,
               current_Vbus);

        WatchTft::add_chart_power_value((lv_coord_t)current_Batt, (lv_coord_t)current_Vbus);
        vTaskDelay(3000);
    }
}