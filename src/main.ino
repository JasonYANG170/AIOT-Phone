/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/integration/framework/arduino.html  */

/*
  Libs:
    TFT_eSPI 2.5.43 (newest)
    lvgl 9.1.0 (newest)
*/

#include <lvgl.h>
#include "FT6336U.h"

#if LV_USE_TFT_ESPI
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
#endif

// Define constants for I2C pins and screen resolution
#define I2C_SDA 7
#define I2C_SCL 8
#define TFT_HOR_RES   240
#define TFT_VER_RES   320
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0

// Create objects for the touch controller and display buffer
FT6336U ft6336u(I2C_SDA, I2C_SCL);
FT6336U_TouchPointType tp;

// Buffer for LVGL rendering, 1/10 screen size is recommended
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// Log function for debugging
#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char * buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

// Tick function for LVGL timer
static uint32_t my_tick(void) {
    return millis();
}

// Display flush function for copying rendered image to display
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint16_t * px_map);

// Touchpad read function for handling touch input
void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data);

// Event handler for the counter button
static void counter_event_handler(lv_event_t * e);

// Event handler for the toggle button
static void toggle_event_handler(lv_event_t * e);

// Button demo function
void lv_button_demo(void);

// Task for LVGL rendering on core 1
void lvgl_task(void *pvParameters) {
    while(1) {
        lv_timer_handler();
        delay(5);
    }
}

////////////////////////////////////////////////////
void setup() {
    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.begin(115200);
    Serial.println(LVGL_Arduino);

    // Initialize TFT display
    tft.begin();
    tft.setRotation(1);

    // Initialize touch controller
    ft6336u.begin();
    Serial.print("FT6336U Firmware Version: ");
    Serial.println(ft6336u.read_firmware_id());
    Serial.print("FT6336U Device Mode: ");
    Serial.println(ft6336u.read_device_mode());

    // Initialize LVGL
    lv_init();
    lv_tick_set_cb(my_tick);

    // Register print function for debugging
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    // Create display object
    lv_display_t * disp;
#if LV_USE_TFT_ESPI
    disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, TFT_ROTATION);
#else
    disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    // Initialize input device driver
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Create LVGL task on core 1
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 4096, NULL, 2, NULL, 1);

    Serial.println("Setup done");
    lv_button_demo();
}

////////////////////////////////////////////////////
void loop() {
    // Handle touch input and other tasks on core 0
    tp = ft6336u.scan();
    if (!tp.touch_count) {
        // Touch released
    } else {
        // Touch pressed
    }
    // ... other tasks on core 0 ...
}

////////////////////////////////////////////////////
// Display flush function for copying rendered image to display
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint16_t * px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(px_map, w * h, true);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

// Touchpad read function for handling touch input
void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    data->state = tp.touch_count ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (tp.touch_count) {
        data->point.x = TFT_HOR_RES - tp.tp[0].y;
        data->point.y = tp.tp[0].x;
        Serial.printf("Touch (x,y): (%03d,%03d)\n",data->point.x,data->point.y );
    }
}

// Event handler for the counter button
static void counter_event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        static uint8_t cnt = 0;
        cnt++;
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Button: %d", cnt);
        LV_LOG_USER("Clicked");
        Serial.println("Clicked");
    }
}

// Event handler for the toggle button
static void toggle_event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (lv_obj_has_state(btn, LV_STATE_CHECKED)) {
            LV_LOG_USER("Toggled ON");
            Serial.println("Toggled ON");
        } else {
            LV_LOG_USER("Toggled OFF");
            Serial.println("Toggled OFF");
        }
    }
}

// Button demo function
void lv_button_demo(void) {
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello Arduino, I'm LVGL 9.1.0!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);

    // Create counter button
    lv_obj_t *btn1 = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn1, counter_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_set_pos(btn1, 100, 100);
    lv_obj_set_size(btn1, 120, 50);
    label = lv_label_create(btn1);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);

    // Create toggle button
    lv_obj_t *btn2 = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn2, toggle_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_pos(btn2, 250, 100);
    lv_obj_set_size(btn2, 120, 50);
    label = lv_label_create(btn2);
    lv_label_set_text(label, "Toggle Button");
    lv_obj_center(label);
}