// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { LEARN_GRADE_MIN=1, LEARN_GRADE_MAX=12, LEARN_GRADE_DEFAULT=7 };
typedef struct { unsigned brightness,volume,fixed_rotation,grade;bool speech_enabled,automatic; } learning_controls_t;
typedef enum { LEARN_CONTROL_BRIGHTNESS,LEARN_CONTROL_VOLUME,LEARN_CONTROL_SPEECH,LEARN_CONTROL_AUTO,LEARN_CONTROL_GRADE } learning_control_t;
const char *learning_grade_name(unsigned grade);
learning_controls_t learning_controls_defaults(void);
bool learning_controls_change(learning_controls_t *v,learning_control_t control,int value,unsigned rotation);
uint32_t learning_controls_pack(const learning_controls_t *v);
bool learning_controls_unpack(uint32_t packed,learning_controls_t *v);
