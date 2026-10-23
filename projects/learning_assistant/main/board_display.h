#pragma once

#include "esp_err.h"
#include "esp_gsp_esp_lcd.h"

esp_err_t board_display_init(esp_display_present_target_config_t *out_target);
esp_err_t board_touch_init(esp_lcd_touch_handle_t *out_touch);
