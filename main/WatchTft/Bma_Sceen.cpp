#include "WatchTft.h"

lv_obj_t *WatchTft::lcd_run_screen = NULL;
lv_obj_t *WatchTft::label_step_count = NULL;
lv_obj_t *WatchTft::arc_counter = NULL;

void WatchTft::run_screen()
{
    lv_obj_t *run_bg = NULL;

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {

        if (lcd_run_screen != NULL)
        {
            lv_obj_clean(lcd_run_screen);
            lcd_run_screen = NULL;
        }
        lcd_run_screen = lv_obj_create(NULL);
        lv_scr_load(lcd_run_screen);
        current_screen = lcd_run_screen;
        lv_obj_clear_flag(lcd_run_screen, LV_OBJ_FLAG_SCROLLABLE);

        static lv_style_t run_bg_style;
        lv_style_init(&run_bg_style);
        lv_style_set_bg_color(&run_bg_style, lv_color_black());
        lv_style_set_radius(&run_bg_style, 0);
        lv_style_set_border_width(&run_bg_style, 0);
        lv_style_set_pad_all(&run_bg_style, 0);

        run_bg = lv_obj_create(lcd_run_screen);
        lv_obj_set_size(run_bg, 240, 210);
        lv_obj_align(run_bg, LV_ALIGN_TOP_LEFT, 0, 30);
        lv_obj_clear_flag(run_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(run_bg, &run_bg_style, LV_PART_MAIN);

        lv_obj_t *btn_home = lv_btn_create(run_bg);
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

        lv_obj_t *label_title_run = lv_label_create(run_bg);
        lv_obj_align(label_title_run, LV_ALIGN_CENTER, 0, -32);
        lv_obj_set_style_text_align(label_title_run, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label_title_run, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_label_set_text(label_title_run, "Steps");

        label_step_count = lv_label_create(run_bg);
        lv_obj_align(label_step_count, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_align(label_step_count, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label_step_count, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(label_step_count, &lv_font_montserrat_48, 0);
        std::string start_steps = std::to_string(WatchBma::steps_count_save);
        lv_label_set_text(label_step_count, start_steps.c_str());

        arc_counter = lv_arc_create(run_bg);
        lv_obj_align(arc_counter, LV_ALIGN_CENTER, 0, 0);
        lv_arc_set_rotation(arc_counter, 270);
        uint16_t start_angle = (WatchBma::steps_count_save % STEP_COUNTER_GOAL) * 360 / STEP_COUNTER_GOAL;
        lv_arc_set_angles(arc_counter, 0, start_angle);
        lv_arc_set_bg_angles(arc_counter, 0, 360);
        lv_obj_set_size(arc_counter, 160, 160);
        lv_obj_set_style_arc_img_src(arc_counter, &gradient_count, LV_PART_INDICATOR);
        lv_obj_remove_style(arc_counter, NULL, LV_PART_KNOB);  /*Be sure the knob is not displayed*/
        lv_obj_clear_flag(arc_counter, LV_OBJ_FLAG_CLICKABLE); /*To not allow adjusting by click*/
        lv_obj_center(arc_counter);
        lv_obj_set_style_arc_rounded(arc_counter, 0, LV_PART_INDICATOR);

        lv_obj_t *label_goal_step = lv_label_create(run_bg);
        lv_obj_align(label_goal_step, LV_ALIGN_CENTER, 0, 32);
        lv_obj_set_style_text_align(label_goal_step, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label_goal_step, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        std::string goal_text = LV_SYMBOL_EYE_OPEN + std::to_string(STEP_COUNTER_GOAL);
        lv_label_set_text(label_goal_step, goal_text.c_str());

        display_top_bar(lcd_run_screen, "Run");

        xSemaphoreGive(xGuiSemaphore);
    }
    TaskHandle_t count_step_task_hanlde = NULL;
    xTaskCreatePinnedToCore(count_step_task, "count_step", 4096 * 2, NULL, 0, &count_step_task_hanlde, 1);
    current_task_hanlde = count_step_task_hanlde;
}

void WatchTft::set_step_counter_value(uint32_t steps)
{
    if (pdTRUE != xSemaphoreTake(xGuiSemaphore, 50))
    {
        return;
    }
    uint16_t angle = (steps % STEP_COUNTER_GOAL) * 360 / STEP_COUNTER_GOAL;
    lv_arc_set_angles(arc_counter, 0, angle);
    std::string step_str = std::to_string(steps);
    lv_label_set_text(label_step_count, step_str.c_str());
    xSemaphoreGive(xGuiSemaphore);
}

void WatchTft::count_step_task(void *pvParameter)
{
    printf("Move/perform the walk/step action with the sensor\n");
    while (1)
    {
        uint16_t int_status = WatchBma::s_int_status;
        WatchBma::s_int_status = 0;
        if (int_status & BMA423_STEP_CNTR_INT)
        {
            WatchBma::steps_count_save = WatchBma::get_steps();
            set_step_counter_value(WatchBma::steps_count_save);
        }

        vTaskDelay(1000);
    }
}
