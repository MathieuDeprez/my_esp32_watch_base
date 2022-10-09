#include "WatchTft.h"

lv_obj_t *WatchTft::lcd_settings_screen = NULL;
lv_obj_t *WatchTft::label_desc_tmo_off = NULL;
lv_obj_t *WatchTft::label_desc_step_goal = NULL;

void WatchTft::settings_screen()
{
    lv_obj_t *settings_bg = NULL;

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {

        if (lcd_settings_screen != NULL)
        {
            lv_obj_clean(lcd_settings_screen);
            lcd_settings_screen = NULL;
        }
        lcd_settings_screen = lv_obj_create(NULL);
        lv_scr_load(lcd_settings_screen);
        current_screen = lcd_settings_screen;
        lv_obj_clear_flag(lcd_settings_screen, LV_OBJ_FLAG_SCROLLABLE);

        static lv_style_t settings_bg_style;
        lv_style_init(&settings_bg_style);
        lv_style_set_bg_color(&settings_bg_style, lv_color_black());
        lv_style_set_radius(&settings_bg_style, 0);
        lv_style_set_border_width(&settings_bg_style, 0);
        lv_style_set_pad_all(&settings_bg_style, 0);

        settings_bg = lv_obj_create(lcd_settings_screen);
        lv_obj_set_size(settings_bg, 240, 210);
        lv_obj_align(settings_bg, LV_ALIGN_TOP_LEFT, 0, 30);
        lv_obj_clear_flag(settings_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(settings_bg, &settings_bg_style, LV_PART_MAIN);

        lv_obj_t *settings_scroll = lv_obj_create(settings_bg);
        lv_obj_set_size(settings_scroll, 240, 165);
        lv_obj_align(settings_scroll, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_add_style(settings_scroll, &settings_bg_style, LV_PART_MAIN);

        lv_obj_t *btn_home = lv_btn_create(settings_bg);
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

        // Timeout Screen

        lv_obj_t *label_title_tmo_off = lv_label_create(settings_scroll);
        lv_obj_align(label_title_tmo_off, LV_ALIGN_TOP_LEFT, 5, 10);
        lv_obj_set_style_text_align(label_title_tmo_off, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label_title_tmo_off, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(label_title_tmo_off, "Timeout screen");

        label_desc_tmo_off = lv_label_create(settings_scroll);
        lv_obj_align(label_desc_tmo_off, LV_ALIGN_TOP_RIGHT, -15, 10);
        lv_obj_set_style_text_align(label_desc_tmo_off, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(label_desc_tmo_off, lv_color_white(), LV_PART_MAIN);
        std::string delay_tmo = std::to_string(timer_turn_off_screen / 1000) + "s";
        lv_label_set_text(label_desc_tmo_off, delay_tmo.c_str());

        lv_obj_t *slider_tmo_off = lv_slider_create(settings_scroll);
        lv_obj_align(slider_tmo_off, LV_ALIGN_TOP_MID, 0, 40);
        lv_obj_set_size(slider_tmo_off, 160, 20);
        lv_slider_set_range(slider_tmo_off, 5, 120);
        lv_slider_set_value(slider_tmo_off, timer_turn_off_screen / 1000, LV_ANIM_ON);
        lv_obj_set_style_bg_color(slider_tmo_off, lv_color_hex(0xed1590), LV_PART_KNOB);
        lv_obj_set_style_bg_color(slider_tmo_off, lv_palette_main(LV_PALETTE_DEEP_PURPLE), LV_PART_INDICATOR);
        lv_obj_add_event_cb(slider_tmo_off, slider_tmo_off_event_cb, LV_EVENT_ALL, NULL);

        lv_obj_t *hor_bar_01 = lv_obj_create(settings_scroll);
        lv_obj_align(hor_bar_01, LV_ALIGN_TOP_MID, 0, 70);
        lv_obj_set_size(hor_bar_01, 200, 2);
        lv_obj_set_style_bg_color(hor_bar_01, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_border_width(hor_bar_01, 0, LV_PART_MAIN);

        // Wakeup action

        lv_obj_t *label_title_wakeup = lv_label_create(settings_scroll);
        lv_obj_align(label_title_wakeup, LV_ALIGN_TOP_MID, 0, 80);
        lv_obj_set_style_text_align(label_title_wakeup, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label_title_wakeup, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(label_title_wakeup, "Wake up action");

        lv_obj_t *label_desc_wakeup_01 = lv_label_create(settings_scroll);
        lv_obj_align(label_desc_wakeup_01, LV_ALIGN_TOP_LEFT, 20, 100);
        lv_obj_set_style_text_align(label_desc_wakeup_01, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_desc_wakeup_01, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(label_desc_wakeup_01, "Double Tap");

        lv_obj_t *cb_wakeup_double_tap = lv_checkbox_create(settings_scroll);
        lv_obj_align(cb_wakeup_double_tap, LV_ALIGN_TOP_LEFT, 55, 120);
        lv_checkbox_set_text(cb_wakeup_double_tap, "");
        lv_obj_set_style_border_color(cb_wakeup_double_tap, lv_palette_main(LV_PALETTE_DEEP_PURPLE), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(cb_wakeup_double_tap, lv_color_hex(0xed1590), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (WatchBma::wakeup_on_double_tap)
        {
            lv_obj_add_state(cb_wakeup_double_tap, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(cb_wakeup_double_tap, wakeup_double_tap_event_cb, LV_EVENT_ALL, NULL);

        lv_obj_t *label_desc_wakeup_02 = lv_label_create(settings_scroll);
        lv_obj_align(label_desc_wakeup_02, LV_ALIGN_TOP_RIGHT, -65, 100);
        lv_obj_set_style_text_align(label_desc_wakeup_02, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(label_desc_wakeup_02, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(label_desc_wakeup_02, "Tilt");

        lv_obj_t *cb_wakeup_tilt = lv_checkbox_create(settings_scroll);
        lv_obj_align(cb_wakeup_tilt, LV_ALIGN_TOP_RIGHT, -55, 120);
        lv_checkbox_set_text(cb_wakeup_tilt, "");
        lv_obj_set_style_border_color(cb_wakeup_tilt, lv_palette_main(LV_PALETTE_DEEP_PURPLE), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(cb_wakeup_tilt, lv_color_hex(0xed1590), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (WatchBma::wakeup_on_tilt)
        {
            lv_obj_add_state(cb_wakeup_tilt, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(cb_wakeup_tilt, wakeup_tilt_event_cb, LV_EVENT_ALL, NULL);

        lv_obj_t *hor_bar_02 = lv_obj_create(settings_scroll);
        lv_obj_align(hor_bar_02, LV_ALIGN_TOP_MID, 0, 150);
        lv_obj_set_size(hor_bar_02, 200, 2);
        lv_obj_set_style_bg_color(hor_bar_02, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_border_width(hor_bar_02, 0, LV_PART_MAIN);

        // Step Goal

        lv_obj_t *label_title_step_goal = lv_label_create(settings_scroll);
        lv_obj_align(label_title_step_goal, LV_ALIGN_TOP_LEFT, 5, 160);
        lv_obj_set_style_text_align(label_title_step_goal, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label_title_step_goal, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(label_title_step_goal, "Steps goal");

        label_desc_step_goal = lv_label_create(settings_scroll);
        lv_obj_align(label_desc_step_goal, LV_ALIGN_TOP_RIGHT, -15, 160);
        lv_obj_set_style_text_align(label_desc_step_goal, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(label_desc_step_goal, lv_color_white(), LV_PART_MAIN);
        std::string step_goal = std::to_string(WatchBma::step_counter_goal) + " steps";
        lv_label_set_text(label_desc_step_goal, step_goal.c_str());

        lv_obj_t *slider_step_goal = lv_slider_create(settings_scroll);
        lv_obj_align(slider_step_goal, LV_ALIGN_TOP_MID, 0, 190);
        lv_obj_set_size(slider_step_goal, 160, 20);
        lv_slider_set_range(slider_step_goal, 5, 30);
        lv_slider_set_value(slider_step_goal, WatchBma::step_counter_goal / 1000, LV_ANIM_ON);
        lv_obj_set_style_bg_color(slider_step_goal, lv_color_hex(0xed1590), LV_PART_KNOB);
        lv_obj_set_style_bg_color(slider_step_goal, lv_palette_main(LV_PALETTE_DEEP_PURPLE), LV_PART_INDICATOR);
        lv_obj_add_event_cb(slider_step_goal, slider_step_goal_event_cb, LV_EVENT_ALL, NULL);

        lv_obj_t *hor_bar_03 = lv_obj_create(settings_scroll);
        lv_obj_align(hor_bar_03, LV_ALIGN_TOP_MID, 0, 220);
        lv_obj_set_size(hor_bar_03, 200, 2);
        lv_obj_set_style_bg_color(hor_bar_03, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_border_width(hor_bar_03, 0, LV_PART_MAIN);

        display_top_bar(lcd_settings_screen, "Settings");

        xSemaphoreGive(xGuiSemaphore);
    }
}

void WatchTft::slider_tmo_off_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider = lv_event_get_target(e);
    timer_turn_off_screen = lv_slider_get_value(slider) * 1000;
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        std::string delay_tmo = std::to_string(timer_turn_off_screen / 1000) + "s";
        lv_label_set_text(label_desc_tmo_off, delay_tmo.c_str());
    }

    if (code == LV_EVENT_RELEASED)
    {
        WatchNvs::set_tmo_screen(timer_turn_off_screen);
    }
}

void WatchTft::wakeup_double_tap_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code != LV_EVENT_VALUE_CHANGED)
    {
        return;
    }
    lv_state_t state = lv_obj_get_state(obj);
    WatchBma::wakeup_on_double_tap = state & LV_STATE_CHECKED;
    WatchNvs::set_wakeup_double_tap(WatchBma::wakeup_on_double_tap);
    printf("lv_State dt: %d\n", state);
}

void WatchTft::wakeup_tilt_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code != LV_EVENT_VALUE_CHANGED)
    {
        return;
    }
    lv_state_t state = lv_obj_get_state(obj);
    WatchBma::wakeup_on_tilt = state & LV_STATE_CHECKED;
    WatchNvs::set_wakeup_tilt(WatchBma::wakeup_on_tilt);
    printf("lv_State ti: %d\n", state);
}

void WatchTft::slider_step_goal_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider = lv_event_get_target(e);
    WatchBma::step_counter_goal = lv_slider_get_value(slider) * 1000;
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        std::string step_goal = std::to_string(WatchBma::step_counter_goal) + " steps";
        lv_label_set_text(label_desc_step_goal, step_goal.c_str());
    }

    if (code == LV_EVENT_RELEASED)
    {
        WatchNvs::set_step_goal(WatchBma::step_counter_goal);
    }
}