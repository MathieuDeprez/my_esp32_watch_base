#include "WatchWiFi.h"

EventGroupHandle_t WatchWiFi::s_wifi_event_group = NULL;
bool WatchWiFi::connected = false;
bool WatchWiFi::enable = false;

void WatchWiFi::init()
{
    printf("WatchWiFi::init()\n");

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_sta_config_t wifi_sta_config = {};
    strcpy((char *)wifi_sta_config.ssid, WIFI_SSID);
    strcpy((char *)wifi_sta_config.password, WIFI_PASSWORD);

    wifi_config_t wifi_config = {
        .sta = wifi_sta_config,
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    printf("wifi_init_sta finished.\n");
}

void WatchWiFi::connect()
{
    printf("WatchWiFi::connect\n");
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WatchWiFi::disconnect()
{
    printf("WatchWiFi::disconnect\n");
    ESP_ERROR_CHECK(esp_wifi_stop());
}

void WatchWiFi::event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        if (enable)
        {
            esp_wifi_connect();
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {

        printf("connect to the AP fail\n");

        connected = false;
        WatchTft::set_wifi_state(enable, connected);
        if (enable)
        {
            esp_wifi_connect();
            printf("retry to connect to the AP\n");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        printf("got ip:" IPSTR "\n", IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        connected = true;
        WatchTft::set_wifi_state(enable, connected);
    }
}

esp_err_t WatchWiFi::_http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        printf("HTTP_EVENT_ERROR\n");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        printf("HTTP_EVENT_ON_CONNECTED\n");
        break;
    case HTTP_EVENT_HEADER_SENT:
        printf("HTTP_EVENT_HEADER_SENT\n");
        break;
    case HTTP_EVENT_ON_HEADER:
        printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s\n", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA, len=%d\n", evt->data_len);
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // If user_data buffer is configured, copy the response into the buffer
            if (evt->user_data)
            {
                memcpy(evt->user_data + output_len, evt->data, evt->data_len);
            }
            else
            {
                if (output_buffer == NULL)
                {
                    output_buffer = (char *)malloc(esp_http_client_get_content_length(evt->client));
                    output_len = 0;
                    if (output_buffer == NULL)
                    {
                        printf("Failed to allocate memory for output buffer\n");
                        return ESP_FAIL;
                    }
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
            }
            output_len += evt->data_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        printf("HTTP_EVENT_ON_FINISH\n");
        if (output_buffer != NULL)
        {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        printf("HTTP_EVENT_DISCONNECTED\n");
        int mbedtls_err = 0;
        if (output_buffer != NULL)
        {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    }
    return ESP_OK;
}

void WatchWiFi::download_tile(int32_t x, int32_t y)
{
    printf("download_tile: %d:%d\n", x, y);

    if (!connected)
    {
        printf("Can't download tile, not connected\n");
        return;
    }
    char tile_url[60] = {};
    sprintf(tile_url, "http://a.tile.openstreetmap.org/15/%u/%u.png", x, y);

    char tile_filename[60] = {};
    sprintf(tile_filename, MOUNT_POINT "/map_15_%u_%u.png", x, y);

    FILE *f_tile = NULL;
    f_tile = fopen(tile_filename, "w");

    char output_buffer[50] = {0}; // Buffer to store response of http request
    int content_length = 0;
    esp_http_client_config_t config = {
        .url = tile_url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET Request
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        printf("Failed to open HTTP connection: %s\n", esp_err_to_name(err));
    }
    else
    {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            printf("HTTP client fetch headers failed\n");
        }
        else
        {
            uint8_t body_data[1024] = {};
            uint32_t size_writen = 0;
            int data_read = esp_http_client_read_response(client, (char *)body_data, sizeof(body_data));
            if (data_read >= 0)
            {
                printf("HTTP GET Status = %d, content_length = %d, data_read = %d\n",
                       esp_http_client_get_status_code(client),
                       esp_http_client_get_content_length(client),
                       data_read);
                if (f_tile != NULL)
                {
                    fwrite(body_data, data_read, 1, f_tile);
                    size_writen += data_read;
                    while (data_read > 0)
                    {
                        data_read = esp_http_client_read_response(client, (char *)body_data, sizeof(body_data));
                        if (data_read > 0)
                        {
                            fwrite(body_data, data_read, 1, f_tile);
                            size_writen += data_read;
                        }
                    }
                    printf("Write tile file DONE: %u !!!\n", size_writen);
                }
                else
                {
                    printf("f_tile NULL\n");
                }
            }
            else
            {
                printf("Failed to read response\n");
            }
        }
    }
    esp_http_client_close(client);

    esp_http_client_cleanup(client);

    fclose(f_tile);
    WatchTft::refresh_tile(x, y);
}
