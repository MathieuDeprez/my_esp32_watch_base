#include "WatchPower.h"

volatile bool WatchPower::_irq = false;
bool WatchPower::is_charging = false;

void WatchPower::init()
{
    axpxx_i2c_init();

    axp202_init();

    register_int_isr();
    is_charging = axpxx_isVBUSPlug();

    xTaskCreatePinnedToCore(power_task, "power", 4096 * 2, NULL, 0, NULL, 1);
}

void WatchPower::register_int_isr()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << AXP202_INT);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_isr_handler_add(AXP202_INT, gpio_isr_handler, NULL);
    axpxx_clearIRQ();
}

void IRAM_ATTR WatchPower::gpio_isr_handler(void *arg)
{
    _irq = true;
}

void WatchPower::enter_light_sleep()
{
    printf("enter_light_sleep()\n");
    static bool is_sleeping = false;
    if (is_sleeping)
        return;

    is_sleeping = true;

    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 80};
    esp_pm_configure(&pm_config);

    gpio_config_t config = {
        .pin_bit_mask = BIT64(AXP202_INT),
        .mode = GPIO_MODE_INPUT};
    ESP_ERROR_CHECK(gpio_config(&config));

    gpio_wakeup_enable(AXP202_INT, GPIO_INTR_LOW_LEVEL);

    esp_sleep_enable_gpio_wakeup();

    WatchTft::turn_screen_off();

    int64_t t_before_us = esp_timer_get_time();
    printf("\nSleeping...\n\n");
    uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);

    esp_light_sleep_start();

    int64_t t_after_us = esp_timer_get_time();

    WatchTft::main_screen_from_sleep();

    const char *wakeup_reason;
    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_TIMER:
        wakeup_reason = "timer";
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
        wakeup_reason = "pin";
        break;
    default:
        wakeup_reason = "other";
        break;
    }

    printf("Returned from light sleep, reason: %s, t=%lld ms, slept for %lld ms\n",
           wakeup_reason, t_after_us / 1000, (t_after_us - t_before_us) / 1000);

    pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 80};
    esp_pm_configure(&pm_config);

    register_int_isr();

    is_sleeping = false;
}

void WatchPower::power_task(void *pvParameter)
{
    while (1)
    {
        if (_irq)
        {
            axpxx_readIRQ();
            _irq = false;
            if (axpxx_isPEKShortPressIRQ())
            {
                WatchTft::toggle_button_menu_view();
                WatchTft::turn_screen_on();
            }
            if (axpxx_isVbusPlugInIRQ())
            {
                printf("axpxx_isVBUSPlug()\n");
                is_charging = true;
                WatchTft::set_charge_state(is_charging);
            }
            if (axpxx_isVbusRemoveIRQ())
            {
                printf("axpxx_isVbusRemoveIRQ()\n");
                is_charging = false;
                WatchTft::set_charge_state(is_charging);
            }
            if (axpxx_isPEKLongtPressIRQ())
            {
                printf("axpxx_isPEKLongtPressIRQ()\n");
            }

            axpxx_clearIRQ();
        }
        vTaskDelay(200);

        static int64_t timer_battery_voltage = esp_timer_get_time() - 9 * 1000 * 1000;
        if ((esp_timer_get_time()) - timer_battery_voltage > 10 * 1000 * 1000)
        {
            timer_battery_voltage = esp_timer_get_time();
            BaseType_t err = xSemaphoreTake(i2c_0_semaphore, 500);
            if (err == pdTRUE)
            {
                WatchTft::set_battery_text(axpxx_getBattPercentage());
                xSemaphoreGive(i2c_0_semaphore);
            }
        }
    }
}