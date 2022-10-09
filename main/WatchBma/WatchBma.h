#include "main.h"

#ifndef WATCH_BMA
#define WATCH_BMA

#define BMA423_ANY_NO_MOTION_AXIS_EN_MSK 0xE0
#define BMA423_ANY_NO_MOTION_AXIS_EN_POS UINT8_C(5)
#define BMA423_ANYMOTION_EN_LEN 2
#define BMA423_INT GPIO_NUM_39

class WatchBma
{
public:
    static struct bma4_dev bma;
    static uint32_t steps_count_save;
    static uint16_t s_int_status;
    static bool wakeup_on_double_tap;
    static bool wakeup_on_tilt;
    static uint32_t step_counter_goal;

    static void init();
    static uint32_t get_steps();
    static uint16_t check_int_status();
    static bool check_irq();

private:
    static volatile bool _irq;

    static void register_int_isr();
    static void IRAM_ATTR gpio_isr_handler(void *arg);
    static void check_int_task(void *pvParameter);
};

#endif