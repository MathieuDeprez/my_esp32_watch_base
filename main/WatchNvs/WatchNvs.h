#include "main.h"

#ifndef WATCH_NVS
#define WATCH_NVS

#define NVS_KEY_TIMEOUT_SCREEN "tmo_screen"
#define NVS_KEY_WAKEUP_DOUBLE_TAP "wakeup_db_tap"
#define NVS_KEY_WAKEUP_TILT "wakeup_tilt"

class WatchNvs
{
public:
    static void init();
    static void set_tmo_screen(uint32_t value);
    static void set_wakeup_double_tap(uint8_t value);
    static void set_wakeup_tilt(uint8_t value);

private:
    static char main_storage_name[7];
};

#endif