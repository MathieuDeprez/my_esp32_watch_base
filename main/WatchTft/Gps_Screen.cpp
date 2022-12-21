#include "WatchTft.h"

lv_obj_t *WatchTft::lcd_gps_screen = NULL;
lv_obj_t *WatchTft::label_title_gps = NULL;
lv_obj_t *WatchTft::label_gps_info = NULL;
lv_obj_t *WatchTft::gps_recording_indicator = NULL;

lv_point_t drag_view(int32_t x, int32_t y);

lv_style_t arc_gps_style;
lv_obj_t *arc_gps_status = NULL;
lv_obj_t *label_gps_status = NULL;

lv_obj_t *btn_start_tracking = NULL;
lv_obj_t *btn_center_gps = NULL;
lv_obj_t *btn_pos_gps = NULL;
lv_point_t btn_pos_gps_point = {};

lv_obj_t *map_bg_p[4] = {};
lv_point_t map_bg_p_points[4] = {};

uint32_t glob_x = 0;
uint32_t glob_y = 0;

int32_t tile_x = 0;
int32_t tile_y = 0;

uint32_t gps_x = 0;
uint32_t gps_y = 0;

int32_t gps_screen_x = 0;
int32_t gps_screen_y = 0;

int32_t square_x[2] = {tile_x, tile_x + 1};
int32_t square_y[2] = {tile_y, tile_y + 1};

uint8_t user_map_square[4] = {0, 1, 2, 3}; // M R B BR

bool centered_to_gps = false;

lv_obj_t *main_bg_img_M = NULL;
lv_obj_t *main_bg_img_R = NULL;
lv_obj_t *main_bg_img_BR = NULL;
lv_obj_t *main_bg_img_B = NULL;

void WatchTft::refresh_tile(int32_t x, int32_t y)
{

    lv_obj_t *main_bg_img = NULL;
    if (x == square_x[0] && y == square_y[0])
    {
        main_bg_img = main_bg_img_M;
    }
    else if (x == square_x[1] && y == square_y[0])
    {
        main_bg_img = main_bg_img_R;
    }
    else if (x == square_x[0] && y == square_y[1])
    {
        main_bg_img = main_bg_img_B;
    }
    else if (x == square_x[1] && y == square_y[1])
    {
        main_bg_img = main_bg_img_BR;
    }

    if (main_bg_img == NULL)
    {
        return;
    }

    char filename[60] = {};
    sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", x, y);
    printf("M: %s\n", filename);
    if (access(filename, F_OK) == 0)
    {
        char filename_lv[60] = {};
        sprintf(filename_lv, "S:/map_15_%u_%u.png", x, y);
        lv_img_set_src(main_bg_img, filename_lv);
    }
    else
    {
        printf("file don't exist...\n");
        lv_img_set_src(main_bg_img, &download);
    }
}

void WatchTft::add_xy_to_gps_point(int32_t x, int32_t y, bool sem_taken)
{

    gps_screen_x -= x;
    gps_screen_y -= y;

    // printf("add_xy_to_gps_point: %d %d\n", x, y);

    btn_pos_gps_point.x -= x;
    btn_pos_gps_point.y -= y;

    // printf("gps_screen_x: %d, gps_screen_y: %d\n", gps_screen_x, gps_screen_y);

    if (sem_taken)
    {

        lv_obj_set_pos(btn_pos_gps, gps_screen_x, gps_screen_y);
    }
    else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
    {
        lv_obj_set_pos(btn_pos_gps, gps_screen_x, gps_screen_y);
        xSemaphoreGive(xGuiSemaphore);
    }
    /*static lv_point_t line_points[] = {{5, 5}, {70, 70}};
    line_points[0].x = gps_screen_x += x;
    line_points[0].y = gps_screen_y += y;

    line_points[1].x = gps_screen_x;
    line_points[1].y = gps_screen_y;

    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 8);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_line_rounded(&style_line, true);

    static lv_obj_t *line1;
    line1 = lv_line_create(lv_scr_act());
    lv_line_set_points(line1, line_points, 2); //Set the points
    lv_obj_add_style(line1, &style_line, 0);
    lv_obj_center(line1);*/
}

