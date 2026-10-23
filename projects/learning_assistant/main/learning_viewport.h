// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { LEARN_PREVIEW_W=416, LEARN_PREVIEW_H=224,
       LEARN_PHOTO_W=832, LEARN_PHOTO_H=448 };
typedef struct {
    unsigned sensor_w,sensor_h,ccw_turns,x,y,w,h;
} learning_viewport_t;
bool learning_viewport_init(learning_viewport_t *v,unsigned sensor_w,
                           unsigned sensor_h,unsigned display_rotation);
bool learning_viewport_init_facing(learning_viewport_t *v,unsigned sensor_w,
                                  unsigned sensor_h,unsigned display_rotation,bool flipped);
// Maps any output raster through the SAME oriented center crop into the sensor.
void learning_viewport_pixel(const learning_viewport_t *v,unsigned x,unsigned y,
                            unsigned width,unsigned height,unsigned *sx,unsigned *sy);
