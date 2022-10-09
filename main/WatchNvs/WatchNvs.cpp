#include "WatchNvs.h"

char WatchNvs::main_storage_name[7] = "stg_01";

void WatchNvs::init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    err = nvs_open(main_storage_name, NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        vTaskDelay(1000);
        esp_restart();
    }
    printf("Done\n");

    // Read tmo_screen
    printf("Reading tmo screen from NVS ... ");
    uint32_t tmo_screen = 60000; // value will default to 0, if not set yet in NVS
    err = nvs_get_u32(my_handle, NVS_KEY_TIMEOUT_SCREEN, &tmo_screen);
    switch (err)
    {
    case ESP_OK:
        printf("Done\n");
        printf("tmo_screen = %d\n", tmo_screen);
        timer_turn_off_screen = tmo_screen;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf("The value is not initialized yet!\n");
        break;
    default:
        printf("Error (%s) reading!\n", esp_err_to_name(err));
    }

    // Read wakeup double tap
    printf("Reading wakeup double tap from NVS ... ");
    uint8_t wakeup_double_tap = 1; // value will default to 0, if not set yet in NVS
    err = nvs_get_u8(my_handle, NVS_KEY_WAKEUP_DOUBLE_TAP, &wakeup_double_tap);
    switch (err)
    {
    case ESP_OK:
        printf("Done\n");
        printf("wakeup_double_tap = %d\n", wakeup_double_tap);
        WatchBma::wakeup_on_double_tap = wakeup_double_tap;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf("The value is not initialized yet!\n");
        break;
    default:
        printf("Error (%s) reading!\n", esp_err_to_name(err));
    }

    // Read wakeup tilt
    printf("Reading wakeup tilt from NVS ... ");
    uint8_t wakeup_tilt = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_u8(my_handle, NVS_KEY_WAKEUP_TILT, &wakeup_tilt);
    switch (err)
    {
    case ESP_OK:
        printf("Done\n");
        printf("wakeup_tilt = %d\n", wakeup_tilt);
        WatchBma::wakeup_on_tilt = wakeup_tilt;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf("The value is not initialized yet!\n");
        break;
    default:
        printf("Error (%s) reading!\n", esp_err_to_name(err));
    }

    nvs_close(my_handle);
}

void WatchNvs::set_tmo_screen(uint32_t value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(main_storage_name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        vTaskDelay(1000);
        esp_restart();
    }

    err |= nvs_set_u32(my_handle, NVS_KEY_TIMEOUT_SCREEN, value);
    err |= nvs_commit(my_handle);

    if (err != ESP_OK)
    {
        printf("set_tmo_screen failed (%s)!\n", esp_err_to_name(err));
        vTaskDelay(1000);
        esp_restart();
    }

    nvs_close(my_handle);
}

void WatchNvs::set_wakeup_double_tap(uint8_t value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(main_storage_name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        vTaskDelay(1000);
        esp_restart();
    }

    err |= nvs_set_u8(my_handle, NVS_KEY_WAKEUP_DOUBLE_TAP, value);
    err |= nvs_commit(my_handle);

    if (err != ESP_OK)
    {
        printf("set_wakeup_double_tap failed (%s)!\n", esp_err_to_name(err));
        vTaskDelay(1000);
        esp_restart();
    }

    nvs_close(my_handle);
}

void WatchNvs::set_wakeup_tilt(uint8_t value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(main_storage_name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        vTaskDelay(1000);
        esp_restart();
    }

    err |= nvs_set_u8(my_handle, NVS_KEY_WAKEUP_TILT, value);
    err |= nvs_commit(my_handle);

    if (err != ESP_OK)
    {
        printf("set_wakeup_tilt failed (%s)!\n", esp_err_to_name(err));
        vTaskDelay(1000);
        esp_restart();
    }

    nvs_close(my_handle);
}