void WatchTft::drag_event_handler(lv_event_t *e)
{

    if (centered_to_gps)
    {
        printf("quit centered to gps\n");
        centered_to_gps = false;
        lv_obj_set_style_bg_color(btn_center_gps, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL)
        return;

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    if (vect.x == 0 && vect.y == 0)
    {
        return;
    }

    lv_point_t lv_point = drag_view(vect.x, vect.y, true);

    add_xy_to_gps_point(-lv_point.x, -lv_point.y, true);
}

lv_point_t WatchTft::drag_view(int32_t x, int32_t y, bool sem_taken)
{
    lv_coord_t x_M = map_bg_p_points[0].x;
    lv_coord_t y_M = map_bg_p_points[0].y;

    lv_coord_t x_R = map_bg_p_points[1].x;
    lv_coord_t y_R = map_bg_p_points[1].y;

    lv_coord_t x_B = map_bg_p_points[2].x;
    lv_coord_t y_B = map_bg_p_points[2].y;

    lv_coord_t x_BR = map_bg_p_points[3].x;
    lv_coord_t y_BR = map_bg_p_points[3].y;

    glob_x -= x;
    glob_y -= y;
    // printf("glob_xy: %u:%u\n", glob_x, glob_y);

    // printf("drag_view: %d %d\n", x, y);

    int32_t new_tile_x = glob_x / 256;
    int32_t new_tile_y = glob_y / 256;
    if (new_tile_x != tile_x || new_tile_y != tile_y)
    {
        tile_x = new_tile_x;
        tile_y = new_tile_y;
        printf("change tile: %d:%d\n", tile_x, tile_y);
    }

    int32_t new_square_x = (glob_x + 128) / 256 - 1;
    int32_t new_square_y = (glob_y + 128) / 256 - 1;

    int32_t offset_x_map = 0;
    int32_t offset_y_map = 0;
    if (new_square_x != square_x[0] || new_square_y != square_y[0])
    {

        offset_x_map = -(square_x[0] - new_square_x) * 256;
        offset_y_map = -(square_y[0] - new_square_y) * 256;

        square_x[0] = new_square_x;
        square_x[1] = new_square_x + 1;
        square_y[0] = new_square_y;
        square_y[1] = new_square_y + 1;
        printf("change square: %d:%d->%d:%d\n",
               square_x[0], square_x[1], square_y[0], square_y[1]);

        printf("off_x_y: %d %d\n", offset_x_map, offset_y_map);

        char filename[60] = {};
        sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", new_square_x, new_square_y);
        printf("M: %s\n", filename);
        if (access(filename, F_OK) == 0)
        {
            char filename_lv[60] = {};
            sprintf(filename_lv, "S:/map_15_%u_%u.png", new_square_x, new_square_y);
            if (sem_taken)
            {
                lv_img_set_src(main_bg_img_M, filename_lv);
            }
            else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_img_set_src(main_bg_img_M, filename_lv);
                xSemaphoreGive(xGuiSemaphore);
            }
        }
        else
        {
            if (sem_taken)
            {
                lv_img_set_src(main_bg_img_M, &download);
            }
            else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_img_set_src(main_bg_img_M, &download);
                xSemaphoreGive(xGuiSemaphore);
            }
        }

        sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", new_square_x + 1, new_square_y);
        printf("R: %s\n", filename);
        if (access(filename, F_OK) == 0)
        {
            char filename_lv[60] = {};
            sprintf(filename_lv, "S:/map_15_%u_%u.png", new_square_x + 1, new_square_y);

            if (sem_taken)
            {
                lv_img_set_src(main_bg_img_R, filename_lv);
            }
            else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_img_set_src(main_bg_img_R, filename_lv);
                xSemaphoreGive(xGuiSemaphore);
            }
        }
        else
        {
            if (sem_taken)
            {
                lv_img_set_src(main_bg_img_R, &download);
            }
            else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_img_set_src(main_bg_img_R, &download);
                xSemaphoreGive(xGuiSemaphore);
            }
        }

        sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", new_square_x, new_square_y + 1);
        printf("B: %s\n", filename);
        if (access(filename, F_OK) == 0)
        {
            char filename_lv[60] = {};
            sprintf(filename_lv, "S:/map_15_%u_%u.png", new_square_x, new_square_y + 1);

            if (sem_taken)
            {
                lv_img_set_src(main_bg_img_B, filename_lv);
            }
            else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_img_set_src(main_bg_img_B, filename_lv);
                xSemaphoreGive(xGuiSemaphore);
            }
        }
        else
        {
            if (sem_taken)
            {
                lv_img_set_src(main_bg_img_B, &download);
            }
            else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_img_set_src(main_bg_img_B, &download);
                xSemaphoreGive(xGuiSemaphore);
            }
        }

        sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", new_square_x + 1, new_square_y + 1);
        printf("BR: %s\n", filename);
        if (access(filename, F_OK) == 0)
        {
            char filename_lv[60] = {};
            sprintf(filename_lv, "S:/map_15_%u_%u.png", new_square_x + 1, new_square_y + 1);
            if (sem_taken)
            {
                lv_img_set_src(main_bg_img_BR, filename_lv);
            }
            else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_img_set_src(main_bg_img_BR, filename_lv);
                xSemaphoreGive(xGuiSemaphore);
            }
        }
        else
        {
            if (sem_taken)
            {
                lv_img_set_src(main_bg_img_BR, &download);
            }
            else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_img_set_src(main_bg_img_BR, &download);
                xSemaphoreGive(xGuiSemaphore);
            }
        }
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        map_bg_p_points[i].x += x + offset_x_map;
        map_bg_p_points[i].y += y + offset_y_map;
    }

    if (sem_taken)
    {
        lv_obj_set_pos(map_bg_p[0], map_bg_p_points[0].x, map_bg_p_points[0].y);
        lv_obj_set_pos(map_bg_p[1], map_bg_p_points[1].x, map_bg_p_points[1].y);
        lv_obj_set_pos(map_bg_p[2], map_bg_p_points[2].x, map_bg_p_points[2].y);
        lv_obj_set_pos(map_bg_p[3], map_bg_p_points[3].x, map_bg_p_points[3].y);
    }
    else if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
    {
        lv_obj_set_pos(map_bg_p[0], map_bg_p_points[0].x, map_bg_p_points[0].y);
        lv_obj_set_pos(map_bg_p[1], map_bg_p_points[1].x, map_bg_p_points[1].y);
        lv_obj_set_pos(map_bg_p[2], map_bg_p_points[2].x, map_bg_p_points[2].y);
        lv_obj_set_pos(map_bg_p[3], map_bg_p_points[3].x, map_bg_p_points[3].y);

        xSemaphoreGive(xGuiSemaphore);
    }

    lv_point_t lv_point = {};
    lv_point.x = x;
    lv_point.y = y;

    return lv_point;
}

