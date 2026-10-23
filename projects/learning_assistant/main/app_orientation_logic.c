// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "app_orientation_logic.h"

#include <stddef.h>

/* 8 samples at the 50 ms task period: 400 ms before changing direction. */
#define ORIENTATION_STABLE_SAMPLES 8U
#define ORIENTATION_MIN_AXIS_G 0.62f
#define ORIENTATION_AXIS_MARGIN_G 0.18f
#define ORIENTATION_MAX_Z_G 0.78f
#define ORIENTATION_MIN_MAG_SQ 0.42f
#define ORIENTATION_MAX_MAG_SQ 1.82f

static float absf_local(float value)
{
    return value < 0.0f ? -value : value;
}

static app_orientation_t classify(float x_g, float y_g, float z_g)
{
    const float magnitude_sq = x_g * x_g + y_g * y_g + z_g * z_g;
    const float abs_x = absf_local(x_g);
    const float abs_y = absf_local(y_g);

    /* Hold the current direction while flat, moving quickly, or near a corner. */
    if (magnitude_sq < ORIENTATION_MIN_MAG_SQ ||
            magnitude_sq > ORIENTATION_MAX_MAG_SQ ||
            absf_local(z_g) > ORIENTATION_MAX_Z_G) {
        return APP_ORIENTATION_UNKNOWN;
    }

    if (abs_x >= ORIENTATION_MIN_AXIS_G &&
            abs_x - abs_y >= ORIENTATION_AXIS_MARGIN_G) {
        /* The production board mounts BMI270 with the horizontal X direction
         * opposite to the display's logical X axis. */
        return x_g < 0.0f ? APP_ORIENTATION_270 : APP_ORIENTATION_90;
    }
    if (abs_y >= ORIENTATION_MIN_AXIS_G &&
            abs_y - abs_x >= ORIENTATION_AXIS_MARGIN_G) {
        return y_g >= 0.0f ? APP_ORIENTATION_0 : APP_ORIENTATION_180;
    }
    return APP_ORIENTATION_UNKNOWN;
}

void app_orientation_filter_init(app_orientation_filter_t *filter,
                                 app_orientation_t current)
{
    if (filter == NULL) {
        return;
    }
    *filter = (app_orientation_filter_t) {
        .current = current,
        .candidate = APP_ORIENTATION_UNKNOWN,
    };
}

bool app_orientation_filter_observe(app_orientation_filter_t *filter,
                                    float x_g, float y_g, float z_g,
                                    app_orientation_t *out_orientation)
{
    if (filter == NULL || out_orientation == NULL) {
        return false;
    }
    const app_orientation_t observed = classify(x_g, y_g, z_g);
    if (observed == APP_ORIENTATION_UNKNOWN || observed == filter->current) {
        filter->candidate = APP_ORIENTATION_UNKNOWN;
        filter->stable_samples = 0;
        return false;
    }
    if (observed != filter->candidate) {
        filter->candidate = observed;
        filter->stable_samples = 1;
        return false;
    }
    if (filter->stable_samples < UINT8_MAX) {
        filter->stable_samples++;
    }
    if (filter->stable_samples < ORIENTATION_STABLE_SAMPLES) {
        return false;
    }

    filter->current = observed;
    filter->candidate = APP_ORIENTATION_UNKNOWN;
    filter->stable_samples = 0;
    *out_orientation = observed;
    return true;
}
