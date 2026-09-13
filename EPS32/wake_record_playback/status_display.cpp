#include <Arduino.h>
#include <lvgl.h>
#include "status_display.h"
#include "Display_ST77916.h"

namespace {
lv_disp_draw_buf_t draw;
lv_color_t pixels[360 * 12];
lv_obj_t *heading = nullptr, *subtitle = nullptr;
uint32_t lastTick;
void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *data) {
  LCD_addWindow(area->x1, area->y1, area->x2, area->y2, &data->full);
  lv_disp_flush_ready(driver);
}
}

// Call only from setup/loop, never from the WakeNet FreeRTOS tasks.
void statusDisplayPoll() {
  if (!heading) return;
  uint32_t now = millis();
  lv_tick_inc(now - lastTick);
  lastTick = now;
  lv_timer_handler();
}

void statusDisplayShow(const char *text, const char *detail) {
  if (!heading) return;
  lv_label_set_text(heading, text);
  lv_label_set_text(subtitle, detail);
  statusDisplayPoll();
  // Draw before blocking recording / HTTP calls, not after they finish.
  lv_refr_now(nullptr);
}

void statusDisplayInit() {
  I2C_Init();
  TCA9554PWR_Init(0x00);
  Backlight_Init();
  LCD_Init();
  lv_init();
  lastTick = millis();
  lv_disp_draw_buf_init(&draw, pixels, nullptr, 360 * 12);
  static lv_disp_drv_t driver;
  lv_disp_drv_init(&driver);
  driver.hor_res = 360;
  driver.ver_res = 360;
  driver.draw_buf = &draw;
  driver.flush_cb = flush;
  lv_disp_drv_register(&driver);
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x101728), 0);
  heading = lv_label_create(lv_scr_act());
  subtitle = lv_label_create(lv_scr_act());
  lv_obj_t *labels[] = {heading, subtitle};
  for (auto label : labels) {
    lv_obj_set_width(label, 280);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
  }
  lv_obj_set_style_text_color(heading, lv_color_hex(0x6EF2A6), 0);
  lv_obj_align(heading, LV_ALIGN_CENTER, 0, -20);
  lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 20);
  statusDisplayShow("Starting...", "AI Speaker");
}
