// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "app_orientation_logic.h"

static void expect_no_change(app_orientation_filter_t *filter,
                             float x, float y, float z, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        app_orientation_t output = APP_ORIENTATION_UNKNOWN;
        assert(!app_orientation_filter_observe(filter, x, y, z, &output));
    }
}

static void expect_change(app_orientation_filter_t *filter,
                          float x, float y, float z,
                          app_orientation_t expected)
{
    expect_no_change(filter, x, y, z, 7);
    app_orientation_t output = APP_ORIENTATION_UNKNOWN;
    assert(app_orientation_filter_observe(filter, x, y, z, &output));
    assert(output == expected);
}

int main(void)
{
    app_orientation_filter_t filter;
    app_orientation_filter_init(&filter, APP_ORIENTATION_0);

    /* Flat, fast-moving, diagonal, and short-lived samples must not rotate. */
    expect_no_change(&filter, 0.02f, 0.03f, 1.00f, 12);
    expect_no_change(&filter, -1.50f, 0.00f, 0.00f, 12);
    expect_no_change(&filter, -0.70f, 0.68f, 0.05f, 12);
    expect_no_change(&filter, -0.90f, 0.05f, 0.10f, 7);
    expect_no_change(&filter, 0.05f, 0.90f, 0.10f, 1);

    expect_change(&filter, -0.90f, 0.05f, 0.10f, APP_ORIENTATION_270);
    expect_no_change(&filter, -0.90f, 0.05f, 0.10f, 12);
    expect_change(&filter, 0.05f, -0.90f, 0.10f, APP_ORIENTATION_180);
    expect_change(&filter, 0.90f, 0.05f, 0.10f, APP_ORIENTATION_90);
    expect_change(&filter, 0.05f, 0.90f, 0.10f, APP_ORIENTATION_0);

    puts("orientation logic: OK");
    return 0;
}
