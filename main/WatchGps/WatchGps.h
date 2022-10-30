#include "main.h"

#ifndef WATCH_GPS
#define WATCH_GPS

#define TIME_ZONE (+2)   // Paris Time
#define YEAR_BASE (2000) // date in GPS starts from 2000

class WatchGps
{
public:
    static bool gps_tracking_stop;
    static float gps_latitude;
    static float gps_longitude;
    static float gps_altitude;
    static float gps_speed;
    static uint32_t gps_point_saved;
    static char gps_datetime[21];
    static char gps_date[11];
    static char gps_time[9];

    static void init();
    static void tracking_task(void *pvParameter);

private:
    static gps_fix_mode_t gps_status;

    static void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
};

#endif