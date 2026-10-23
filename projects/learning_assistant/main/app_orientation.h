// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include "esp_err.h"
#include "esp_gsp.h"

/** Start BMI270 gravity sensing and four-direction display auto-rotation. */
esp_err_t app_orientation_start(esp_gsp_handle_t ui);
/** App-lifetime orientation service: worker holds this across one camera frame;
 * the orientation task holds it across pause/rotate/resume. Never use in UI callbacks. */
void app_orientation_frame_lock(void);
void app_orientation_frame_unlock(void);
unsigned app_orientation_get(void);
// Nonblocking policy update. Fixed mode holds this angle, including after boot.
void app_orientation_set_mode(bool automatic,unsigned fixed_rotation);
