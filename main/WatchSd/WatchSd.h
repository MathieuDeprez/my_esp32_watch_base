#include "main.h"

#ifndef WATCH_SD
#define WATCH_SD

#define MOUNT_POINT "/sdcard"

// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO  GPIO_NUM_4
#define PIN_NUM_MOSI  GPIO_NUM_15
#define PIN_NUM_CLK   GPIO_NUM_14
#define PIN_NUM_CS    GPIO_NUM_13

class WatchSd
{
public:
    static void init();

private:
};

#endif