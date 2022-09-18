#include "WatchTft.h"

WatchTft watchTft;
QueueHandle_t WatchTft::xQueueLcdCmd;
/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

#define DELAY_BETWEEN_LV(X)                                     \
    xSemaphoreGive(xGuiSemaphore);                              \
    }                                                           \
    vTaskDelay(X);                                              \
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) \
    {

lv_style_t img_recolor_white_style;

disp_backlight_h WatchTft::bckl_handle;
uint8_t WatchTft::bl_value = 75;
bool WatchTft::screen_en = true;

// top menu
lv_obj_t *WatchTft::label_battery;
lv_obj_t *WatchTft::top_menu;
lv_obj_t *WatchTft::main_label_battery;
lv_obj_t *WatchTft::main_top_menu;
lv_obj_t *WatchTft::slider_top_bl;
lv_obj_t *WatchTft::main_slider_top_bl;
uint8_t WatchTft::s_battery_percent = 0;

// chart power
lv_obj_t *WatchTft::chart_power = NULL;
lv_obj_t *WatchTft::label_chart_usb;
lv_obj_t *WatchTft::label_chart_bat;
lv_chart_series_t *WatchTft::power_chart_ser_bat = NULL;
lv_chart_series_t *WatchTft::power_chart_ser_usb = NULL;

// physical button menu
lv_obj_t *WatchTft::button_menu = NULL;
lv_obj_t *WatchTft::lcd_turn_off_screen = NULL;
lv_obj_t *WatchTft::lcd_reset_screen = NULL;
lv_obj_t *WatchTft::lcd_power_screen = NULL;
lv_obj_t *WatchTft::lcd_sleep_screen = NULL;

lv_obj_t *WatchTft::main_screen;
lv_obj_t *WatchTft::current_screen;

lv_indev_t *touch_driver = NULL;
lv_indev_drv_t indev_drv;

static EventGroupHandle_t lvgl_event_group;
const int LVGL_INIT_DONE = BIT0;

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
            return_home_screen();
            break;
        case LCD_CMD::POWER_SCREEN:
            printf("POWER_SCREEN\n");
            power_screen();
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

    lv_obj_t *btn_next_page = NULL;
    lv_obj_t *btn_previous_page = NULL;

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
    {
        main_screen = lv_obj_create(NULL);

        lv_scr_load(main_screen);
        lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *main_bg_img = lv_img_create(main_screen);
        lv_img_set_src(main_bg_img, &main_bg);
        lv_obj_align(main_bg_img, LV_ALIGN_TOP_LEFT, 0, 30);
        lv_obj_set_size(main_bg_img, 240, 210);

#ifdef DISPLAY_TOUCH_LIMIT
        lv_obj_t *rec_L = lv_obj_create(main_screen);
        lv_obj_set_size(rec_L, 25, 240);
        lv_obj_align(rec_L, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_color(rec_L, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_radius(rec_L, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(rec_L, 0, LV_PART_MAIN);

        lv_obj_t *rec_R = lv_obj_create(main_screen);
        lv_obj_set_size(rec_R, 35, 240);
        lv_obj_align(rec_R, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(rec_R, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_radius(rec_R, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(rec_R, 0, LV_PART_MAIN);
#endif

        lv_obj_t *btn_0_0 = lv_btn_create(main_screen);
        lv_obj_align(btn_0_0, LV_ALIGN_TOP_LEFT, 15, 45);
        lv_obj_set_size(btn_0_0, 60, 60);
        lv_obj_set_style_bg_opa(btn_0_0, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_0_0, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_power_screen = LCD_BTN_EVENT::POWER_SCREEN;
        lv_obj_add_event_cb(btn_0_0, event_handler_main, LV_EVENT_CLICKED, &cmd_power_screen);

        lv_style_init(&img_recolor_white_style);
        lv_style_set_img_recolor_opa(&img_recolor_white_style, 255);
        lv_style_set_img_recolor(&img_recolor_white_style, lv_color_white());

        lv_obj_t *img_bolt = lv_img_create(btn_0_0);
        lv_img_set_src(img_bolt, &bolt);
        lv_obj_align(img_bolt, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_bolt, 24, 24);
        lv_obj_add_style(img_bolt, &img_recolor_white_style, LV_PART_MAIN);

        lv_obj_t *btn_0_1 = lv_btn_create(main_screen);
        lv_obj_align(btn_0_1, LV_ALIGN_TOP_LEFT, 90, 45);
        lv_obj_set_size(btn_0_1, 60, 60);
        lv_obj_set_style_bg_opa(btn_0_1, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_0_1, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_reset = LCD_BTN_EVENT::RESET_ESP;
        lv_obj_add_event_cb(btn_0_1, event_handler_main, LV_EVENT_CLICKED, &cmd_reset);

        lv_obj_t *btn_0_2 = lv_btn_create(main_screen);
        lv_obj_align(btn_0_2, LV_ALIGN_TOP_LEFT, 165, 45);
        lv_obj_set_size(btn_0_2, 60, 60);
        lv_obj_set_style_bg_opa(btn_0_2, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_0_2, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_add_event_cb(btn_0_2, event_handler_main, LV_EVENT_CLICKED, &cmd_reset);

        lv_obj_t *btn_1_0 = lv_btn_create(main_screen);
        lv_obj_align(btn_1_0, LV_ALIGN_TOP_LEFT, 15, 120);
        lv_obj_set_size(btn_1_0, 60, 60);
        lv_obj_set_style_bg_opa(btn_1_0, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_1_0, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_add_event_cb(btn_1_0, event_handler_main, LV_EVENT_CLICKED, &cmd_reset);

        lv_obj_t *btn_1_1 = lv_btn_create(main_screen);
        lv_obj_align(btn_1_1, LV_ALIGN_TOP_LEFT, 90, 120);
        lv_obj_set_size(btn_1_1, 60, 60);
        lv_obj_set_style_bg_opa(btn_1_1, 150, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_1_1, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_add_event_cb(btn_1_1, event_handler_main, LV_EVENT_CLICKED, &cmd_reset);

        lv_obj_t *btn_1_2 = lv_btn_create(main_screen);
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

        lv_obj_t *btn_main_page = lv_btn_create(main_screen);
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

        DELAY_BETWEEN_LV(10)

        btn_next_page = lv_btn_create(main_screen);

        lv_obj_align(btn_next_page, LV_ALIGN_BOTTOM_MID, 58, -10);
        lv_obj_set_size(btn_next_page, 48, 35);
        lv_obj_set_style_bg_opa(btn_next_page, 0, LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_right = LCD_BTN_EVENT::NEXT_PAGE;
        lv_obj_add_event_cb(btn_next_page, event_handler_main, LV_EVENT_CLICKED, &cmd_right);

        DELAY_BETWEEN_LV(10)

        lv_obj_t *img_arrow_right = lv_img_create(btn_next_page);
        lv_img_set_src(img_arrow_right, &arrow);
        lv_obj_align(img_arrow_right, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_arrow_right, 24, 24);
        lv_obj_add_style(img_arrow_right, &img_recolor_white_style, LV_PART_MAIN);

        DELAY_BETWEEN_LV(10)

        btn_previous_page = lv_btn_create(main_screen);
        lv_obj_align(btn_previous_page, LV_ALIGN_BOTTOM_MID, -58, -10);
        lv_obj_set_size(btn_previous_page, 48, 35);
        lv_obj_set_style_bg_opa(btn_previous_page, 0, LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_left = LCD_BTN_EVENT::PREVIOUS_PAGE;
        lv_obj_add_event_cb(btn_previous_page, event_handler_main, LV_EVENT_CLICKED, &cmd_left);

        DELAY_BETWEEN_LV(10)

        lv_obj_t *img_arrow_left = lv_img_create(btn_previous_page);
        lv_img_set_src(img_arrow_left, &arrow);
        lv_obj_align(img_arrow_left, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_arrow_left, 24, 24);
        lv_img_set_angle(img_arrow_left, 1800);
        lv_obj_add_style(img_arrow_left, &img_recolor_white_style, LV_PART_MAIN);

        display_top_bar(main_screen, "Home");

        DELAY_BETWEEN_LV(10)

        show_button_menu();

        xSemaphoreGive(xGuiSemaphore);
    }
    current_screen = main_screen;
}

void WatchTft::show_button_menu()
{

    printf("show_button_menu()\n");

    /******************
     *  BUTTON MENU
     *****************/
    button_menu = lv_obj_create(main_screen);
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
}

void WatchTft::display_top_bar(lv_obj_t *parent, const char *title)
{
    /******************
     *    TOP BAR
     *****************/
    lv_obj_t *top_bar = lv_obj_create(parent);
    lv_obj_set_size(top_bar, 240, 30);
    lv_obj_set_style_bg_color(top_bar, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN);
    static LCD_BTN_EVENT cmd_top_bar = LCD_BTN_EVENT::TOP_BAR_TOGGLE;
    lv_obj_add_event_cb(top_bar, event_handler_main, LV_EVENT_PRESSED, &cmd_top_bar);

    lv_obj_t *label_title = lv_label_create(top_bar);
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

    top_menu = lv_obj_create(parent);
    lv_obj_set_size(top_menu, 240, 170);
    lv_obj_align(top_menu, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(top_menu, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top_menu, 200, LV_PART_MAIN);
    lv_obj_set_style_radius(top_menu, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(top_menu, 0, LV_PART_MAIN);
    lv_obj_add_flag(top_menu, LV_OBJ_FLAG_HIDDEN);

    slider_top_bl = lv_slider_create(top_menu);
    lv_obj_align(slider_top_bl, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_size(slider_top_bl, 180, 20);
    lv_slider_set_range(slider_top_bl, 10, 100);
    lv_slider_set_value(slider_top_bl, bl_value, LV_ANIM_ON);
    lv_obj_set_style_bg_color(slider_top_bl, lv_color_hex(0xed1590), LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider_top_bl, lv_palette_main(LV_PALETTE_DEEP_PURPLE), LV_PART_INDICATOR);
    lv_obj_add_event_cb(slider_top_bl, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (parent == main_screen)
    {
        main_label_battery = label_battery;
        main_top_menu = top_menu;
        main_slider_top_bl = slider_top_bl;
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

    watchPower.enter_light_sleep();
}

void WatchTft::power_screen()
{
    lv_obj_t *power_bg = NULL;

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {

        if (lcd_power_screen != NULL)
        {
            lv_obj_clean(lcd_power_screen);
            lcd_power_screen = NULL;
        }
        lcd_power_screen = lv_obj_create(NULL);
        lv_scr_load(lcd_power_screen);
        current_screen = lcd_power_screen;
        lv_obj_clear_flag(lcd_power_screen, LV_OBJ_FLAG_SCROLLABLE);

        static lv_style_t power_bg_style;
        lv_style_init(&power_bg_style);
        lv_style_set_bg_color(&power_bg_style, lv_color_black());
        lv_style_set_radius(&power_bg_style, 0);
        lv_style_set_border_width(&power_bg_style, 0);
        lv_style_set_pad_all(&power_bg_style, 0);

        power_bg = lv_obj_create(lcd_power_screen);
        lv_obj_set_size(power_bg, 240, 210);
        lv_obj_align(power_bg, LV_ALIGN_TOP_LEFT, 0, 30);
        lv_obj_clear_flag(power_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(power_bg, &power_bg_style, LV_PART_MAIN);

        DELAY_BETWEEN_LV(10)

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

        DELAY_BETWEEN_LV(10)

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

        DELAY_BETWEEN_LV(10)

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

        display_top_bar(lcd_power_screen, "Power");

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

void WatchTft::main_screen_from_sleep()
{
    printf("main_screen_from_sleep\n");
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

void WatchTft::return_home_screen()
{

    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
    {
        lv_obj_set_parent(button_menu, main_screen);

        lv_scr_load(main_screen);
        lv_obj_clean(current_screen);
        current_screen = main_screen;

        if (lcd_power_screen != NULL)
        {
            lv_obj_clean(lcd_power_screen);
            lcd_power_screen = NULL;
        }
        xSemaphoreGive(xGuiSemaphore);
    }
    else
    {
        printf("Failed to take gui Semaphore X01\n");
    }
    label_battery = main_label_battery;
    top_menu = main_top_menu;
    slider_top_bl = main_slider_top_bl;
    power_chart_ser_bat = NULL;
    power_chart_ser_usb = NULL;
    set_battery_text(s_battery_percent);
    lv_slider_set_value(slider_top_bl, bl_value, LV_ANIM_ON);
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

    if (button_menu_visible)
    {
        if (lcd_turn_off_screen != NULL)
        {
            lv_obj_clean(lcd_turn_off_screen);
            lcd_turn_off_screen = NULL;
        }
        lv_scr_load(current_screen);
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, 1000))
        {
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
    }
    button_menu_visible = !button_menu_visible;
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
    std::string battery_percent = std::to_string(percent) + "%";
    s_battery_percent = percent;
    lv_label_set_text(label_battery, battery_percent.c_str());
}