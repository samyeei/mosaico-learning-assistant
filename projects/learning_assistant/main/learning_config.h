// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { LEARN_KEY_CAP = 257, LEARN_SSID_CAP = 33, LEARN_PASSWORD_CAP = 65 };
typedef struct { uint32_t version; char ssid[LEARN_SSID_CAP];
    char password[LEARN_PASSWORD_CAP]; char key[LEARN_KEY_CAP];
    unsigned region; } learning_config_t;
bool learning_config_valid(const learning_config_t *config);
const char *learning_config_host(unsigned region);
const char *learning_config_model(void);
