// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_ORIENTATION_UNKNOWN = -1,
    APP_ORIENTATION_0 = 0,
    APP_ORIENTATION_90 = 90,
    APP_ORIENTATION_180 = 180,
    APP_ORIENTATION_270 = 270,
} app_orientation_t;

typedef struct {
    app_orientation_t current;
    app_orientation_t candidate;
    uint8_t stable_samples;
} app_orientation_filter_t;

void app_orientation_filter_init(app_orientation_filter_t *filter,
                                 app_orientation_t current);

/*
 * Observe one filtered acceleration sample. Returns true exactly once when a
 * new orientation has remained stable long enough to apply it.
 */
bool app_orientation_filter_observe(app_orientation_filter_t *filter,
                                    float x_g, float y_g, float z_g,
                                    app_orientation_t *out_orientation);
