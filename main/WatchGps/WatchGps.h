#include "main.h"

#ifndef WATCH_GPS
#define WATCH_GPS

#define TIME_ZONE (+2)   // Paris Time
#define YEAR_BASE (2000) // date in GPS starts from 2000

class WatchGps
{
public:
    static void init();
    static void tracking_task(void *pvParameter);

private:
    static void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
};

#endif