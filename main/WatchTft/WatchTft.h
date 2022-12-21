#include "main.h"
#include "lvgl/lvgl.h"
#include "lvgl_helpers.h"

#ifndef WATCH_TFT
#define WATCH_TFT

#define LV_TICK_PERIOD_MS 2

#define DELAY_BETWEEN_LV(X)                                     \
    xSemaphoreGive(xGuiSemaphore);                              \
    }                                                           \
    vTaskDelay(X);                                              \
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) \
    {

// #define DISPLAY_TOUCH_LIMIT

class WatchTft
{

public:
    static bool screen_en;
    static TaskHandle_t current_task_hanlde;

    static const lv_img_dsc_t download;

    static void init();

    static void turn_screen_on();
    static void turn_screen_off();
    static void turn_off_screen();
    static void set_battery_text(uint8_t percent);
    static void toggle_button_menu_view();

    static void main_screen_from_sleep();
    static void add_chart_power_value(lv_coord_t batt, lv_coord_t vbus);

    static void set_gps_info(bool fixed, uint8_t in_use, float dop, double lat, double lon);
    static void set_gps_tracking_hidden_state(bool status);

    static void refresh_tile(int32_t x, int32_t y);
    static void add_gps_pos(int32_t x, int32_t y);

private:
    enum class LCD_CMD : uint32_t
    {
        TURN_OFF_SCREEN,
        SLEEP_SCREEN,
        RESET_ESP,
        RETURN_HOME,
        POWER_SCREEN,
        RUN_SCREEN,
        SETTINGS_SCREEN,
        GPS_SCREEN,
    };

    enum class LCD_BTN_EVENT : uint32_t
    {
        TURN_OFF_SCREEN,
        SLEEP_SCREEN,
        MAIN_PAGE,
        NEXT_PAGE,
        PREVIOUS_PAGE,
        RESET_ESP,
        TOP_BAR_TOGGLE,
        RETURN_HOME,
        POWER_SCREEN,
        RUN_SCREEN,
        SETTINGS_SCREEN,
        GPS_SCREEN,
        GPS_TRACKING,
        CENTER_GPS,
        CLEAN_FILE,
    };

    static lv_obj_t *current_screen;
    static lv_obj_t *main_screen;

    // images
    static const lv_img_dsc_t lune;
    static const lv_img_dsc_t ampoule;
    static const lv_img_dsc_t main_bg;
    static const lv_img_dsc_t gradient_count;
    static const lv_img_dsc_t rotate;
    static const lv_img_dsc_t arrow;
    static const lv_img_dsc_t circle;
    static const lv_img_dsc_t run;

    // top menu
    static lv_obj_t *label_battery;
    static lv_obj_t *top_menu;
    static lv_obj_t *main_label_battery;
    static lv_obj_t *main_top_menu;
    static lv_obj_t *slider_top_bl;
    static lv_obj_t *main_slider_top_bl;
    static uint8_t s_battery_percent;

    // power screen
    static lv_obj_t *lcd_power_screen;
    static lv_obj_t *chart_power;
    static lv_obj_t *label_chart_usb;
    static lv_obj_t *label_chart_bat;
    static lv_chart_series_t *power_chart_ser_bat;
    static lv_chart_series_t *power_chart_ser_usb;

    // run screen
    static lv_obj_t *lcd_run_screen;
    static lv_obj_t *label_step_count;
    static lv_obj_t *arc_counter;

    // settings screen
    static lv_obj_t *lcd_settings_screen;
    static lv_obj_t *label_desc_tmo_off;
    static lv_obj_t *label_desc_step_goal;

    // gps screen
    static lv_obj_t *lcd_gps_screen;
    static lv_obj_t *label_title_gps;
    static lv_obj_t *label_gps_info;
    static lv_obj_t *gps_recording_indicator;

    // physical button menu
    static lv_obj_t *button_menu;
    static lv_obj_t *lcd_turn_off_screen;
    static lv_obj_t *lcd_reset_screen;
    static lv_obj_t *lcd_sleep_screen;

    static uint8_t bl_value;
    static disp_backlight_h bckl_handle;
    static lv_style_t img_recolor_white_style;

    static SemaphoreHandle_t xGuiSemaphore;
    static QueueHandle_t xQueueLcdCmd;

    static void lv_tick_task(void *arg);
    static void gui_task(void *pvParameter);
    static void queue_cmd_task(void *pvParameter);
    static void init_home_screen(void);
    static void slider_event_cb(lv_event_t *e);
    static void set_bl(uint8_t value);
    static void toggle_top_bar_view();
    static void event_handler_main(lv_event_t *e);
    static void sleep_screen();
    static void reset_screen();
    static void power_screen();
    static void return_home_screen();
    static void display_top_bar(lv_obj_t *parent, const char *title);
    static void show_button_menu();

    static void run_screen();
    static void count_step_task(void *pvParameter);
    static void set_step_counter_value(uint32_t steps);

    static void settings_screen();
    static void slider_tmo_off_event_cb(lv_event_t *e);
    static void wakeup_double_tap_event_cb(lv_event_t *e);
    static void wakeup_tilt_event_cb(lv_event_t *e);
    static void slider_step_goal_event_cb(lv_event_t *e);

    static void gps_screen();
    static void center_gps();
    static lv_point_t drag_view(int32_t x, int32_t y, bool sem_taken = false);
    static void drag_event_handler(lv_event_t *e);
    static void add_xy_to_gps_point(int32_t x, int32_t y, bool sem_taken = false);
};

#endif