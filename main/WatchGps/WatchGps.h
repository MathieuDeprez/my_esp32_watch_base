#include "main.h"

#ifndef WATCH_GPS
#define WATCH_GPS

#define TIME_ZONE (+1)   // Paris Time
#define YEAR_BASE (2000) // date in GPS starts from 2000

#define GPS_MAX_SATELLITES_IN_USE (12)
#define GPS_MAX_SATELLITES_IN_VIEW (16)

#define CONFIG_NMEA_PARSER_RING_BUFFER_SIZE 4096
#define CONFIG_NMEA_PARSER_TASK_STACK_SIZE 4096
#define CONFIG_NMEA_PARSER_TASK_PRIORITY 2

#define CONFIG_NMEA_STATEMENT_GGA 1
#define CONFIG_NMEA_STATEMENT_GSA 1
#define CONFIG_NMEA_STATEMENT_RMC 1
#define CONFIG_NMEA_STATEMENT_GSV 1
#define CONFIG_NMEA_STATEMENT_GLL 1
#define CONFIG_NMEA_STATEMENT_VTG 1

#define NMEA_PARSER_RUNTIME_BUFFER_SIZE (CONFIG_NMEA_PARSER_RING_BUFFER_SIZE / 2)
#define NMEA_EVENT_LOOP_QUEUE_SIZE (16)

#define NMEA_MAX_STATEMENT_ITEM_LENGTH (16)

#define NMEA_PARSER_CONFIG_DEFAULT()       \
    {                                      \
        .uart = {                          \
            .uart_port = UART_NUM_1,       \
            .rx_pin = GPIO_NUM_36,         \
            .baud_rate = 9600,             \
            .data_bits = UART_DATA_8_BITS, \
            .parity = UART_PARITY_DISABLE, \
            .stop_bits = UART_STOP_BITS_1, \
            .event_queue_size = 16         \
        }                                  \
    }

class WatchGps
{
public:
    typedef enum
    {
        GPS_FIX_INVALID, /*!< Not fixed */
        GPS_FIX_GPS,     /*!< GPS */
        GPS_FIX_DGPS,    /*!< Differential GPS */
    } gps_fix_t;

    typedef enum
    {
        GPS_MODE_INVALID = 1, /*!< Not fixed */
        GPS_MODE_2D,          /*!< 2D GPS */
        GPS_MODE_3D           /*!< 3D GPS */
    } gps_fix_mode_t;

    typedef struct
    {
        uint8_t num;       /*!< Satellite number */
        uint8_t elevation; /*!< Satellite elevation */
        uint16_t azimuth;  /*!< Satellite azimuth */
        uint8_t snr;       /*!< Satellite signal noise ratio */
    } gps_satellite_t;

    typedef struct
    {
        uint8_t hour;      /*!< Hour */
        uint8_t minute;    /*!< Minute */
        uint8_t second;    /*!< Second */
        uint16_t thousand; /*!< Thousand */
    } gps_time_t;

    typedef struct
    {
        uint8_t day;   /*!< Day (start from 1) */
        uint8_t month; /*!< Month (start from 1) */
        uint16_t year; /*!< Year (start from 2000) */
    } gps_date_t;

    typedef enum
    {
        STATEMENT_UNKNOWN = 0, /*!< Unknown statement */
        STATEMENT_GGA,         /*!< GGA */
        STATEMENT_GSA,         /*!< GSA */
        STATEMENT_RMC,         /*!< RMC */
        STATEMENT_GSV,         /*!< GSV */
        STATEMENT_GLL,         /*!< GLL */
        STATEMENT_VTG,         /*!< VTG */
        STATEMENT_GPTXT,       /*!< GPTXT */
        STATEMENT_GNZDA        /*!< GNZDA */
    } nmea_statement_t;

