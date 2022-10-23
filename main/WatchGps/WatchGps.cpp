#include "WatchGps.h"

void WatchGps::init()
{
    axpxx_setPowerOutPut(AXP202_LDO4, AXP202_ON);

    static bool is_gps_init_done = false;
    if (!is_gps_init_done)
    {
        /* NMEA parser configuration */
        nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
        /* init NMEA parser library */
        nmea_parser_handle_t nmea_hdl = nmea_parser_init(&config);
        /* register event handler for NMEA parser library */
        nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
        is_gps_init_done = true;
    }
}

void WatchGps::tracking_task(void *pvParameter)
{
    while (1)
    {
        printf("tracking task\n");
        vTaskDelay(5000);
    }
}

void WatchGps::gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    gps_t *gps = NULL;
    switch (event_id)
    {
    case GPS_UPDATE:
    {
        gps = (gps_t *)event_data;

        static bool gps_looking_state = true;
        if (gps->fix_mode == gps_fix_mode_t::GPS_MODE_INVALID)
        {
            if (!gps_looking_state)
            {
                gps_looking_state = true;
                WatchTft::gps_print_title("Looking for GPS...");
            }
            char gps_info_str[300] = {};
            sprintf(gps_info_str, "%d stat(s) in view\n"
                                  "snr: ",
                    gps->sats_in_view);

            for (uint8_t sat = 0; sat < gps->sats_in_view; sat++)
            {
                if (sat)
                {
                    strcat(gps_info_str, ", ");
                }
                char sat_snr[20] = {};
                bzero(sat_snr, sizeof(sat_snr));
                sprintf(sat_snr, "%d", gps->sats_desc_in_view[sat].snr);
                strcat(gps_info_str, sat_snr);
            }
            strcat(gps_info_str, "\n");
            printf("%s", gps_info_str);
            WatchTft::gps_print_info(gps_info_str);
        }
        else
        {
            if (gps_looking_state)
            {
                gps_looking_state = false;
                WatchTft::gps_print_title("GPS location found.");
            }
            char gps_info_str[400] = {};
            sprintf(gps_info_str, "datetime: %d/%d/%d %d:%d:%d\n"
                                  "latitude: %.05f°N\n"
                                  "longitude: %.05f°E\n"
                                  "altitude: %.02fm\n"
                                  "speed: %fm/s\n"
                                  "sat(s) in view: %d\n"
                                  "snr: \n",
                    gps->date.year + YEAR_BASE, gps->date.month, gps->date.day,
                    gps->tim.hour + TIME_ZONE, gps->tim.minute, gps->tim.second,
                    gps->latitude, gps->longitude, gps->altitude, gps->speed,
                    gps->sats_in_view);
            for (uint8_t sat = 0; sat < gps->sats_in_view; sat++)
            {
                if (sat)
                {
                    strcat(gps_info_str, ", ");
                }
                char sat_snr[20] = {};
                bzero(sat_snr, sizeof(sat_snr));
                sprintf(sat_snr, "%d", gps->sats_desc_in_view[sat].snr);
                strcat(gps_info_str, sat_snr);
            }
            strcat(gps_info_str, "\n");
            printf("%s", gps_info_str);
            WatchTft::gps_print_info(gps_info_str);
        }

        /* print information parsed from GPS statements */
    }
    break;
    case GPS_UNKNOWN:
        /* print unknown statements */
        printf("Unknown statement:%s\n", (char *)event_data);
        break;
    default:
        break;
    }
}