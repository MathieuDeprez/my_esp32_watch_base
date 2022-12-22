#include "WatchTft.h"

// chart power
lv_obj_t *WatchTft::chart_power = NULL;
lv_obj_t *WatchTft::label_chart_usb;
lv_obj_t *WatchTft::label_chart_bat;
lv_chart_series_t *WatchTft::power_chart_ser_bat = NULL;
lv_chart_series_t *WatchTft::power_chart_ser_usb = NULL;

void WatchTft::power_screen()
{
    lv_obj_t *power_bg = NULL;

    static bool init_style_done = false;
    static lv_style_t power_bg_style;
    if (!init_style_done)
    {
        lv_style_init(&power_bg_style);
        lv_style_set_bg_color(&power_bg_style, lv_color_black());
        lv_style_set_radius(&power_bg_style, 0);
        lv_style_set_border_width(&power_bg_style, 0);
        lv_style_set_pad_all(&power_bg_style, 0);
        init_style_done = true;
    }

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {

        power_bg = lv_obj_create(current_screen);
        lv_obj_set_size(power_bg, 240, 210);
        lv_obj_align(power_bg, LV_ALIGN_TOP_LEFT, 0, 30);
        lv_obj_clear_flag(power_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(power_bg, &power_bg_style, LV_PART_MAIN);

        lv_obj_t *btn_home = lv_btn_create(power_bg);
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

        lv_obj_t *label_chart_unit = lv_label_create(power_bg);
        lv_obj_align(label_chart_unit, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_set_style_text_align(label_chart_unit, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_chart_unit, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_label_set_text(label_chart_unit, "(mA)");

        label_chart_usb = lv_label_create(power_bg);
        lv_obj_align(label_chart_usb, LV_ALIGN_BOTTOM_RIGHT, -10, -30);
        lv_obj_set_style_text_align(label_chart_usb, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(label_chart_usb, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
        lv_label_set_text(label_chart_usb, "USB   417ma");

        label_chart_bat = lv_label_create(power_bg);
        lv_obj_align(label_chart_bat, LV_ALIGN_BOTTOM_RIGHT, -10, -15);
        lv_obj_set_style_text_align(label_chart_bat, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(label_chart_bat, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_label_set_text(label_chart_bat, "BATT 536ma");

        chart_power = lv_chart_create(power_bg);
        lv_obj_set_size(chart_power, 185, 130);
        lv_obj_align(chart_power, LV_ALIGN_TOP_LEFT, 45, 25);
        lv_chart_set_type(chart_power, LV_CHART_TYPE_LINE);
        lv_chart_set_range(chart_power, LV_CHART_AXIS_PRIMARY_Y, 0, 500);
        lv_chart_set_range(chart_power, LV_CHART_AXIS_SECONDARY_Y, 0, 500);
        lv_chart_set_axis_tick(chart_power, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 6, 2, true, 50);
        power_chart_ser_bat = lv_chart_add_series(chart_power, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
        power_chart_ser_usb = lv_chart_add_series(chart_power, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_SECONDARY_Y);
        for (uint8_t i = 0; i < 10; i++)
        {
            power_chart_ser_bat->y_points[i] = 0;
            power_chart_ser_usb->y_points[i] = 0;
        }

        lv_chart_refresh(chart_power);

        xSemaphoreGive(xGuiSemaphore);
    }
}

void WatchTft::add_chart_power_value(lv_coord_t batt, lv_coord_t vbus)
{
    if (power_chart_ser_bat == NULL || power_chart_ser_usb == NULL)
    {
        return;
    }
    for (uint8_t i = 0; i < 9; i++)
    {
        power_chart_ser_bat->y_points[i] = power_chart_ser_bat->y_points[i + 1];
        power_chart_ser_usb->y_points[i] = power_chart_ser_usb->y_points[i + 1];
    }
    power_chart_ser_bat->y_points[9] = batt;
    power_chart_ser_usb->y_points[9] = vbus;

    char usb_str[15] = {};
    sprintf(usb_str, "USB %dma", vbus);

    char batt_str[15] = {};
    sprintf(batt_str, "BATT %dma", batt);

    lv_label_set_text(label_chart_usb, usb_str);
    lv_label_set_text(label_chart_bat, batt_str);

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 50))
    {
        lv_chart_refresh(chart_power);
        xSemaphoreGive(xGuiSemaphore);
    }
}