    typedef struct
    {
        float latitude;                                                /*!< Latitude (degrees) */
        float longitude;                                               /*!< Longitude (degrees) */
        float altitude;                                                /*!< Altitude (meters) */
        gps_fix_t fix;                                                 /*!< Fix status */
        uint8_t sats_in_use;                                           /*!< Number of satellites in use */
        gps_time_t tim;                                                /*!< time in UTC */
        gps_fix_mode_t fix_mode;                                       /*!< Fix mode */
        uint8_t sats_id_in_use[GPS_MAX_SATELLITES_IN_USE];             /*!< ID list of satellite in use */
        float dop_h;                                                   /*!< Horizontal dilution of precision */
        float dop_p;                                                   /*!< Position dilution of precision  */
        float dop_v;                                                   /*!< Vertical dilution of precision  */
        uint8_t sats_in_view;                                          /*!< Number of satellites in view */
        gps_satellite_t sats_desc_in_view[GPS_MAX_SATELLITES_IN_VIEW]; /*!< Information of satellites in view */
        gps_date_t date;                                               /*!< Fix date */
        bool valid;                                                    /*!< GPS validity */
        float speed;                                                   /*!< Ground speed, unit: m/s */
        float cog;                                                     /*!< Course over ground */
        float variation;                                               /*!< Magnetic variation */
    } gps_t;

    typedef struct esp_gps_t
    {
        uint8_t item_pos;                              /*!< Current position in item */
        uint8_t item_num;                              /*!< Current item number */
        uint8_t asterisk;                              /*!< Asterisk detected flag */
        uint8_t crc;                                   /*!< Calculated CRC value */
        uint8_t parsed_statement;                      /*!< OR'd of statements that have been parsed */
        uint8_t sat_num;                               /*!< Satellite number */
        uint8_t sat_count;                             /*!< Satellite count */
        uint8_t cur_statement;                         /*!< Current statement ID */
        uint32_t all_statements;                       /*!< All statements mask */
        char item_str[NMEA_MAX_STATEMENT_ITEM_LENGTH]; /*!< Current item */
        gps_t parent;                                  /*!< Parent class */
        uart_port_t uart_port;                         /*!< Uart port number */
        uint8_t *buffer;                               /*!< Runtime buffer */
        TaskHandle_t tsk_hdl;                          /*!< NMEA Parser task handle */
        QueueHandle_t event_queue;                     /*!< UART event queue handle */
    } esp_gps_t;

    typedef struct
    {
        struct
        {
            uart_port_t uart_port;        /*!< UART port number */
            uint32_t rx_pin;              /*!< UART Rx Pin number */
            uint32_t baud_rate;           /*!< UART baud rate */
            uart_word_length_t data_bits; /*!< UART data bits length */
            uart_parity_t parity;         /*!< UART parity */
            uart_stop_bits_t stop_bits;   /*!< UART stop bits length */
            uint32_t event_queue_size;    /*!< UART event queue size */
        } uart;                           /*!< UART specific configuration */
    } nmea_parser_config_t;

    typedef void *nmea_parser_handle_t;

    typedef enum
    {
        GPS_UPDATE, /*!< GPS information has been updated */
        GPS_UNKNOWN /*!< Unknown statements detected */
    } nmea_event_id_t;

    static bool gps_tracking_stop;
    static uint32_t gps_point_saved;
    static gps_t gps_saved;
    static gps_fix_mode_t gps_status;
    static esp_gps_t global_gps;
    // static float gps_latitude;
    // static float gps_longitude;
    // static float gps_altitude;
    // static float gps_speed;
    // static char gps_datetime[21];
    // static char gps_date[11];
    // static char gps_time[9];

    static void init();
    static void tracking_task(void *pvParameter);

    static nmea_parser_handle_t nmea_parser_init(const nmea_parser_config_t *config);

    static double gps_lat;
    static double gps_lon;
    static uint32_t lon_to_tile_x(double lon);
    static uint32_t lat_lon_to_tile_y(double lat, double lon);
    static uint32_t lon_to_x(double lon);
    static uint32_t lat_lon_to_y(double lat, double lon);

private:
    // static void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void nmea_parser_task_entry(void *arg);
    static esp_err_t gps_decode(uint8_t *buff_uart, size_t len);
    static esp_err_t parse_item(esp_gps_t *esp_gps);
    static void parse_gptxt(esp_gps_t *esp_gps);
    static void parse_vtg(esp_gps_t *esp_gps);
    static void parse_gll(esp_gps_t *esp_gps);
    static void parse_gnzda(esp_gps_t *esp_gps);
    static void parse_rmc(esp_gps_t *esp_gps);
    static void parse_gsv(esp_gps_t *esp_gps);
    static void parse_gsa(esp_gps_t *esp_gps);
    static void parse_gga(esp_gps_t *esp_gps);
    static void parse_utc_time(esp_gps_t *esp_gps);
    static float parse_lat_long(esp_gps_t *esp_gps);
    static inline uint8_t convert_two_digit2number(const char *digit_char);
};

#endif