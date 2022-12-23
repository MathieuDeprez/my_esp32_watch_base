#include "main.h"

#ifndef WATCH_POWER
#define WATCH_POWER

#define AXP202_INT GPIO_NUM_35

class WatchPower
{
    public:

    static bool is_charging;
    
    static void init();
    static void register_int_isr();
    static void enter_light_sleep();
    static void turn_off_tft();

    private:
    static volatile bool _irq;
    static void IRAM_ATTR gpio_isr_handler(void *arg);
    static void power_task(void *pvParameter);
};
#endif