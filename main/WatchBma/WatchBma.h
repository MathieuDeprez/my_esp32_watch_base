#include "main.h"

#ifndef WATCH_BMA
#define WATCH_BMA

#define BMA423_ANY_NO_MOTION_AXIS_EN_MSK 0xE0
#define BMA423_ANY_NO_MOTION_AXIS_EN_POS UINT8_C(5)
#define BMA423_ANYMOTION_EN_LEN 2

#define STEP_COUNTER_GOAL 1000

class WatchBma
{
public:
    static struct bma4_dev bma;
    static void init();
    static uint32_t get_steps();
    static uint32_t steps_count_save;

private:
    // private
};

#endif