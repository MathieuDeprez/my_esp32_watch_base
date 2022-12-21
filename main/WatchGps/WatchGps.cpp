#include "WatchGps.h"

WatchGps::gps_fix_mode_t WatchGps::gps_status = gps_fix_mode_t::GPS_MODE_INVALID;
bool WatchGps::gps_tracking_stop = true;
WatchGps::esp_gps_t WatchGps::global_gps = {};

WatchGps::gps_t WatchGps::gps_saved = {};
uint32_t WatchGps::gps_point_saved = 0;

double WatchGps::gps_lat = HOME_LAT;
double WatchGps::gps_lon = HOME_LON;

void WatchGps::init()
{
    axpxx_setPowerOutPut(AXP202_LDO4, AXP202_ON);

    static bool is_gps_init_done = false;
    if (!is_gps_init_done)
    {
        nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
        nmea_parser_handle_t nmea_hdl = nmea_parser_init(&config);
        is_gps_init_done = true;
    }
}

WatchGps::nmea_parser_handle_t WatchGps::nmea_parser_init(const nmea_parser_config_t *config)
{
    esp_gps_t *esp_gps = (esp_gps_t *)calloc(1, sizeof(esp_gps_t));
    if (!esp_gps)
    {
        printf("calloc memory for esp_fps failed\n");
        free(esp_gps);
        return NULL;
    }
    esp_gps->buffer = (uint8_t *)calloc(1, NMEA_PARSER_RUNTIME_BUFFER_SIZE);
    if (!esp_gps->buffer)
    {
        printf("calloc memory for runtime buffer failed\n");
        free(esp_gps->buffer);
        free(esp_gps);
        return NULL;
    }
#if CONFIG_NMEA_STATEMENT_GSA
    esp_gps->all_statements |= (1 << STATEMENT_GSA);
#endif
#if CONFIG_NMEA_STATEMENT_GSV
    esp_gps->all_statements |= (1 << STATEMENT_GSV);
#endif
#if CONFIG_NMEA_STATEMENT_GGA
    esp_gps->all_statements |= (1 << STATEMENT_GGA);
#endif
#if CONFIG_NMEA_STATEMENT_RMC
    esp_gps->all_statements |= (1 << STATEMENT_RMC);
#endif
#if CONFIG_NMEA_STATEMENT_GLL
    esp_gps->all_statements |= (1 << STATEMENT_GLL);
#endif
#if CONFIG_NMEA_STATEMENT_VTG
    esp_gps->all_statements |= (1 << STATEMENT_VTG);
#endif
    esp_gps->all_statements |= (1 << STATEMENT_GPTXT);
    esp_gps->all_statements |= (1 << STATEMENT_GNZDA);
    // Set attributes
    esp_gps->uart_port = config->uart.uart_port;
    esp_gps->all_statements &= 0xFE;
    // Install UART friver
    uart_config_t uart_config = {
        .baud_rate = (int)config->uart.baud_rate,
        .data_bits = config->uart.data_bits,
        .parity = config->uart.parity,
        .stop_bits = config->uart.stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    if (uart_driver_install(esp_gps->uart_port, CONFIG_NMEA_PARSER_RING_BUFFER_SIZE, 0,
                            0, NULL, 0) != ESP_OK)
    {
        printf("install uart driver failed\n");
        uart_driver_delete(esp_gps->uart_port);
        free(esp_gps->buffer);
        free(esp_gps);
        return NULL;
    }
    if (uart_param_config(esp_gps->uart_port, &uart_config) != ESP_OK)
    {
        printf("config uart parameter failed\n");
        free(esp_gps->buffer);
        free(esp_gps);
        return NULL;
    }
    if (uart_set_pin(esp_gps->uart_port, GPIO_NUM_26, config->uart.rx_pin, // TX to GPIO_NUM_26
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
    {
        printf("config uart gpio failed\n");
        free(esp_gps->buffer);
        free(esp_gps);
        return NULL;
    }

    uart_flush(esp_gps->uart_port);

    // Create NMEA Parser task
    BaseType_t err = xTaskCreate(
        nmea_parser_task_entry,
        "nmea_parser",
        CONFIG_NMEA_PARSER_TASK_STACK_SIZE,
        esp_gps,
        CONFIG_NMEA_PARSER_TASK_PRIORITY,
        &esp_gps->tsk_hdl);
    if (err != pdTRUE)
    {
        printf("create NMEA Parser task failed\n");
        uart_driver_delete(esp_gps->uart_port);
        free(esp_gps->buffer);
        free(esp_gps);
        return NULL;
    }
    printf("NMEA Parser init OK\n");
    return esp_gps;
}

void WatchGps::nmea_parser_task_entry(void *arg)
{
    esp_gps_t *esp_gps = (esp_gps_t *)arg;
    uart_event_t event;
    while (1)
    {
        uint8_t data[1024] = {};
        int len = uart_read_bytes(UART_NUM_1, data, 1023, 20 / portTICK_RATE_MS);

        if (len)
        {
            if (gps_decode(data, len) != ESP_OK)
            {
                printf("GPS decode line failed\n");
            }
            vTaskDelay(20);
        }

        static uint32_t timer_stage = esp_timer_get_time() / 1000U;
        static uint8_t get_done_stage = 0;
        const uint32_t timer_btw_stage = 1000;

        if ((esp_timer_get_time() / 500U) - timer_stage > timer_btw_stage && get_done_stage == 0)
        {

            printf("Enabling GALILEO L76L!\n");

            char cmd_qt_02[] = "$PMTK353,1,1,1,0,0*2A\r\n"; // set protocol galileo enable
            uart_write_bytes(UART_NUM_1, cmd_qt_02, sizeof(cmd_qt_02));
            uart_wait_tx_done(UART_NUM_1, 10000 / portTICK_PERIOD_MS);

            timer_stage = esp_timer_get_time() / 1000U;
            get_done_stage++;
        }
        else if ((esp_timer_get_time() / 500U) - timer_stage > timer_btw_stage && get_done_stage == 1)
        {
            printf("Setting pedestrian mode L76L!\n");
            char cmd_qt_04[] = "$PMTK886,1*29\r\n";
            uart_write_bytes(UART_NUM_1, cmd_qt_04, sizeof(cmd_qt_04));
            uart_wait_tx_done(UART_NUM_1, 10000 / portTICK_PERIOD_MS);

            timer_stage = esp_timer_get_time() / 1000U;
            get_done_stage++;
        }

        static uint32_t timer_print = esp_timer_get_time() / 1000U;
        if ((esp_timer_get_time() / 1000U) - timer_print > 1000)
        {

            gps_t *gps = &global_gps.parent;

            WatchGps::gps_status = gps->fix_mode;

            if (gps->fix_mode == gps_fix_mode_t::GPS_MODE_INVALID)
            {
                WatchTft::set_gps_info(false, gps->sats_in_view, gps->dop_p, gps->latitude, gps->longitude);
            }
            else
            {
                WatchTft::set_gps_info(true, gps->sats_in_use, gps->dop_p, gps->latitude, gps->longitude);

                memcpy(&WatchGps::gps_saved, gps, sizeof(WatchGps::gps_saved));
            }

            printf("%d/%d/%d %d:%d:%d => view %d, use %d, lat %.05f, lon %.05f, alt: %.02f, dop %.2f\r\n",
                   gps->date.year + YEAR_BASE, gps->date.month, gps->date.day,
                   gps->tim.hour + TIME_ZONE, gps->tim.minute, gps->tim.second,
                   gps->sats_in_view, gps->sats_in_use, gps->latitude, gps->longitude, gps->altitude,
                   gps->dop_p);

            timer_print = esp_timer_get_time() / 1000U;
        }
    }
    vTaskDelete(NULL);
}

esp_err_t WatchGps::gps_decode(uint8_t *buff_uart, size_t len)
{
    const uint8_t *d = buff_uart;

    static bool is_nmea = false;
    static bool is_ubx = false;
    static bool ga_printed = false;

    /*for (size_t i = 0; i < len; i++)
    {
        if (i && i % 32 == 0)
        {
            printf("\n");
        }
        printf("%02x ", buff_uart[i]);
    }
    printf("\n");*/

    for (size_t i = 0; i < len; i++, buff_uart++)
    {
        // Start of a statement
        if (*d == '$')
        {
            if (is_ubx && !is_nmea)
            {
                printf("\n");
            }
            is_nmea = true;
            is_ubx = false;

            ga_printed = false;
            // Reset runtime information
            global_gps.asterisk = 0;
            global_gps.item_num = 0;
            global_gps.item_pos = 0;
            global_gps.cur_statement = 0;
            global_gps.crc = 0;
            global_gps.sat_count = 0;
            global_gps.sat_num = 0;
            // Add character to item
            global_gps.item_str[global_gps.item_pos++] = *d;
            global_gps.item_str[global_gps.item_pos] = '\0';
        }
        else if (!is_nmea)
        {
            printf("%02x ", *d);
            is_ubx = true;

            if (len > 30 && !ga_printed)
            {
                if (*d == 0xb5 &&
                    *(d + 1) == 0x62 &&
                    *(d + 2) == 0x06 &&
                    *(d + 3) == 0x3e &&
                    *(d + 4) == 0x3c &&
                    *(d + 5) == 0x00)
                {
                    bool galileo_en = d[30];
                    printf("galileo_en: %d\n", galileo_en);
                    ga_printed = true;
                }
            }
        }
        // Detect item separator character
        else if (*d == ',')
        {
            // Parse current item
            parse_item(&global_gps);
            // Add character to CRC computation
            global_gps.crc ^= (uint8_t)(*d);
            // Start with next item
            global_gps.item_pos = 0;
            global_gps.item_str[0] = '\0';
            global_gps.item_num++;
        }
        // End of CRC computation
        else if (*d == '*')
        {
            // Parse current item
            parse_item(&global_gps);
            // Asterisk detected
            global_gps.asterisk = 1;
            // Start with next item
            global_gps.item_pos = 0;
            global_gps.item_str[0] = '\0';
            global_gps.item_num++;
        }
        // End of statement
        else if (*d == '\r')
        {
            // Convert received CRC from string (hex) to number
            uint8_t crc = (uint8_t)strtol(global_gps.item_str, NULL, 16);
            // CRC passed
            if (global_gps.crc == crc)
            {
                switch (global_gps.cur_statement)
                {
#if CONFIG_NMEA_STATEMENT_GGA
                case STATEMENT_GGA:
                    global_gps.parsed_statement |= 1 << STATEMENT_GGA;
                    break;
#endif
#if CONFIG_NMEA_STATEMENT_GSA
                case STATEMENT_GSA:
                    global_gps.parsed_statement |= 1 << STATEMENT_GSA;
                    break;
#endif
#if CONFIG_NMEA_STATEMENT_RMC
                case STATEMENT_RMC:
                    global_gps.parsed_statement |= 1 << STATEMENT_RMC;
                    break;
#endif
#if CONFIG_NMEA_STATEMENT_GSV
                case STATEMENT_GSV:
                    if (global_gps.sat_num == global_gps.sat_count)
                    {
                        global_gps.parsed_statement |= 1 << STATEMENT_GSV;
                    }
                    break;
#endif
#if CONFIG_NMEA_STATEMENT_GLL
                case STATEMENT_GLL:
                    global_gps.parsed_statement |= 1 << STATEMENT_GLL;
                    break;
#endif
#if CONFIG_NMEA_STATEMENT_VTG
                case STATEMENT_VTG:
                    global_gps.parsed_statement |= 1 << STATEMENT_VTG;
                    break;
#endif
                case STATEMENT_GPTXT:
                    global_gps.parsed_statement |= 1 << STATEMENT_GPTXT;
                    break;
                case STATEMENT_GNZDA:
                    global_gps.parsed_statement |= 1 << STATEMENT_GNZDA;
                    break;
                default:
                    break;
                }
                // Check if all statements have been parsed
                if (((global_gps.parsed_statement) & global_gps.all_statements) == global_gps.all_statements)
                {
                    global_gps.parsed_statement = 0;
                }
                if (*(d + 1) == '\n')
                {
                    d++;
                    i++;
                }
            }
            else
            {
                printf("CRC Error for statement:%s\n", global_gps.buffer);
            }
            if (global_gps.cur_statement == STATEMENT_UNKNOWN)
            {
                // Send signal to notify that one unknown statement has been met
            }
        }
        // Other non-space character
        else
        {
            if (!(global_gps.asterisk))
            {
                // Add to CRC
                global_gps.crc ^= (uint8_t)(*d);
            }
            // Add character to item
            global_gps.item_str[global_gps.item_pos++] = *d;
            global_gps.item_str[global_gps.item_pos] = '\0';
        }
        // Process next character
        d++;
    }
    return ESP_OK;
}

esp_err_t WatchGps::parse_item(esp_gps_t *esp_gps)
{
    esp_err_t err = ESP_OK;
    /* start of a statement */
    if (esp_gps->item_num == 0 && esp_gps->item_str[0] == '$')
    {
        if (0)
        {
        }
#if CONFIG_NMEA_STATEMENT_GGA
        else if (strstr(esp_gps->item_str, "GGA"))
        {
            esp_gps->cur_statement = STATEMENT_GGA;
        }
#endif
#if CONFIG_NMEA_STATEMENT_GSA
        else if (strstr(esp_gps->item_str, "GSA"))
        {
            esp_gps->cur_statement = STATEMENT_GSA;
        }
#endif
#if CONFIG_NMEA_STATEMENT_RMC
        else if (strstr(esp_gps->item_str, "RMC"))
        {
            esp_gps->cur_statement = STATEMENT_RMC;
        }
#endif
#if CONFIG_NMEA_STATEMENT_GSV
        else if (strstr(esp_gps->item_str, "GSV"))
        {
            esp_gps->cur_statement = STATEMENT_GSV;
        }
#endif
#if CONFIG_NMEA_STATEMENT_GLL
        else if (strstr(esp_gps->item_str, "GLL"))
        {
            esp_gps->cur_statement = STATEMENT_GLL;
        }
#endif
#if CONFIG_NMEA_STATEMENT_VTG
        else if (strstr(esp_gps->item_str, "VTG"))
        {
            esp_gps->cur_statement = STATEMENT_VTG;
        }
#endif
        else if (strstr(esp_gps->item_str, "GPTXT"))
        {
            esp_gps->cur_statement = STATEMENT_GPTXT;
        }
        else if (strstr(esp_gps->item_str, "GNZDA"))
        {
            esp_gps->cur_statement = STATEMENT_GNZDA;
        }
        else
        {
            esp_gps->cur_statement = STATEMENT_UNKNOWN;
        }
        goto out;
    }
    // Parse each item, depend on the type of the statement
    if (esp_gps->cur_statement == STATEMENT_UNKNOWN)
    {
        goto out;
    }
#if CONFIG_NMEA_STATEMENT_GGA
    else if (esp_gps->cur_statement == STATEMENT_GGA)
    {
        parse_gga(esp_gps);
    }
#endif
#if CONFIG_NMEA_STATEMENT_GSA
    else if (esp_gps->cur_statement == STATEMENT_GSA)
    {
        parse_gsa(esp_gps);
    }
#endif
#if CONFIG_NMEA_STATEMENT_GSV
    else if (esp_gps->cur_statement == STATEMENT_GSV)
    {
        parse_gsv(esp_gps);
    }
#endif
#if CONFIG_NMEA_STATEMENT_RMC
    else if (esp_gps->cur_statement == STATEMENT_RMC)
    {
        parse_rmc(esp_gps);
    }
#endif
#if CONFIG_NMEA_STATEMENT_GLL
    else if (esp_gps->cur_statement == STATEMENT_GLL)
    {
        parse_gll(esp_gps);
    }
#endif
#if CONFIG_NMEA_STATEMENT_VTG
    else if (esp_gps->cur_statement == STATEMENT_VTG)
    {
        parse_vtg(esp_gps);
    }
#endif
    else if (esp_gps->cur_statement == STATEMENT_GPTXT)
    {
        parse_gptxt(esp_gps);
    }
    else if (esp_gps->cur_statement == STATEMENT_GNZDA)
    {
        parse_gnzda(esp_gps);
    }
    else
    {
        err = ESP_FAIL;
    }
out:
    return err;
}

float WatchGps::parse_lat_long(esp_gps_t *esp_gps)
{
    float ll = strtof(esp_gps->item_str, NULL);
    int deg = ((int)ll) / 100;
    float min = ll - (deg * 100);
    ll = deg + min / 60.0f;
    // printf("lat_long %.6f\n", ll);
    return ll;
}

#if CONFIG_NMEA_STATEMENT_GGA
/**
 * @brief Parse GGA statements
 *
 * @param esp_gps esp_gps_t type object
 */
void WatchGps::parse_gga(esp_gps_t *esp_gps)
{
    // Process GGA statement
    switch (esp_gps->item_num)
    {
    case 1: // Process UTC time
        parse_utc_time(esp_gps);
        break;
    case 2: // Latitude
        esp_gps->parent.latitude = parse_lat_long(esp_gps);
        break;
    case 3: // Latitude north(1)/south(-1) information
        if (esp_gps->item_str[0] == 'S' || esp_gps->item_str[0] == 's')
        {
            esp_gps->parent.latitude *= -1;
        }
        break;
    case 4: // Longitude
        esp_gps->parent.longitude = parse_lat_long(esp_gps);
        break;
    case 5: // Longitude east(1)/west(-1) information
        if (esp_gps->item_str[0] == 'W' || esp_gps->item_str[0] == 'w')
        {
            esp_gps->parent.longitude *= -1;
        }
        break;
    case 6: // Fix status
        esp_gps->parent.fix = (gps_fix_t)strtol(esp_gps->item_str, NULL, 10);
        break;
    case 7: // Satellites in use
        esp_gps->parent.sats_in_use = (uint8_t)strtol(esp_gps->item_str, NULL, 10);
        break;
    case 8: // HDOP
        esp_gps->parent.dop_h = strtof(esp_gps->item_str, NULL);
        break;
    case 9: // Altitude
        esp_gps->parent.altitude = strtof(esp_gps->item_str, NULL);
        break;
    case 11: // Altitude above ellipsoid
        esp_gps->parent.altitude += strtof(esp_gps->item_str, NULL);
        break;
    default:
        break;
    }
}
#endif

uint8_t WatchGps::convert_two_digit2number(const char *digit_char)
{
    return 10 * (digit_char[0] - '0') + (digit_char[1] - '0');
}

void WatchGps::parse_utc_time(esp_gps_t *esp_gps)
{
    esp_gps->parent.tim.hour = convert_two_digit2number(esp_gps->item_str + 0);
    esp_gps->parent.tim.minute = convert_two_digit2number(esp_gps->item_str + 2);
    esp_gps->parent.tim.second = convert_two_digit2number(esp_gps->item_str + 4);
    if (esp_gps->item_str[6] == '.')
    {
        uint16_t tmp = 0;
        uint8_t i = 7;
        while (esp_gps->item_str[i])
        {
            tmp = 10 * tmp + esp_gps->item_str[i] - '0';
            i++;
        }
        esp_gps->parent.tim.thousand = tmp;
    }
}

#if CONFIG_NMEA_STATEMENT_GSA
/**
 * @brief Parse GSA statements
 *
 * @param esp_gps esp_gps_t type object
 */
void WatchGps::parse_gsa(esp_gps_t *esp_gps)
{
    // Process GSA statement
    switch (esp_gps->item_num)
    {
    case 2: // Process fix mode
        esp_gps->parent.fix_mode = (gps_fix_mode_t)strtol(esp_gps->item_str, NULL, 10);
        break;
    case 15: // Process PDOP
        esp_gps->parent.dop_p = strtof(esp_gps->item_str, NULL);
        break;
    case 16: // Process HDOP
        esp_gps->parent.dop_h = strtof(esp_gps->item_str, NULL);
        break;
    case 17: // Process VDOP
        esp_gps->parent.dop_v = strtof(esp_gps->item_str, NULL);
        break;
    default:
        // Parse satellite IDs
        if (esp_gps->item_num >= 3 && esp_gps->item_num <= 14)
        {
            esp_gps->parent.sats_id_in_use[esp_gps->item_num - 3] = (uint8_t)strtol(esp_gps->item_str, NULL, 10);
        }
        break;
    }
}
#endif

#if CONFIG_NMEA_STATEMENT_GSV
/**
 * @brief Parse GSV statements
 *
 * @param esp_gps esp_gps_t type object
 */
void WatchGps::parse_gsv(esp_gps_t *esp_gps)
{
    // Process GSV statement
    switch (esp_gps->item_num)
    {
    case 1: // total GSV numbers
        esp_gps->sat_count = (uint8_t)strtol(esp_gps->item_str, NULL, 10);
        break;
    case 2: // Current GSV statement number
        esp_gps->sat_num = (uint8_t)strtol(esp_gps->item_str, NULL, 10);
        break;
    case 3: // Process satellites in view
        esp_gps->parent.sats_in_view = (uint8_t)strtol(esp_gps->item_str, NULL, 10);
        break;
    default:
        if (esp_gps->item_num >= 4 && esp_gps->item_num <= 19)
        {
            uint8_t item_num = esp_gps->item_num - 4; // Normalize item number from 4-19 to 0-15
            uint8_t index;
            uint32_t value;
            index = 4 * (esp_gps->sat_num - 1) + item_num / 4; // Get array index
            if (index < GPS_MAX_SATELLITES_IN_VIEW)
            {
                value = strtol(esp_gps->item_str, NULL, 10);
                switch (item_num % 4)
                {
                case 0:
                    esp_gps->parent.sats_desc_in_view[index].num = (uint8_t)value;
                    break;
                case 1:
                    esp_gps->parent.sats_desc_in_view[index].elevation = (uint8_t)value;
                    break;
                case 2:
                    esp_gps->parent.sats_desc_in_view[index].azimuth = (uint16_t)value;
                    break;
                case 3:
                    esp_gps->parent.sats_desc_in_view[index].snr = (uint8_t)value;
                    break;
                default:
                    break;
                }
            }
        }
        break;
    }
}
#endif

#if CONFIG_NMEA_STATEMENT_RMC
/**
 * @brief Parse RMC statements
 *
 * @param esp_gps esp_gps_t type object
 */
void WatchGps::parse_rmc(esp_gps_t *esp_gps)
{
    // Process GPRMC statement
    switch (esp_gps->item_num)
    {
    case 1: // Process UTC time
        parse_utc_time(esp_gps);
        break;
    case 2: // Process valid status
        esp_gps->parent.valid = (esp_gps->item_str[0] == 'A');
        break;
    case 3: // Latitude
        esp_gps->parent.latitude = parse_lat_long(esp_gps);
        break;
    case 4: // Latitude north(1)/south(-1) information
        if (esp_gps->item_str[0] == 'S' || esp_gps->item_str[0] == 's')
        {
            esp_gps->parent.latitude *= -1;
        }
        break;
    case 5: // Longitude
        esp_gps->parent.longitude = parse_lat_long(esp_gps);
        break;
    case 6: // Longitude east(1)/west(-1) information
        if (esp_gps->item_str[0] == 'W' || esp_gps->item_str[0] == 'w')
        {
            esp_gps->parent.longitude *= -1;
        }
        break;
    case 7: // Process ground speed in unit m/s
        esp_gps->parent.speed = strtof(esp_gps->item_str, NULL) * 1.852;
        break;
    case 8: // Process true course over ground
        esp_gps->parent.cog = strtof(esp_gps->item_str, NULL);
        break;
    case 9: // Process date
        esp_gps->parent.date.day = convert_two_digit2number(esp_gps->item_str + 0);
        esp_gps->parent.date.month = convert_two_digit2number(esp_gps->item_str + 2);
        esp_gps->parent.date.year = convert_two_digit2number(esp_gps->item_str + 4);
        break;
    case 10: // Process magnetic variation
        esp_gps->parent.variation = strtof(esp_gps->item_str, NULL);
        break;
    default:
        break;
    }
}
#endif

void WatchGps::parse_gnzda(esp_gps_t *esp_gps)
{
    // Process GPRMC statement
    switch (esp_gps->item_num)
    {
    case 1: // Process UTC time
        parse_utc_time(esp_gps);
        break;
    case 2:
        esp_gps->parent.date.day = convert_two_digit2number(esp_gps->item_str);
    case 3:
        esp_gps->parent.date.month = convert_two_digit2number(esp_gps->item_str);
    case 4:
        esp_gps->parent.date.year = convert_two_digit2number(esp_gps->item_str);
    default:
        break;
    }
}

#if CONFIG_NMEA_STATEMENT_GLL
/**
 * @brief Parse GLL statements
 *
 * @param esp_gps esp_gps_t type object
 */
void WatchGps::parse_gll(esp_gps_t *esp_gps)
{
    // Process GPGLL statement
    switch (esp_gps->item_num)
    {
    case 1: // Latitude
        esp_gps->parent.latitude = parse_lat_long(esp_gps);
        break;
    case 2: // Latitude north(1)/south(-1) information
        if (esp_gps->item_str[0] == 'S' || esp_gps->item_str[0] == 's')
        {
            esp_gps->parent.latitude *= -1;
        }
        break;
    case 3: // Longitude
        esp_gps->parent.longitude = parse_lat_long(esp_gps);
        break;
    case 4: // Longitude east(1)/west(-1) information
        if (esp_gps->item_str[0] == 'W' || esp_gps->item_str[0] == 'w')
        {
            esp_gps->parent.longitude *= -1;
        }
        break;
    case 5: // Process UTC time
        parse_utc_time(esp_gps);
        break;
    case 6: // Process valid status
        esp_gps->parent.valid = (esp_gps->item_str[0] == 'A');
        break;
    default:
        break;
    }
}
#endif

#if CONFIG_NMEA_STATEMENT_VTG
/**
 * @brief Parse VTG statements
 *
 * @param esp_gps esp_gps_t type object
 */
void WatchGps::parse_vtg(esp_gps_t *esp_gps)
{
    // Process GPVGT statement
    switch (esp_gps->item_num)
    {
    case 1: // Process true course over ground
        esp_gps->parent.cog = strtof(esp_gps->item_str, NULL);
        break;
    case 3: // Process magnetic variation
        esp_gps->parent.variation = strtof(esp_gps->item_str, NULL);
        break;
    case 5:                                                              // Process ground speed in unit m/s
        esp_gps->parent.speed = strtof(esp_gps->item_str, NULL) * 1.852; // knots to m/s
        break;
    case 7:                                                            // Process ground speed in unit m/s
        esp_gps->parent.speed = strtof(esp_gps->item_str, NULL) / 3.6; // km/h to m/s
        break;
    default:
        break;
    }
}
#endif

void WatchGps::parse_gptxt(esp_gps_t *esp_gps)
{
    // Process GPVGT statement
    switch (esp_gps->item_num)
    {
    case 1:
    case 2:
    case 3:
        printf("%s ", esp_gps->item_str);
        break;
    case 4: // Process magnetic variation
        printf("%s\n", esp_gps->item_str);
        break;
    default:
        break;
    }
}

void WatchGps::tracking_task(void *pvParameter)
{
    gps_tracking_stop = false;
    WatchTft::set_gps_tracking_hidden_state(0);

    char gps_file_name[32] = {};
    //  "/sdcard/2016-07-17-08:30:28.gpx";

    gps_point_saved = 0;

    while (1)
    {

        static uint32_t timer_gps_task = (uint32_t)esp_timer_get_time() / 1000U;

        if ((uint32_t)esp_timer_get_time() / 1000U - timer_gps_task > 3000)
        {
            printf("tracking task\n");
            timer_gps_task = (uint32_t)esp_timer_get_time() / 1000U;

            if (gps_status != gps_fix_mode_t::GPS_MODE_INVALID)
            {
                printf("Saving pos to SD\n");

                if (strlen(gps_file_name) == 0)
                {

                    char gps_date[11] = "2016-07-17";
                    char gps_time[9] = "08-30-28";

                    sprintf(gps_date, "%hu-%02hhu-%02hhu",
                            gps_saved.date.year + YEAR_BASE,
                            gps_saved.date.month,
                            gps_saved.date.day);
                    sprintf(gps_time, "%02hhu-%02hhu-%02hhu",
                            gps_saved.tim.hour + TIME_ZONE,
                            gps_saved.tim.minute,
                            gps_saved.tim.second);

                    sprintf(gps_file_name, "/sdcard/%s-%s.gpx",
                            gps_date,
                            gps_time);

                    printf("new gps_file_name: %s\n", gps_file_name);

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
                    printf("Gps_file start written\n");
                }

                if (gps_status == gps_fix_mode_t::GPS_MODE_INVALID)
                {
                    gps_saved.latitude = (float)(rand() % 100000) / (float)50000;
                    gps_saved.longitude = (float)(rand() % 100000) / (float)50000;
                    gps_saved.altitude = (float)(rand() % 100000) / (float)50000;
                }

                char gps_datetime[21] = "2016-07-17T08:30:28Z";
                sprintf(gps_datetime, "%hu-%02hhu-%02hhuT%02hhu:%02hhu:%02hhuZ",
                        gps_saved.date.year + YEAR_BASE,
                        gps_saved.date.month,
                        gps_saved.date.day,
                        gps_saved.tim.hour + TIME_ZONE,
                        gps_saved.tim.minute,
                        gps_saved.tim.second);

                char gps_pos_line[200] = {};
                sprintf(gps_pos_line, "<trkpt lat=\"%.05f\" lon=\"%.05f\">\n"
                                      "<ele>%.05f</ele>\n"
                                      "<time>%s</time>\n"
                                      "<speed>%.05f</speed>\n"
                                      "<sat>%hhu</sat>\n"
                                      "<hdop>%.02f</hdop>\n"
                                      "<vdop>%.02f</vdop>\n"
                                      "<pdop>%.02f</pdop>\n"
                                      "</trkpt>\n",
                        gps_saved.latitude,
                        gps_saved.longitude,
                        gps_saved.altitude,
                        gps_datetime,
                        gps_saved.speed,
                        gps_saved.sats_in_use,
                        gps_saved.dop_h,
                        gps_saved.dop_v,
                        gps_saved.dop_p);
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
            }
            else
            {
                fwrite(end_file, strlen(end_file), 1, f);
                fclose(f);
                printf("Gps_file written\n");
            }

            f = fopen(gps_file_name, "r");
            if (f == NULL)
            {
                printf("Failed to open gps_file for end reading\n");
            }
            else
            {
                char currentline[100];
                while (fgets(currentline, sizeof(currentline), f) != NULL)
                {
                    printf("%s", currentline);
                }
                printf("\n");

                fclose(f);
            }
            WatchTft::set_gps_tracking_hidden_state(1);
            WatchTft::current_task_hanlde = NULL;
            vTaskDelete(NULL);
        }

        vTaskDelay(200);
    }
}

uint32_t WatchGps::lon_to_tile_x(double lon)
{
    uint32_t z = 15;
    return (int)(floor((lon + 180.0) / 360.0 * (1 << z)));
}

uint32_t WatchGps::lat_lon_to_tile_y(double lat, double lon)
{
    double latrad = lat * M_PI / 180.0;

    uint32_t z = 15;
    return (int)(floor((1.0 - asinh(tan(latrad)) / M_PI) / 2.0 * (1 << z)));
}

uint32_t WatchGps::lon_to_x(double lon)
{
    uint32_t z = 15;
    double xtile_d = (lon + 180.0) / 360.0 * (1 << z);
    return xtile_d * 256;
}

uint32_t WatchGps::lat_lon_to_y(double lat, double lon)
{
    uint32_t z = 15;

    double latrad = lat * M_PI / 180.0;

    double ytile_d = (1.0 - asinh(tan(latrad)) / M_PI) / 2.0 * (1 << z);
    return ytile_d * 256;
}