static void pressed_event_handler(lv_event_t *e)
{

    static struct timeval timer_start;
    if (e->code == 1)
    {
        gettimeofday(&timer_start, NULL);
        return;
    }

    struct timeval timer_end;
    gettimeofday(&timer_end, NULL);
    uint32_t ms_btw = (timer_end.tv_sec - timer_start.tv_sec) * 1000 + timer_end.tv_usec / 1000 - timer_start.tv_usec / 1000;

    printf("took %u us\n", ms_btw);

    if (ms_btw > 250)
    {
        return;
    }

    uint8_t index = *((uint8_t *)e->user_data);
    int32_t tile_index_x = square_x[index % 2];
    int32_t tile_index_y = square_y[index > 1];

    char filename[60] = {};
    sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", tile_index_x, tile_index_y);

    bool file_exist = false;
    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        file_exist = false;
    }
    else
    {
        file_exist = true;
        fclose(f);
    }

    printf("CLICKED %d %d:%d, exist: %d, event: %d\n", index, tile_index_x, tile_index_y, file_exist, e->code);

    if (!file_exist)
    {
        WatchWiFi::download_tile(tile_index_x, tile_index_y);
    }
}

void WatchTft::center_gps()
{

    centered_to_gps = !centered_to_gps;
    if (centered_to_gps)
    {

        int32_t diff_x = glob_x - gps_x;
        int32_t diff_y = glob_y - gps_y;
        printf("diff: %d %d %u:%u\n", diff_x, diff_y, glob_x, gps_x);
        drag_view(diff_x, diff_y, true);
        add_xy_to_gps_point(-diff_x, -diff_y, true);

        lv_obj_set_style_bg_color(btn_center_gps, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    }
    else
    {
        lv_obj_set_style_bg_color(btn_center_gps, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    }
}

void WatchTft::gps_screen()
{

    tile_x = WatchGps::lon_to_tile_x(WatchGps::gps_lon);
    tile_y = WatchGps::lat_lon_to_tile_y(WatchGps::gps_lat, WatchGps::gps_lon);
    printf("tile_x: %u, tile_y: %u\n", tile_x, tile_y);

    glob_x = tile_x * 256 + 128;
    glob_y = tile_y * 256 + 128;
    printf("glob_x: %u, glob_y: %u\n", glob_x, glob_y);

    square_x[0] = tile_x;
    square_x[1] = tile_x + 1;
    square_y[0] = tile_y;
    square_y[1] = tile_y + 1;

    gps_x = WatchGps::lon_to_x(WatchGps::gps_lon);
    gps_y = WatchGps::lat_lon_to_y(WatchGps::gps_lat, WatchGps::gps_lon);
    printf("gps_x: %u, gps_y: %u\n", gps_x, gps_y);

    lv_obj_t *gps_bg = NULL;

    size_t free_block = heap_caps_get_largest_free_block(MALLOC_CAP_32BIT);
    printf("free_block: %u\n", free_block);

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

        main_bg_img_M = lv_img_create(gps_bg);
        char filename[50] = {};
        sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", square_x[0], square_y[0]);
        if (access(filename, F_OK) == 0)
        {
            char filename_lv[50] = {};
            sprintf(filename_lv, "S:/map_15_%u_%u.png", square_x[0], square_y[0]);
            lv_img_set_src(main_bg_img_M, filename_lv);
        }
        else
        {
            lv_img_set_src(main_bg_img_M, &download);
        }

        lv_obj_align(main_bg_img_M, LV_ALIGN_TOP_LEFT, -8, -23);
        lv_obj_set_size(main_bg_img_M, 256, 256);
        lv_obj_add_flag(main_bg_img_M, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(main_bg_img_M, drag_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(main_bg_img_M, pressed_event_handler, LV_EVENT_PRESSED, &user_map_square[0]);
        lv_obj_add_event_cb(main_bg_img_M, pressed_event_handler, LV_EVENT_RELEASED, &user_map_square[0]);
        map_bg_p[0] = main_bg_img_M;

        main_bg_img_R = lv_img_create(gps_bg);
        sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", square_x[1], square_y[0]);
        if (access(filename, F_OK) == 0)
        {
            char filename_lv[50] = {};
            sprintf(filename_lv, "S:/map_15_%u_%u.png", square_x[1], square_y[0]);
            lv_img_set_src(main_bg_img_R, filename_lv);
        }
        else
        {
            lv_img_set_src(main_bg_img_R, &download);
        }
        lv_obj_align(main_bg_img_R, LV_ALIGN_TOP_LEFT, 248, -23);
        lv_obj_set_size(main_bg_img_R, 256, 256);
        lv_obj_add_flag(main_bg_img_R, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(main_bg_img_R, drag_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(main_bg_img_R, pressed_event_handler, LV_EVENT_PRESSED, &user_map_square[1]);
        lv_obj_add_event_cb(main_bg_img_R, pressed_event_handler, LV_EVENT_RELEASED, &user_map_square[1]);
        map_bg_p[1] = main_bg_img_R;

        main_bg_img_B = lv_img_create(gps_bg);
        sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", square_x[0], square_y[1]);
        if (access(filename, F_OK) == 0)
        {
            char filename_lv[50] = {};
            sprintf(filename_lv, "S:/map_15_%u_%u.png", square_x[0], square_y[1]);
            lv_img_set_src(main_bg_img_B, filename_lv);
        }
        else
        {
            lv_img_set_src(main_bg_img_B, &download);
        }
        lv_obj_align(main_bg_img_B, LV_ALIGN_TOP_LEFT, -8, 233);
        lv_obj_set_size(main_bg_img_B, 256, 256);
        lv_obj_add_flag(main_bg_img_B, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(main_bg_img_B, drag_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(main_bg_img_B, pressed_event_handler, LV_EVENT_PRESSED, &user_map_square[2]);
        lv_obj_add_event_cb(main_bg_img_B, pressed_event_handler, LV_EVENT_RELEASED, &user_map_square[2]);
        map_bg_p[2] = main_bg_img_B;

        main_bg_img_BR = lv_img_create(gps_bg);
        sprintf(filename, MOUNT_POINT "/map_15_%u_%u.png", square_x[1], square_y[1]);
        if (access(filename, F_OK) == 0)
        {
            char filename_lv[50] = {};
            sprintf(filename_lv, "S:/map_15_%u_%u.png", square_x[1], square_y[1]);
            lv_img_set_src(main_bg_img_BR, filename_lv);
        }
        else
        {
            lv_img_set_src(main_bg_img_BR, &download);
        }
        lv_obj_align(main_bg_img_BR, LV_ALIGN_TOP_LEFT, 248, 233);
        lv_obj_set_size(main_bg_img_BR, 256, 256);
        lv_obj_add_flag(main_bg_img_BR, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(main_bg_img_BR, drag_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(main_bg_img_BR, pressed_event_handler, LV_EVENT_PRESSED, &user_map_square[3]);
        lv_obj_add_event_cb(main_bg_img_BR, pressed_event_handler, LV_EVENT_RELEASED, &user_map_square[3]);
        map_bg_p[3] = main_bg_img_BR;

        lv_obj_t *label_container_gps_status = lv_obj_create(gps_bg);
        lv_obj_set_size(label_container_gps_status, 50, 50);
        lv_obj_set_style_radius(label_container_gps_status, 50, LV_PART_MAIN);
        lv_obj_align(label_container_gps_status, LV_ALIGN_TOP_LEFT, -10, 10);
        lv_obj_clear_flag(label_container_gps_status, LV_OBJ_FLAG_SCROLLABLE);

        arc_gps_status = lv_arc_create(label_container_gps_status);
        lv_arc_set_bg_angles(arc_gps_status, 0, 0);
        lv_arc_set_angles(arc_gps_status, 0, 260);
        lv_obj_set_size(arc_gps_status, 40, 40);
        lv_arc_set_rotation(arc_gps_status, -135);

        lv_style_init(&arc_gps_style);
        lv_style_set_arc_color(&arc_gps_style, lv_palette_main(LV_PALETTE_RED));
        lv_style_set_arc_width(&arc_gps_style, 5);
        lv_obj_add_style(arc_gps_status, &arc_gps_style, LV_PART_INDICATOR);
        lv_obj_remove_style(arc_gps_status, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc_gps_status, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_center(arc_gps_status);

        label_gps_status = lv_label_create(label_container_gps_status);
        lv_obj_align(label_gps_status, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_align(label_gps_status, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(label_gps_status, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_text_font(label_gps_status, &lv_font_montserrat_16, 0);
        lv_label_set_text(label_gps_status, "0");

        btn_start_tracking = lv_btn_create(gps_bg);
        lv_obj_align(btn_start_tracking, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_obj_set_style_radius(btn_start_tracking, 50, LV_PART_MAIN);
        lv_obj_set_size(btn_start_tracking, 50, 50);
        lv_obj_set_style_bg_color(btn_start_tracking, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_gps_tracking = LCD_BTN_EVENT::GPS_TRACKING;
        lv_obj_add_event_cb(btn_start_tracking, event_handler_main, LV_EVENT_CLICKED, &cmd_gps_tracking);

        lv_obj_t *img_run = lv_img_create(btn_start_tracking);
        lv_img_set_src(img_run, &run);
        lv_obj_align(img_run, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(img_run, 24, 24);
        lv_obj_add_style(img_run, &img_recolor_white_style, LV_PART_MAIN);

        btn_center_gps = lv_btn_create(gps_bg);
        lv_obj_align(btn_center_gps, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
        lv_obj_set_size(btn_center_gps, 48, 35);
        lv_obj_set_style_bg_color(btn_center_gps, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_center_gps = LCD_BTN_EVENT::CENTER_GPS;
        lv_obj_add_event_cb(btn_center_gps, event_handler_main, LV_EVENT_CLICKED, &cmd_center_gps);

        lv_obj_t *img_gps = lv_label_create(btn_center_gps);
        lv_obj_set_size(img_gps, 24, 24);
        lv_label_set_text(img_gps, LV_SYMBOL_GPS);
        lv_obj_set_style_text_align(img_gps, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(img_gps, LV_ALIGN_CENTER, 0, 3);
        lv_img_set_angle(img_gps, 1800);
        lv_obj_add_style(img_gps, &img_recolor_white_style, LV_PART_MAIN);

        lv_obj_t *btn_clean_file = lv_btn_create(gps_bg);
        lv_obj_align(btn_clean_file, LV_ALIGN_BOTTOM_RIGHT, -5, -45);
        lv_obj_set_size(btn_clean_file, 48, 35);
        lv_obj_set_style_bg_color(btn_clean_file, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_clean_file = LCD_BTN_EVENT::CLEAN_FILE;
        lv_obj_add_event_cb(btn_clean_file, event_handler_main, LV_EVENT_CLICKED, &cmd_clean_file);

        lv_obj_t *img_clean_file = lv_label_create(btn_clean_file);
        lv_obj_set_size(img_clean_file, 24, 24);
        lv_label_set_text(img_clean_file, LV_SYMBOL_DRIVE);
        lv_obj_set_style_text_align(img_clean_file, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(img_clean_file, LV_ALIGN_CENTER, 0, 3);
        lv_img_set_angle(img_clean_file, 1800);
        lv_obj_add_style(img_clean_file, &img_recolor_white_style, LV_PART_MAIN);

        lv_obj_t *btn_home = lv_btn_create(gps_bg);
        lv_obj_align(btn_home, LV_ALIGN_BOTTOM_LEFT, 5, -5);
        lv_obj_set_size(btn_home, 48, 35);
        lv_obj_set_style_bg_color(btn_home, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        static LCD_BTN_EVENT cmd_home = LCD_BTN_EVENT::RETURN_HOME;
        lv_obj_add_event_cb(btn_home, event_handler_main, LV_EVENT_CLICKED, &cmd_home);

        gps_screen_x = gps_x - glob_x + 120;
        gps_screen_y = gps_y - glob_y + 105;
        printf("gps_screen_x: %u, gps_screen_y: %u\n", gps_screen_x, gps_screen_y);
        btn_pos_gps = lv_btn_create(gps_bg);
        lv_obj_align(btn_pos_gps, LV_ALIGN_TOP_LEFT, gps_screen_x, gps_screen_y);
        lv_obj_set_style_radius(btn_pos_gps, 50, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(btn_pos_gps, 0, LV_PART_MAIN);
        lv_obj_set_size(btn_pos_gps, 10, 10);
        lv_obj_set_style_bg_color(btn_pos_gps, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);

        lv_obj_t *center_point = lv_btn_create(gps_bg);
        lv_obj_align(center_point, LV_ALIGN_TOP_LEFT, 120, 105);
        lv_obj_set_style_radius(center_point, 50, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(center_point, 0, LV_PART_MAIN);
        lv_obj_set_size(center_point, 10, 10);
        lv_obj_set_style_bg_color(center_point, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);

        lv_obj_t *img_home = lv_label_create(btn_home);
        lv_obj_set_size(img_home, 24, 24);
        lv_label_set_text(img_home, LV_SYMBOL_HOME);
        lv_obj_set_style_text_align(img_home, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(img_home, LV_ALIGN_CENTER, 0, 3);
        lv_img_set_angle(img_home, 1800);
        lv_obj_add_style(img_home, &img_recolor_white_style, LV_PART_MAIN);

        display_top_bar(lcd_gps_screen, "GPS");

        btn_pos_gps_point.x = gps_screen_x;
        btn_pos_gps_point.y = gps_screen_y;

        printf("**btn_pos_gps_point.x: %d, btn_pos_gps_point.y: %d\n", btn_pos_gps_point.x, btn_pos_gps_point.y);

        map_bg_p_points[0].x = lv_obj_get_x(map_bg_p[0]);
        map_bg_p_points[0].y = lv_obj_get_y(map_bg_p[0]);

        map_bg_p_points[1].x = lv_obj_get_x(map_bg_p[1]);
        map_bg_p_points[1].y = lv_obj_get_y(map_bg_p[1]);

        map_bg_p_points[2].x = lv_obj_get_x(map_bg_p[2]);
        map_bg_p_points[2].y = lv_obj_get_y(map_bg_p[2]);

        map_bg_p_points[3].x = lv_obj_get_x(map_bg_p[3]);
        map_bg_p_points[3].y = lv_obj_get_y(map_bg_p[3]);

        printf("**map_bg_p_points[0].x: %d, map_bg_p_points[0].y: %d\n", map_bg_p_points[0].x, map_bg_p_points[0].y);

        xSemaphoreGive(xGuiSemaphore);

        WatchGps::init();
    }
    else
    {
        printf("Sem not takn !!!\n");
    }
}

void WatchTft::set_gps_tracking_hidden_state(bool status)
{
    if (pdTRUE != xSemaphoreTake(xGuiSemaphore, 50))
    {
        return;
    }
    if (status)
    {
        lv_obj_add_flag(gps_recording_indicator, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(gps_recording_indicator, LV_OBJ_FLAG_HIDDEN);
    }
    xSemaphoreGive(xGuiSemaphore);
}

void WatchTft::set_gps_info(bool fixed, uint8_t in_use, float dop, double lat, double lon)
{

    static bool last_fixed = false;
    static uint8_t last_in_use = 0;
    static float last_dop = 0;
    static double last_lat = WatchGps::gps_lat;
    static double last_lon = WatchGps::gps_lon;

    if (fixed && in_use == 0)
    {
        fixed = false;
    }

    bool fixed_changed = false;
    if (last_fixed != fixed)
    {
        last_fixed = fixed;
        fixed_changed = true;
    }

    bool in_use_changed = false;
    if (last_in_use != in_use)
    {
        last_in_use = in_use;
        in_use_changed = true;
    }

    bool dop_changed = false;
    if (last_dop != dop)
    {
        last_dop = dop;
        dop_changed = true;
    }

    bool latlon_changed = false;
    if ((lat != last_lat || lon != last_lon) &&
        (lat != 0 || lon != 0))
    {
        last_lat = lat;
        last_lon = lon;
        latlon_changed = true;
    }

    if (!fixed_changed && !in_use_changed && !dop_changed)
    {
        return;
    }

    if (pdTRUE != xSemaphoreTake(xGuiSemaphore, 0))
    {
        return;
    }

    if (dop_changed)
    {
        if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
        {
            if (dop < 1)
            {
                lv_arc_set_angles(arc_gps_status, 0, 260);
            }
            else if (dop < 2)
            {
                lv_arc_set_angles(arc_gps_status, 20, 240);
            }
            else if (dop < 5)
            {
                lv_arc_set_angles(arc_gps_status, 60, 200);
            }
            else
            {
                lv_arc_set_angles(arc_gps_status, 100, 160);
            }
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    if (fixed_changed)
    {
        if (fixed)
        {
            if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_style_set_arc_color(&arc_gps_style, lv_palette_main(LV_PALETTE_GREEN));
                lv_obj_set_style_text_color(label_gps_status, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
                xSemaphoreGive(xGuiSemaphore);
            }
        }
        else
        {
            if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
            {
                lv_style_set_arc_color(&arc_gps_style, lv_palette_main(LV_PALETTE_RED));
                lv_obj_set_style_text_color(label_gps_status, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
                xSemaphoreGive(xGuiSemaphore);
            }
        }

        if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
        {
            lv_obj_add_style(arc_gps_status, &arc_gps_style, LV_PART_INDICATOR);
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    if (in_use_changed)
    {
        char sat_in_use[5] = {};
        sprintf(sat_in_use, "%d", in_use);
        if (xSemaphoreTake(xGuiSemaphore, 0) == pdTRUE)
        {
            lv_label_set_text(label_gps_status, sat_in_use);
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    xSemaphoreGive(xGuiSemaphore);

    if (latlon_changed)
    {
        WatchGps::gps_lat = lat;
        WatchGps::gps_lon = lon;

        uint32_t new_gps_x = WatchGps::lon_to_x(WatchGps::gps_lon);
        uint32_t new_gps_y = WatchGps::lat_lon_to_y(WatchGps::gps_lat, WatchGps::gps_lon);

        int32_t diff_x = gps_x - new_gps_x;
        int32_t diff_y = gps_y - new_gps_y;

        printf("gps move: %u->%u, %u->%u\n", gps_x, new_gps_x, gps_y, new_gps_y);

        add_gps_pos(diff_x, diff_y);
    }
}

void WatchTft::add_gps_pos(int32_t x, int32_t y)
{
    gps_x -= x;
    gps_y -= y;

    printf("gps_xy: %u:%u\n", gps_x, gps_y);

    lv_point_t lv_point = {};
    lv_point.x = 0;
    lv_point.y = 0;

    if (centered_to_gps)
    {
        lv_point = drag_view(x, y);
    }
    else
    {
        add_xy_to_gps_point(x, y);
    }
}
