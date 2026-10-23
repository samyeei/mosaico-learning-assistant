// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include "esp_err.h"
#include "esp_gsp.h"
typedef struct learning_camera_t *learning_camera_handle_t;
esp_err_t learning_camera_create(esp_gsp_handle_t ui, learning_camera_handle_t *out);
esp_err_t learning_camera_delete(learning_camera_handle_t camera);
esp_err_t learning_camera_start(learning_camera_handle_t camera);
esp_err_t learning_camera_stop(learning_camera_handle_t camera);
// Worker-owned: image compensation only, not whole-screen orientation.
esp_err_t learning_camera_options(learning_camera_handle_t camera,bool light,bool flipped);
esp_err_t learning_camera_preview(learning_camera_handle_t camera);
esp_err_t learning_camera_capture(learning_camera_handle_t camera, const void **jpeg, size_t *size);
