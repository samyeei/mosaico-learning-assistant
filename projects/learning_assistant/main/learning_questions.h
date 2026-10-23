// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include <stdint.h>
enum { LEARN_QUESTION_MAX=24, LEARN_QUESTION_ROWS=3,
       LEARN_QUESTION_NUMBER_CAP=49, LEARN_QUESTION_SUMMARY_CAP=97,
       LEARN_QUESTION_LOCATION_CAP=145 };
typedef struct {
    char number[LEARN_QUESTION_NUMBER_CAP];
    char summary[LEARN_QUESTION_SUMMARY_CAP];
    char location[LEARN_QUESTION_LOCATION_CAP];
} learning_question_t;
typedef struct {
    unsigned count;
    learning_question_t items[LEARN_QUESTION_MAX];
} learning_questions_t;
