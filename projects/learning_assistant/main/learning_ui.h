// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include "esp_gsp.h"
typedef struct learning_ui_t *learning_ui_handle_t;
esp_gsp_err_t learning_ui_create(esp_gsp_handle_t ui, learning_ui_handle_t *out);
void learning_ui_delete(learning_ui_handle_t ui);

// UI-thread observer for native simulator evidence; no cross-task access.
bool learning_ui_quick_is_open(learning_ui_handle_t ui);
