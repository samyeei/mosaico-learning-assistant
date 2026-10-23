// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include "esp_err.h"
#include "learning_config.h"
#include <stddef.h>
esp_err_t learning_network_start(void);
bool learning_network_online(void);
bool learning_network_configured(void);
const learning_config_t *learning_network_config(void);
esp_err_t learning_network_setup(void);
bool learning_network_setup_info(const char **ssid, const char **password);
// Worker-only provider smoke check used before atomically committing a candidate.
esp_err_t learning_provider_test(const learning_config_t *config, char *error, size_t capacity);
