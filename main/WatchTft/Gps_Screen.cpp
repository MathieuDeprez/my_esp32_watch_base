#include "WatchTft.h"

lv_obj_t *WatchTft::lcd_gps_screen = NULL;
lv_obj_t *WatchTft::label_title_gps = NULL;
lv_obj_t *WatchTft::label_gps_info = NULL;

void WatchTft::gps_screen()
{
    lv_obj_t *gps_bg = NULL;

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {

        if (lcd_gps_screen != NULL)
        {
            lv_obj_clean(lcd_gps_screen);
            lcd_gps_screen = NULL;
        }
        lcd_gps_screen = lv_obj_create(NULL);
        lv_scr_load(lcd_gps_screen);
        current_screen = lcd_gps_screen;
        lv_obj_clear_flag(lcd_gps_screen, LV_OBJ_FLAG_SCROLLABLE);

        static lv_style_t gps_bg_style;
        lv_style_init(&gps_bg_style);
        lv_style_set_bg_color(&gps_bg_style, lv_color_black());
        lv_style_set_radius(&gps_bg_style, 0);
        lv_style_set_border_width(&gps_bg_style, 0);
        lv_style_set_pad_all(&gps_bg_style, 0);

        gps_bg = lv_obj_create(lcd_gps_screen);
        lv_obj_set_size(gps_bg, 240, 210);
        lv_obj_align(gps_bg, LV_ALIGN_TOP_LEFT, 0, 30);
        lv_obj_clear_flag(gps_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(gps_bg, &gps_bg_style, LV_PART_MAIN);

        lv_obj_t *btn_home = lv_btn_create(gps_bg);
        lv_obj_align(btn_home, LV_ALIGN_BOTTOM_LEFT, 5, -5);
        lv_obj_set_size(btn_home, 48, 35);
        lv_obj_set_style_bg_opa(btn_home, 0, LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_home = LCD_BTN_EVENT::RETURN_HOME;
        lv_obj_add_event_cb(btn_home, event_handler_main, LV_EVENT_CLICKED, &cmd_home);

        lv_obj_t *img_arrow_left = lv_img_create(btn_home);
        lv_img_set_src(img_arrow_left, &arrow);
        lv_obj_align(img_arrow_left, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_arrow_left, 24, 24);
        lv_img_set_angle(img_arrow_left, 1800);
        lv_obj_add_style(img_arrow_left, &img_recolor_white_style, LV_PART_MAIN);

        label_title_gps = lv_label_create(gps_bg);
        lv_obj_align(label_title_gps, LV_ALIGN_TOP_LEFT, 5, 0);
        lv_obj_set_style_text_align(label_title_gps, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_title_gps, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_label_set_text(label_title_gps, "Looking for GPS...");

        label_gps_info = lv_label_create(gps_bg);
        lv_obj_align(label_gps_info, LV_ALIGN_TOP_LEFT, 5, 20);
        lv_obj_set_style_text_align(label_gps_info, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_gps_info, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(label_gps_info, "...");

        lv_obj_t *btn_start_tracking = lv_btn_create(gps_bg);
        lv_obj_align(btn_start_tracking, LV_ALIGN_BOTTOM_MID, 0, -40);
        lv_obj_set_size(btn_start_tracking, 60, 60);
        static LCD_BTN_EVENT cmd_gps_tracking = LCD_BTN_EVENT::GPS_TRACKING;
        lv_obj_add_event_cb(btn_start_tracking, event_handler_main, LV_EVENT_CLICKED, &cmd_gps_tracking);

        lv_obj_t *img_run = lv_img_create(btn_start_tracking);
        lv_img_set_src(img_run, &run);
        lv_obj_align(img_run, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_run, 24, 24);
        lv_obj_add_style(img_run, &img_recolor_white_style, LV_PART_MAIN);

        display_top_bar(lcd_gps_screen, "GPS");

        xSemaphoreGive(xGuiSemaphore);
    }
    WatchGps::init();
}

void WatchTft::gps_print_title(const char *info)
{
    if (pdTRUE != xSemaphoreTake(xGuiSemaphore, 50))
    {
        return;
    }
    if (label_title_gps)
        lv_label_set_text(label_title_gps, info);
    xSemaphoreGive(xGuiSemaphore);
}

void WatchTft::gps_print_info(const char *info)
{
    if (pdTRUE != xSemaphoreTake(xGuiSemaphore, 50))
    {
        return;
    }
    if (label_gps_info)
        lv_label_set_text(label_gps_info, info);
    xSemaphoreGive(xGuiSemaphore);
}

