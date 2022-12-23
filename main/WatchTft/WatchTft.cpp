#include "WatchTft.h"

QueueHandle_t WatchTft::xQueueLcdCmd;
/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t WatchTft::xGuiSemaphore;
TaskHandle_t WatchTft::current_task_hanlde = NULL;

lv_style_t WatchTft::img_recolor_white_style;

disp_backlight_h WatchTft::bckl_handle;
uint8_t WatchTft::bl_value = 75;
bool WatchTft::screen_en = true;

WatchTft::screen_index_t WatchTft::current_screen_index = screen_index_t::main;

// top menu
lv_obj_t *WatchTft::label_battery = NULL;
lv_obj_t *WatchTft::slider_top_bl;
uint8_t WatchTft::s_battery_percent = 0;

// physical button menu
lv_obj_t *WatchTft::lcd_turn_off_screen = NULL;
lv_obj_t *WatchTft::lcd_reset_screen = NULL;
lv_obj_t *WatchTft::lcd_sleep_screen = NULL;

lv_obj_t *WatchTft::current_screen = NULL;
lv_obj_t *WatchTft::top_menu = NULL;
lv_obj_t *WatchTft::button_menu = NULL;

lv_indev_t *touch_driver = NULL;
lv_indev_drv_t indev_drv;

static EventGroupHandle_t lvgl_event_group;
const int LVGL_INIT_DONE = BIT0;

lv_obj_t *WatchTft::img_wifi = NULL;
lv_obj_t *WatchTft::line_wifi = NULL;
lv_obj_t *WatchTft::img_gps = NULL;
lv_obj_t *WatchTft::line_gps = NULL;

lv_point_t WatchTft::wifi_line_points[2] = {};
lv_point_t WatchTft::gps_line_points[2] = {};

void WatchTft::init()
{
    xGuiSemaphore = xSemaphoreCreateMutex();
    lvgl_event_group = xEventGroupCreate();

    xQueueLcdCmd = xQueueCreate(10, sizeof(LCD_CMD));
    xTaskCreatePinnedToCore(gui_task, "gui", 4096 * 2, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(queue_cmd_task, "queue_cmd", 4096 * 2, NULL, 0, NULL, 1);

    xEventGroupWaitBits(lvgl_event_group, LVGL_INIT_DONE, true, true, portMAX_DELAY);
    init_home_screen();
}

void WatchTft::queue_cmd_task(void *pvParameter)
{
    while (1)
    {
        LCD_CMD cmd = LCD_CMD::TURN_OFF_SCREEN;
        xQueueReceive(xQueueLcdCmd, &cmd, portMAX_DELAY);
        switch (cmd)
        {
        case LCD_CMD::TURN_OFF_SCREEN:
            printf("SHOW_TURN_OFF_SCREEN\n");
            turn_off_screen();
            break;
        case LCD_CMD::SLEEP_SCREEN:
            printf("SHOW_SLEEP_SCREEN\n");
            sleep_screen();
            break;
        case LCD_CMD::RESET_ESP:
            printf("RESET_ESP\n");
            reset_screen();
            break;
        case LCD_CMD::RETURN_HOME:
            printf("RETURN_HOME\n");
            load_screen(screen_index_t::main);
            break;
        case LCD_CMD::POWER_SCREEN:
            printf("POWER_SCREEN\n");
            load_screen(screen_index_t::power);
            break;
        case LCD_CMD::RUN_SCREEN:
            printf("RUN_SCREEN\n");
            load_screen(screen_index_t::run);
            break;
        case LCD_CMD::SETTINGS_SCREEN:
            printf("SETTINGS_SCREEN\n");
            load_screen(screen_index_t::settings);
            break;
        case LCD_CMD::GPS_SCREEN:
            printf("GPS_SCREEN\n");
            load_screen(screen_index_t::gps);
            break;

        default:
            printf("unknow LCD CMD");
            break;
        }
    }
}

void WatchTft::gui_task(void *pvParameter)
{
    (void)pvParameter;

    lv_init();

    lv_png_init();

    /* Initialize SPI or I2C bus used by the drivers */
    bckl_handle = lvgl_driver_init();

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    static lv_disp_draw_buf_t disp_buf;

    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);
    bzero(buf1, DISP_BUF_SIZE * sizeof(lv_color_t));

    /* Use double buffered when not working with monochrome displays */
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);
    bzero(buf2, DISP_BUF_SIZE * sizeof(lv_color_t));

    uint32_t size_in_px = DISP_BUF_SIZE;

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);

    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.sw_rotate = 0;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    touch_driver = lv_indev_drv_register(&indev_drv);
#endif

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    xEventGroupSetBits(lvgl_event_group, LVGL_INIT_DONE);

    while (1)
    {
        vTaskDelay(10 / portTICK_RATE_MS);

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    /* A task should NEVER return */
    free(buf1);
    free(buf2);
    vTaskDelete(NULL);
}

