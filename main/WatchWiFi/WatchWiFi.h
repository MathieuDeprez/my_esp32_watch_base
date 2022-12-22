#include "main.h"
#include "wifi_credentials.h"

#ifndef WATCH_WIFI
#define WATCH_WIFI

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

class WatchWiFi
{

public:
    static EventGroupHandle_t s_wifi_event_group;
    static bool connected;
    static bool enable;

    static void init();
    static void connect();
    static void disconnect();
    static void download_tile(int32_t x, int32_t y);

private:
    static void event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);

    static esp_err_t _http_event_handler(esp_http_client_event_t *evt);
};
#endif