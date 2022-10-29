#include "WatchGps.h"

gps_fix_mode_t WatchGps::gps_status = gps_fix_mode_t::GPS_MODE_INVALID;
bool WatchGps::gps_tracking_stop = true;
float WatchGps::gps_latitude = 0;
float WatchGps::gps_longitude = 0;
float WatchGps::gps_altitude = 0;
uint32_t WatchGps::gps_point_saved = 0;
float WatchGps::gps_speed = 0;
char WatchGps::gps_datetime[21] = "2016-07-17T08:30:28Z";
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
    gps_tracking_stop = false;
    WatchTft::set_gps_tracking_hidden_state(0);

    char gps_file_name[21];

    // reseed the random number generator
    srand(time(NULL));

    gps_file_name[0] = '/';
    gps_file_name[1] = 's';
    gps_file_name[2] = 'd';
    gps_file_name[3] = 'c';
    gps_file_name[4] = 'a';
    gps_file_name[5] = 'r';
    gps_file_name[6] = 'd';
    gps_file_name[7] = '/';
    for (uint8_t i = 8; i < 16; i++)
    {
        // Add random printable ASCII char
        gps_file_name[i] = (rand() % ('z' - 'a')) + 'a';
    }
    gps_file_name[16] = '.';
    gps_file_name[17] = 'g';
    gps_file_name[18] = 'p';
    gps_file_name[19] = 'x';
    gps_file_name[20] = '\0';

    printf("gps_file_name: %s\n", gps_file_name);

    char start_file[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<gpx>\n"
                        "<trk>\n"
                        "<name>esp_gpx</name>\n"
                        "<type>Running</type>\n"
                        "<trkseg>\n";

    FILE *f = fopen(gps_file_name, "w");
    if (f == NULL)
    {
        printf("Failed to open gps_file for writing\n");

        gps_tracking_stop = true;
        WatchTft::set_gps_tracking_hidden_state(1);
        WatchTft::current_task_hanlde = NULL;
        vTaskDelete(NULL);
    }
    fwrite(start_file, strlen(start_file), 1, f);
    fclose(f);
    printf("Gps_file written\n");

    gps_point_saved = 0;

    while (1)
    {

        static uint32_t timer_gps_task = (uint32_t)esp_timer_get_time() / 1000U;

        if ((uint32_t)esp_timer_get_time() / 1000U - timer_gps_task > 5000)
        {
            printf("tracking task\n");
            timer_gps_task = (uint32_t)esp_timer_get_time() / 1000U;

            if (gps_status != gps_fix_mode_t::GPS_MODE_INVALID /*|| true*/)
            {
                printf("Saving pos to SD\n");

                if (gps_status == gps_fix_mode_t::GPS_MODE_INVALID)
                {
                    gps_latitude = (float)(rand() % 100000) / (float)50000;
                    gps_longitude = (float)(rand() % 100000) / (float)50000;
                    gps_altitude = (float)(rand() % 100000) / (float)50000;
                }

                char gps_pos_line[150] = {};
                sprintf(gps_pos_line, "<trkpt lat=\"%.05f\" lon=\"%.05f\">\n"
                                      "<ele>%.05f</ele>\n"
                                      "<time>%s</time>\n"
                                      "<speed>%.05f</speed>\n"
                                      "</trkpt>\n",
                        gps_latitude,
                        gps_longitude,
                        gps_altitude,
                        gps_datetime,
                        gps_speed);
                printf("line: %s\n", gps_pos_line);

                FILE *f = fopen(gps_file_name, "a");
                if (f == NULL)
                {
                    printf("Failed to open gps_file for appending\n");
                    continue;
                }
                fwrite(gps_pos_line, strlen(gps_pos_line), 1, f);
                fclose(f);
                printf("Gps_file written\n");

                gps_point_saved++;
            }
        }

        if (gps_tracking_stop)
        {
            printf("STOP gps tracking task: %s\n", gps_file_name);

            char end_file[] = "</trkseg>\n"
                              "</trk>\n"
                              "</gpx>\n";

            FILE *f = fopen(gps_file_name, "a");
            if (f == NULL)
            {
                printf("Failed to open gps_file for end appending\n");
                continue;
            }
            fwrite(end_file, strlen(end_file), 1, f);
            fclose(f);
            printf("Gps_file written\n");

            f = fopen(gps_file_name, "r");
            if (f == NULL)
            {
                printf("Failed to open gps_file for end reading\n");
                continue;
            }
            char currentline[100];
            while (fgets(currentline, sizeof(currentline), f) != NULL)
            {
                printf("%s", currentline);
            }
            printf("\n");

            fclose(f);

            if (gps_point_saved == 0)
            {
                printf("remove gps_file because no point\n");
                remove(gps_file_name);
            }

            WatchTft::set_gps_tracking_hidden_state(1);
            WatchTft::current_task_hanlde = NULL;
            vTaskDelete(NULL);
        }

        vTaskDelay(200);
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

        gps_status = gps->fix_mode;

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
                                  "%u points saved\n"
                                  "snr: ",
                    gps->sats_in_view, gps_point_saved);

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
                                  "(%u)latitude: %.05f°N\n"
                                  "longitude: %.05f°E\n"
                                  "altitude: %.02fm\n"
                                  "speed: %fm/s\n"
                                  "sat(s) in view: %d\n"
                                  "snr: \n",
                    gps->date.year + YEAR_BASE, gps->date.month, gps->date.day,
                    gps->tim.hour + TIME_ZONE, gps->tim.minute, gps->tim.second,
                    gps_point_saved, gps->latitude, gps->longitude, gps->altitude, gps->speed,
                    gps->sats_in_view);

            gps_latitude = gps->latitude;
            gps_longitude = gps->longitude;
            gps_altitude = gps->altitude;
            gps_speed = gps->speed;
            sprintf(gps_datetime, "%hu-%hhu-%hhuT%hhu:%hhu:%hhuZ",
                    gps->date.year + YEAR_BASE,
                    gps->date.month,
                    gps->date.day,
                    gps->tim.hour + TIME_ZONE,
                    gps->tim.minute,
                    gps->tim.second);
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