void WatchTft::init_home_screen(void)
{
    printf("init_home_screen()\n");
    set_bl(75);

    lv_style_init(&img_recolor_white_style);
    lv_style_set_img_recolor_opa(&img_recolor_white_style, 255);
    lv_style_set_img_recolor(&img_recolor_white_style, lv_color_white());

    load_screen(screen_index_t::main);
    set_charge_state(WatchPower::is_charging);
}

void WatchTft::load_main_screen()
{
    lv_obj_t *btn_next_page = NULL;
    lv_obj_t *btn_previous_page = NULL;

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
    {
        current_screen = lv_obj_create(NULL);
        lv_scr_load(current_screen);
        lv_obj_clear_flag(current_screen, LV_OBJ_FLAG_SCROLLABLE);
        current_screen = current_screen;

        lv_obj_t *main_bg_img = lv_img_create(current_screen);
        lv_img_set_src(main_bg_img, &main_bg);
        lv_obj_align(main_bg_img, LV_ALIGN_TOP_LEFT, 0, 30);
        lv_obj_set_size(main_bg_img, 240, 210);

#ifdef DISPLAY_TOUCH_LIMIT
        lv_obj_t *rec_L = lv_obj_create(current_screen);
        lv_obj_set_size(rec_L, 25, 240);
        lv_obj_align(rec_L, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_color(rec_L, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_radius(rec_L, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(rec_L, 0, LV_PART_MAIN);

        lv_obj_t *rec_R = lv_obj_create(current_screen);
        lv_obj_set_size(rec_R, 35, 240);
        lv_obj_align(rec_R, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(rec_R, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_radius(rec_R, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(rec_R, 0, LV_PART_MAIN);
#endif

        lv_obj_t *btn_0_0 = lv_btn_create(current_screen);
        lv_obj_align(btn_0_0, LV_ALIGN_TOP_LEFT, 15, 45);
        lv_obj_set_size(btn_0_0, 60, 60);
        lv_obj_set_style_bg_opa(btn_0_0, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_0_0, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_power_screen = LCD_BTN_EVENT::POWER_SCREEN;
        lv_obj_add_event_cb(btn_0_0, event_handler_main, LV_EVENT_CLICKED, &cmd_power_screen);

        lv_obj_t *power_icon = lv_label_create(btn_0_0);
        lv_obj_align(power_icon, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(power_icon, 24, 24);
        lv_label_set_text(power_icon, LV_SYMBOL_CHARGE);
        lv_obj_set_style_text_font(power_icon, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_align(power_icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_style(power_icon, &img_recolor_white_style, LV_PART_MAIN);

        lv_obj_t *btn_0_1 = lv_btn_create(current_screen);
        lv_obj_align(btn_0_1, LV_ALIGN_TOP_LEFT, 90, 45);
        lv_obj_set_size(btn_0_1, 60, 60);
        lv_obj_set_style_bg_opa(btn_0_1, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_0_1, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_run = LCD_BTN_EVENT::RUN_SCREEN;
        lv_obj_add_event_cb(btn_0_1, event_handler_main, LV_EVENT_CLICKED, &cmd_run);

        lv_obj_t *img_run = lv_img_create(btn_0_1);
        lv_img_set_src(img_run, &run);
        lv_obj_align(img_run, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_run, 24, 24);
        lv_obj_add_style(img_run, &img_recolor_white_style, LV_PART_MAIN);

        lv_obj_t *btn_0_2 = lv_btn_create(current_screen);
        lv_obj_align(btn_0_2, LV_ALIGN_TOP_LEFT, 165, 45);
        lv_obj_set_size(btn_0_2, 60, 60);
        lv_obj_set_style_bg_opa(btn_0_2, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_0_2, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_gps = LCD_BTN_EVENT::GPS_SCREEN;
        lv_obj_add_event_cb(btn_0_2, event_handler_main, LV_EVENT_CLICKED, &cmd_gps);

        lv_obj_t *gps_icon = lv_label_create(btn_0_2);
        lv_obj_align(gps_icon, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(gps_icon, 24, 24);
        lv_label_set_text(gps_icon, LV_SYMBOL_GPS);
        lv_obj_set_style_text_font(gps_icon, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_align(gps_icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_style(gps_icon, &img_recolor_white_style, LV_PART_MAIN);

        lv_obj_t *btn_1_0 = lv_btn_create(current_screen);
        lv_obj_align(btn_1_0, LV_ALIGN_TOP_LEFT, 15, 120);
        lv_obj_set_size(btn_1_0, 60, 60);
        lv_obj_set_style_bg_opa(btn_1_0, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_1_0, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_reset = LCD_BTN_EVENT::RESET_ESP;
        lv_obj_add_event_cb(btn_1_0, event_handler_main, LV_EVENT_CLICKED, &cmd_reset);

        lv_obj_t *btn_1_1 = lv_btn_create(current_screen);
        lv_obj_align(btn_1_1, LV_ALIGN_TOP_LEFT, 90, 120);
        lv_obj_set_size(btn_1_1, 60, 60);
        lv_obj_set_style_bg_opa(btn_1_1, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_1_1, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_settings = LCD_BTN_EVENT::SETTINGS_SCREEN;
        lv_obj_add_event_cb(btn_1_1, event_handler_main, LV_EVENT_CLICKED, &cmd_settings);

        lv_obj_t *settings_icon = lv_label_create(btn_1_1);
        lv_obj_align(settings_icon, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(settings_icon, 24, 24);
        lv_label_set_text(settings_icon, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_font(settings_icon, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_align(settings_icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_style(settings_icon, &img_recolor_white_style, LV_PART_MAIN);

        lv_obj_t *btn_1_2 = lv_btn_create(current_screen);
        lv_obj_align(btn_1_2, LV_ALIGN_TOP_LEFT, 165, 120);
        lv_obj_set_size(btn_1_2, 60, 60);
        lv_obj_set_style_bg_opa(btn_1_2, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_1_2, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_add_event_cb(btn_1_2, event_handler_main, LV_EVENT_CLICKED, &cmd_reset);

        lv_obj_t *img_reboot = lv_img_create(btn_1_2);
        lv_img_set_src(img_reboot, &rotate);
        lv_obj_align(img_reboot, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_reboot, 24, 24);
        lv_obj_add_style(img_reboot, &img_recolor_white_style, LV_PART_MAIN);

        lv_obj_t *btn_main_page = lv_btn_create(current_screen);
        lv_obj_align(btn_main_page, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_size(btn_main_page, 48, 35);
        lv_obj_set_style_bg_opa(btn_main_page, 0, LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_cicle = LCD_BTN_EVENT::MAIN_PAGE;
        lv_obj_add_event_cb(btn_main_page, event_handler_main, LV_EVENT_CLICKED, &cmd_cicle);

        lv_obj_t *img_circle = lv_img_create(btn_main_page);
        lv_img_set_src(img_circle, &circle);
        lv_obj_align(img_circle, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_circle, 24, 24);
        lv_obj_add_style(img_circle, &img_recolor_white_style, LV_PART_MAIN);

        btn_next_page = lv_btn_create(current_screen);

        lv_obj_align(btn_next_page, LV_ALIGN_BOTTOM_MID, 58, -10);
        lv_obj_set_size(btn_next_page, 48, 35);
        lv_obj_set_style_bg_opa(btn_next_page, 0, LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_right = LCD_BTN_EVENT::NEXT_PAGE;
        lv_obj_add_event_cb(btn_next_page, event_handler_main, LV_EVENT_CLICKED, &cmd_right);

        lv_obj_t *img_arrow_right = lv_img_create(btn_next_page);
        lv_img_set_src(img_arrow_right, &arrow);
        lv_obj_align(img_arrow_right, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_arrow_right, 24, 24);
        lv_obj_add_style(img_arrow_right, &img_recolor_white_style, LV_PART_MAIN);

        btn_previous_page = lv_btn_create(current_screen);
        lv_obj_align(btn_previous_page, LV_ALIGN_BOTTOM_MID, -58, -10);
        lv_obj_set_size(btn_previous_page, 48, 35);
        lv_obj_set_style_bg_opa(btn_previous_page, 0, LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_left = LCD_BTN_EVENT::PREVIOUS_PAGE;
        lv_obj_add_event_cb(btn_previous_page, event_handler_main, LV_EVENT_CLICKED, &cmd_left);

        lv_obj_t *img_arrow_left = lv_img_create(btn_previous_page);
        lv_img_set_src(img_arrow_left, &arrow);
        lv_obj_align(img_arrow_left, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_arrow_left, 24, 24);
        lv_img_set_angle(img_arrow_left, 1800);
        lv_obj_add_style(img_arrow_left, &img_recolor_white_style, LV_PART_MAIN);

        xSemaphoreGive(xGuiSemaphore);
    }
}

void WatchTft::load_button_menu()
{
    printf("load_button_menu()\n");

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
    {
        if (button_menu != NULL)
        {
            lv_obj_set_parent(button_menu, current_screen);
            xSemaphoreGive(xGuiSemaphore);
            return;
        }

        /******************
         *  BUTTON MENU
         *****************/
        button_menu = lv_obj_create(current_screen);
        lv_obj_set_size(button_menu, 240, 240);
        lv_obj_align(button_menu, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_add_flag(button_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(button_menu, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(button_menu, 60, LV_PART_MAIN);
        lv_obj_set_style_radius(button_menu, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(button_menu, 0, LV_PART_MAIN);
        lv_obj_clear_flag(button_menu, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *buttons_container = lv_obj_create(button_menu);
        lv_obj_set_size(buttons_container, 120, 60);
        lv_obj_align(buttons_container, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_bg_color(buttons_container, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_radius(buttons_container, 20, LV_PART_MAIN);
        lv_obj_set_style_border_width(buttons_container, 0, LV_PART_MAIN);
        lv_obj_clear_flag(buttons_container, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *btn_tft_off = lv_btn_create(buttons_container);
        lv_obj_align(btn_tft_off, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_size(btn_tft_off, 40, 40);
        lv_obj_set_style_bg_color(btn_tft_off, lv_color_white(), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_tft_off = LCD_BTN_EVENT::TURN_OFF_SCREEN;
        lv_obj_add_event_cb(btn_tft_off, event_handler_main, LV_EVENT_CLICKED, &cmd_tft_off);

        lv_obj_t *img_ampoule = lv_img_create(btn_tft_off);
        lv_img_set_src(img_ampoule, &ampoule);
        lv_obj_align(img_ampoule, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_ampoule, 24, 24);

        lv_obj_t *btn_light_sleep = lv_btn_create(buttons_container);
        lv_obj_align(btn_light_sleep, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_size(btn_light_sleep, 40, 40);
        lv_obj_set_style_bg_color(btn_light_sleep, lv_color_white(), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_light_sleep = LCD_BTN_EVENT::SLEEP_SCREEN;
        lv_obj_add_event_cb(btn_light_sleep, event_handler_main, LV_EVENT_CLICKED, &cmd_light_sleep);

        lv_obj_t *img_lune = lv_img_create(btn_light_sleep);
        lv_img_set_src(img_lune, &lune);
        lv_obj_align(img_lune, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_lune, 24, 24);

        xSemaphoreGive(xGuiSemaphore);
    }
}

void WatchTft::load_top_bar(const char *title)
{

    static lv_obj_t *label_title = NULL;
    static lv_obj_t *top_bar = NULL;

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
    {

        if (label_title != NULL)
        {
            lv_obj_set_parent(top_bar, current_screen);
            lv_obj_set_parent(top_menu, current_screen);
            lv_label_set_text(label_title, title);

            xSemaphoreGive(xGuiSemaphore);
            return;
        }

        /******************
         *    TOP BAR
         *****************/
        top_bar = lv_obj_create(current_screen);
        lv_obj_set_size(top_bar, 240, 30);
        lv_obj_set_style_bg_color(top_bar, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_top_bar = LCD_BTN_EVENT::TOP_BAR_TOGGLE;
        lv_obj_add_event_cb(top_bar, event_handler_main, LV_EVENT_PRESSED, &cmd_top_bar);

        label_title = lv_label_create(top_bar);
        lv_obj_align(label_title, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_text_align(label_title, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_title, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(label_title, title);

        label_battery = lv_label_create(top_bar);
        lv_obj_set_width(label_battery, 50);
        lv_obj_align(label_battery, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_text_align(label_battery, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(label_battery, lv_color_white(), LV_PART_MAIN);
        std::string battery_percent = std::to_string(s_battery_percent) + "%";
        lv_label_set_text(label_battery, battery_percent.c_str());

        static lv_style_t style_line;
        lv_style_init(&style_line);
        lv_style_set_line_width(&style_line, 2);

        img_wifi = lv_label_create(top_bar);
        lv_obj_set_size(img_wifi, 24, 24);
        lv_label_set_text(img_wifi, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_align(img_wifi, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(img_wifi, LV_ALIGN_RIGHT_MID, -40, 6);

        wifi_line_points[0] = {165 - WatchPower::is_charging * 5, 8};
        wifi_line_points[1] = {184 - WatchPower::is_charging * 5, 22};
        line_wifi = lv_line_create(top_bar);
        lv_obj_set_size(line_wifi, 240, 30);
        lv_obj_align(line_wifi, LV_ALIGN_RIGHT_MID, 10, 0);
        lv_line_set_points(line_wifi, wifi_line_points, 2);
        lv_obj_add_style(line_wifi, &style_line, LV_PART_MAIN);
        lv_obj_center(line_wifi);

        if (WatchWiFi::connected)
        {
            lv_obj_set_style_text_color(img_wifi, lv_color_white(), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_text_color(img_wifi, lv_color_black(), LV_PART_MAIN);
        }

        if (WatchWiFi::enable)
        {
            lv_obj_add_flag(line_wifi, LV_OBJ_FLAG_HIDDEN);
        }

        img_gps = lv_label_create(top_bar);
        lv_obj_set_size(img_gps, 24, 24);
        lv_label_set_text(img_gps, LV_SYMBOL_GPS);
        lv_obj_set_style_text_align(img_gps, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(img_gps, LV_ALIGN_RIGHT_MID, -65, 4);

        gps_line_points[0] = {142 - WatchPower::is_charging * 5, 8};
        gps_line_points[1] = {161 - WatchPower::is_charging * 5, 22};
        line_gps = lv_line_create(top_bar);
        lv_obj_set_size(line_gps, 240, 30);
        lv_obj_align(line_gps, LV_ALIGN_RIGHT_MID, 10, 0);
        lv_line_set_points(line_gps, gps_line_points, 2);
        lv_obj_add_style(line_gps, &style_line, LV_PART_MAIN);
        lv_obj_center(line_gps);

        if (WatchGps::gps_fixed)
        {
            lv_obj_set_style_text_color(img_gps, lv_color_white(), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_text_color(img_gps, lv_color_black(), LV_PART_MAIN);
        }

        if (WatchGps::gps_enabled)
        {
            lv_obj_add_flag(line_gps, LV_OBJ_FLAG_HIDDEN);
        }

        top_menu = lv_obj_create(current_screen);
        lv_obj_set_size(top_menu, 240, 170);
        lv_obj_align(top_menu, LV_ALIGN_TOP_MID, 0, 30);
        lv_obj_set_style_bg_color(top_menu, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(top_menu, 200, LV_PART_MAIN);
        lv_obj_set_style_radius(top_menu, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(top_menu, 0, LV_PART_MAIN);
        lv_obj_add_flag(top_menu, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *label_wifi = lv_label_create(top_menu);
        lv_obj_align(label_wifi, LV_ALIGN_TOP_LEFT, 0, 10);
        lv_obj_set_style_text_align(label_wifi, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_wifi, lv_color_black(), LV_PART_MAIN);
        lv_label_set_text(label_wifi, "WiFi");

        lv_obj_t *sw_wifi;
        sw_wifi = lv_switch_create(top_menu);
        lv_obj_align(sw_wifi, LV_ALIGN_TOP_LEFT, 40, 3);
        lv_obj_set_style_bg_color(sw_wifi, lv_palette_main(LV_PALETTE_DEEP_PURPLE), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (WatchWiFi::enable)
        {
            lv_obj_add_state(sw_wifi, LV_STATE_CHECKED);
        }
        static uint8_t wifi_data = 1;
        lv_obj_add_event_cb(sw_wifi, top_menu_event_handler, LV_EVENT_VALUE_CHANGED, &wifi_data);

        lv_obj_t *label_gps = lv_label_create(top_menu);
        lv_obj_align(label_gps, LV_ALIGN_TOP_LEFT, 0, 45);
        lv_obj_set_style_text_align(label_gps, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_gps, lv_color_black(), LV_PART_MAIN);
        lv_label_set_text(label_gps, "GPS");

        lv_obj_t *sw_gps;
        sw_gps = lv_switch_create(top_menu);
        lv_obj_align(sw_gps, LV_ALIGN_TOP_LEFT, 40, 38);
        lv_obj_set_style_bg_color(sw_gps, lv_palette_main(LV_PALETTE_DEEP_PURPLE), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (WatchGps::gps_enabled)
        {
            lv_obj_add_state(sw_gps, LV_STATE_CHECKED);
        }
        static uint8_t gps_data = 2;
        lv_obj_add_event_cb(sw_gps, top_menu_event_handler, LV_EVENT_VALUE_CHANGED, &gps_data);

        lv_obj_t *label_brightness = lv_label_create(top_menu);
        lv_obj_align(label_brightness, LV_ALIGN_BOTTOM_LEFT, 18, -35);
        lv_obj_set_style_text_align(label_brightness, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_brightness, lv_color_black(), LV_PART_MAIN);
        lv_label_set_text(label_brightness, "Brightness");

        slider_top_bl = lv_slider_create(top_menu);
        lv_obj_align(slider_top_bl, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_size(slider_top_bl, 180, 20);
        lv_slider_set_range(slider_top_bl, 10, 100);
        lv_slider_set_value(slider_top_bl, bl_value, LV_ANIM_ON);
        lv_obj_set_style_bg_color(slider_top_bl, lv_color_hex(0xed1590), LV_PART_KNOB);
        lv_obj_set_style_bg_color(slider_top_bl, lv_palette_main(LV_PALETTE_DEEP_PURPLE), LV_PART_INDICATOR);
        lv_obj_add_event_cb(slider_top_bl, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        xSemaphoreGive(xGuiSemaphore);
    }
}

void WatchTft::set_charge_state(bool state)
{
    if (img_wifi == NULL)
    {
        return;
    }

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {
        lv_obj_align(img_wifi, LV_ALIGN_RIGHT_MID, -40 - WatchPower::is_charging * 5, 6);
        lv_obj_align(img_gps, LV_ALIGN_RIGHT_MID, -65 - WatchPower::is_charging * 5, 4);
        lv_label_set_text(label_battery, WatchPower::is_charging ? "100%" LV_SYMBOL_CHARGE : "100%");

        wifi_line_points[0] = {165 - WatchPower::is_charging * 5, 8};
        wifi_line_points[1] = {184 - WatchPower::is_charging * 5, 22};
        lv_line_set_points(line_wifi, wifi_line_points, 2);

        gps_line_points[0] = {142 - WatchPower::is_charging * 5, 8};
        gps_line_points[1] = {161 - WatchPower::is_charging * 5, 22};
        lv_line_set_points(line_gps, gps_line_points, 2);

        xSemaphoreGive(xGuiSemaphore);
    }
}

void WatchTft::top_menu_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint8_t user_data = *(uint8_t *)e->user_data;
    lv_obj_t *obj = lv_event_get_target(e);
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        if (user_data == 1)
        {
            if (WatchWiFi::enable != state)
            {
                printf("State WiFi: %d\n", state);
                WatchWiFi::enable = state;
                set_wifi_state(WatchWiFi::enable, WatchWiFi::connected);
                if (WatchWiFi::enable)
                {
                    WatchWiFi::connect();
                }
                else
                {
                    WatchWiFi::disconnect();
                }
            }
        }
        else if (user_data == 2)
        {
            if (WatchGps::gps_enabled != state)
            {
                printf("State GPS: %d\n", state);
                WatchGps::gps_enabled = state;
                set_gps_state(WatchGps::gps_enabled, WatchGps::gps_fixed);

                WatchGps::set_state_gps(WatchGps::gps_enabled);
            }
        }
    }
}

void WatchTft::set_wifi_state(bool enable, bool connected)
{
    if (connected)
    {
        lv_obj_set_style_text_color(img_wifi, lv_color_white(), LV_PART_MAIN);
    }
    else
    {
        lv_obj_set_style_text_color(img_wifi, lv_color_black(), LV_PART_MAIN);
    }

    if (enable)
    {
        lv_obj_add_flag(line_wifi, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(line_wifi, LV_OBJ_FLAG_HIDDEN);
    }
}

void WatchTft::set_gps_state(bool enable, bool fixed)
{
    if (!enable)
    {
        fixed = false;
    }

    if (fixed)
    {
        lv_obj_set_style_text_color(img_gps, lv_color_white(), LV_PART_MAIN);
    }
    else
    {
        lv_obj_set_style_text_color(img_gps, lv_color_black(), LV_PART_MAIN);
    }

    if (enable)
    {
        lv_obj_add_flag(line_gps, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(line_gps, LV_OBJ_FLAG_HIDDEN);
    }
}

void WatchTft::turn_off_screen()
{
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {
        if (lcd_turn_off_screen != NULL)
        {
            lv_obj_clean(lcd_turn_off_screen);
            lcd_turn_off_screen = NULL;
        }
        lcd_turn_off_screen = lv_obj_create(NULL);

        lv_obj_t *lcd_bg = lv_obj_create(lcd_turn_off_screen);
        lv_obj_set_size(lcd_bg, 240, 240);
        lv_obj_set_style_bg_color(lcd_bg, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_radius(lcd_bg, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(lcd_bg, 0, LV_PART_MAIN);

        lv_obj_t *img_ampoule = lv_img_create(lcd_bg);
        lv_img_set_src(img_ampoule, &ampoule);
        lv_obj_align(img_ampoule, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_style(img_ampoule, &img_recolor_white_style, LV_PART_MAIN);
        lv_obj_set_size(img_ampoule, 24, 24);

        lv_scr_load(lcd_turn_off_screen);

        xSemaphoreGive(xGuiSemaphore);
    }

    vTaskDelay(500);

    timer_last_touch = esp_timer_get_time() / 1000;
    turn_screen_off();
}

void WatchTft::sleep_screen()
{
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {
        if (lcd_sleep_screen != NULL)
        {
            lv_obj_clean(lcd_sleep_screen);
            lcd_sleep_screen = NULL;
        }
        lcd_sleep_screen = lv_obj_create(NULL);

        lv_obj_t *lcd_bg = lv_obj_create(lcd_sleep_screen);
        lv_obj_set_size(lcd_bg, 240, 240);
        lv_obj_set_style_bg_color(lcd_bg, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_radius(lcd_bg, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(lcd_bg, 0, LV_PART_MAIN);

        lv_obj_t *img_ampoule = lv_img_create(lcd_bg);
        lv_img_set_src(img_ampoule, &lune);
        lv_obj_align(img_ampoule, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_style(img_ampoule, &img_recolor_white_style, LV_PART_MAIN);
        lv_obj_set_size(img_ampoule, 24, 24);

        lv_scr_load(lcd_sleep_screen);
        xSemaphoreGive(xGuiSemaphore);
    }

    vTaskDelay(500);

    WatchPower::enter_light_sleep();
}

void WatchTft::main_screen_from_sleep()
{
    printf("current_screen_from_sleep\n");
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 10000))
    {
        if (lcd_sleep_screen != NULL)
        {
            lv_obj_clean(lcd_sleep_screen);
            lcd_sleep_screen = NULL;
        }
        lv_scr_load(current_screen);
        xSemaphoreGive(xGuiSemaphore);
    }
    else
    {
        printf("Failed to take gui Semaphore X01\n");
    }
    toggle_button_menu_view();
    turn_screen_on();
}

void WatchTft::load_screen(screen_index_t screen_index)
{
    lv_obj_t *old_screen = current_screen;
    current_screen = lv_obj_create(NULL);
    lv_scr_load(current_screen);
    lv_obj_clear_flag(current_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Delete old task
    if (current_task_hanlde != NULL)
    {
        vTaskDelete(current_task_hanlde);
        current_task_hanlde = NULL;
    }

    power_chart_ser_bat = NULL;
    power_chart_ser_usb = NULL;

    switch (screen_index)
    {
    case screen_index_t::main:
        load_main_screen();
        load_top_bar("Home");
        break;
    case screen_index_t::power:
        power_screen();
        load_top_bar("Power");
        break;
    case screen_index_t::settings:
        settings_screen();
        load_top_bar("Settings");
        break;
    case screen_index_t::gps:
        gps_screen();
        load_top_bar("GPS");
        break;
    case screen_index_t::run:
        run_screen();
        load_top_bar("Run");
        break;

    default:
        break;
    }

    load_button_menu();

    // Delete old screen
    if (old_screen != NULL)
    {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
        {
            lv_obj_del(old_screen);
            xSemaphoreGive(xGuiSemaphore);
        }
        else
        {
            printf("can't delete old screen, sem taken\n");
        }
    }

    current_screen_index = screen_index;
}

void WatchTft::reset_screen()
{
    if (lcd_reset_screen != NULL)
    {
        lv_obj_clean(lcd_reset_screen);
        lcd_reset_screen = NULL;
    }
    lcd_reset_screen = lv_obj_create(NULL);

    lv_obj_t *lcd_bg = lv_obj_create(lcd_reset_screen);
    lv_obj_set_size(lcd_bg, 240, 240);
    lv_obj_set_style_bg_color(lcd_bg, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_radius(lcd_bg, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(lcd_bg, 0, LV_PART_MAIN);

    lv_obj_t *img_rotate = lv_img_create(lcd_bg);
    lv_img_set_src(img_rotate, &rotate);
    lv_obj_align(img_rotate, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(img_rotate, &img_recolor_white_style, LV_PART_MAIN);
    lv_obj_set_size(img_rotate, 24, 24);

    lv_scr_load(lcd_reset_screen);

    vTaskDelay(500);

    esp_restart();
}

void WatchTft::event_handler_main(lv_event_t *e)
{
    LCD_BTN_EVENT *cmd = (LCD_BTN_EVENT *)e->user_data;
    printf("event_handler_main (LCD_BTN_EVENT) %u\n", *((uint32_t *)cmd));

    switch (*cmd)
    {
    case LCD_BTN_EVENT::TOP_BAR_TOGGLE:
        toggle_top_bar_view();
        break;
    case LCD_BTN_EVENT::TURN_OFF_SCREEN:
    {
        LCD_CMD cmd = LCD_CMD::TURN_OFF_SCREEN;
        xQueueSend(xQueueLcdCmd, &cmd, 0);
        break;
    }
    case LCD_BTN_EVENT::SLEEP_SCREEN:
    {
        LCD_CMD cmd = LCD_CMD::SLEEP_SCREEN;
        xQueueSend(xQueueLcdCmd, &cmd, 0);
        break;
    }
    case LCD_BTN_EVENT::RESET_ESP:
    {
        LCD_CMD cmd = LCD_CMD::RESET_ESP;
        xQueueSend(xQueueLcdCmd, &cmd, 0);
        break;
    }
    case LCD_BTN_EVENT::RETURN_HOME:
    {
        LCD_CMD cmd = LCD_CMD::RETURN_HOME;
        xQueueSend(xQueueLcdCmd, &cmd, 0);
        break;
    }
    case LCD_BTN_EVENT::POWER_SCREEN:
    {
        LCD_CMD cmd = LCD_CMD::POWER_SCREEN;
        xQueueSend(xQueueLcdCmd, &cmd, 0);
        break;
    }
    case LCD_BTN_EVENT::RUN_SCREEN:
    {
        LCD_CMD cmd = LCD_CMD::RUN_SCREEN;
        xQueueSend(xQueueLcdCmd, &cmd, 0);
        break;
    }
    case LCD_BTN_EVENT::SETTINGS_SCREEN:
    {
        LCD_CMD cmd = LCD_CMD::SETTINGS_SCREEN;
        xQueueSend(xQueueLcdCmd, &cmd, 0);
        break;
    }
    case LCD_BTN_EVENT::GPS_SCREEN:
    {
        LCD_CMD cmd = LCD_CMD::GPS_SCREEN;
        xQueueSend(xQueueLcdCmd, &cmd, 0);
        break;
    }
    case LCD_BTN_EVENT::GPS_TRACKING:
    {
        if (current_task_hanlde == NULL)
        {
            printf("Creating gps tracking task\n");
            TaskHandle_t tracking_gps_task_hanlde = NULL;
            lv_obj_set_style_bg_color(btn_start_tracking, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
            xTaskCreatePinnedToCore(WatchGps::tracking_task, "tracking_gps", 4096 * 2, NULL, 0, &tracking_gps_task_hanlde, 1);
            current_task_hanlde = tracking_gps_task_hanlde;
        }
        else
        {
            printf("Stopping gps tracking task...\n");
            lv_obj_set_style_bg_color(btn_start_tracking, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
            WatchGps::gps_tracking_stop = true;
        }

        break;
    }
    case LCD_BTN_EVENT::CENTER_GPS:
    {
        WatchTft::center_gps();
        break;
    }
    case LCD_BTN_EVENT::CLEAN_FILE:
    {
        // Open the directory
        DIR *dir = opendir(MOUNT_POINT "/");
        if (dir == NULL)
        {
            printf("Error opendir\n");
            break;
        }

        // Read the directory entries
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            // Check if the file name starts with the specified word
            if (strncmp(entry->d_name, "map_15_", strlen("map_15_")) == 0)
            {
                // Construct the full path of the file
                char path[300];
                snprintf(path, sizeof(path), MOUNT_POINT "/%s", entry->d_name);

                printf("Remove: %s\n", path);
                // Remove the file
                if (remove(path) != 0)
                {
                    perror("remove");
                    break;
                }
            }
        }

        // Close the directory
        closedir(dir);

        break;
    }

    default:
        break;
    }
}

void WatchTft::toggle_top_bar_view()
{
    printf("top bar pressed\n");

    static bool top_menu_visible = false;
    if (top_menu_visible)
    {
        lv_obj_add_flag(top_menu, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(top_menu, LV_OBJ_FLAG_HIDDEN);
    }
    top_menu_visible = !top_menu_visible;
}

void WatchTft::toggle_button_menu_view()
{
    static bool button_menu_visible = false;
    printf("turning %s button_menu_visible\n", button_menu_visible ? "off" : "on");

    if (button_menu_visible || !screen_en)
    {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
        {
            if (lcd_turn_off_screen != NULL)
            {
                printf("cleaning lcd_turn_off_screen\n");
                lv_obj_clean(lcd_turn_off_screen);
                lcd_turn_off_screen = NULL;
            }
            else
            {
                printf("no lcd_turn_off_screen\n");
            }
            printf("loading current_screen\n");
            lv_scr_load(current_screen);

            if (button_menu != NULL)
            {
                lv_obj_add_flag(button_menu, LV_OBJ_FLAG_HIDDEN);
            }
            xSemaphoreGive(xGuiSemaphore);
        }
        else
        {
            printf("Failed to take GUI Semaphore AX02\n");
        }
        button_menu_visible = false;
    }
    else
    {
        vTaskDelay(20);
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
        {
            lv_obj_set_parent(button_menu, current_screen);
            lv_obj_clear_flag(button_menu, LV_OBJ_FLAG_HIDDEN);
            xSemaphoreGive(xGuiSemaphore);
        }
        else
        {
            printf("Failed to take GUI Semaphore AX03\n");
        }
        button_menu_visible = true;
    }
}

void WatchTft::slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    bl_value = lv_slider_get_value(slider);
    disp_backlight_set(bckl_handle, bl_value);
}

void WatchTft::set_bl(uint8_t value)
{
    bl_value = value;
    disp_backlight_set(bckl_handle, bl_value);
}

void WatchTft::lv_tick_task(void *arg)
{
    (void)arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}

void WatchTft::turn_screen_on()
{
    if (screen_en == 1)
        return;

    screen_en = 1;
    timer_last_touch = esp_timer_get_time() / 1000;

    axpxx_setPowerOutPut(AXP202_LDO3, 1);
    axpxx_setPowerOutPut(AXP202_LDO2, 1);
    vTaskDelay(100);
    touch_driver = lv_indev_drv_register(&indev_drv);
    set_bl(bl_value);
}

void WatchTft::turn_screen_off()
{
    if (screen_en == 0)
        return;

    screen_en = 0;

    lv_indev_delete(touch_driver);
    vTaskDelay(100);
    axpxx_setPowerOutPut(AXP202_LDO3, 0);
    axpxx_setPowerOutPut(AXP202_LDO2, 0);
}

void WatchTft::set_battery_text(uint8_t percent)
{
    if (label_battery == NULL)
    {
        return;
    }
    std::string battery_percent = std::to_string(percent) + "%";
    if (WatchPower::is_charging)
    {
        battery_percent += LV_SYMBOL_CHARGE;
    }
    s_battery_percent = percent;

    if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
    {
        lv_label_set_text(label_battery, battery_percent.c_str());
        xSemaphoreGive(xGuiSemaphore);
    }
}