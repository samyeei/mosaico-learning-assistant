// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include "learning_config.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "learning_questions.h"
typedef struct learning_provider_t *learning_provider_handle_t;
typedef bool (*learning_current_cb_t)(void *context, uint32_t generation);
esp_err_t learning_provider_create(learning_current_cb_t current, void *context,
                                  learning_provider_handle_t *out);
void learning_provider_delete(learning_provider_handle_t provider);
bool learning_provider_set_grade(learning_provider_handle_t provider,unsigned grade);
esp_err_t learning_provider_identify(learning_provider_handle_t provider,
    const learning_config_t *config,uint32_t generation,const void *jpeg,size_t jpeg_size,
    learning_questions_t *questions);
esp_err_t learning_provider_answer(learning_provider_handle_t provider,
    const learning_config_t *config, uint32_t generation, const char *question,
    const void *jpeg, size_t jpeg_size, const char *previous_answer,
    char *answer, size_t capacity);
esp_err_t learning_provider_transcribe(learning_provider_handle_t provider,
    const learning_config_t *config, uint32_t generation, const void *wav,
    size_t wav_size, char *text, size_t capacity);
esp_err_t learning_provider_audio(learning_provider_handle_t provider,
    const learning_config_t *config, uint32_t generation, const char *text,
    uint8_t **wav, size_t *size);
const char *learning_provider_error(learning_provider_handle_t provider);
// Maintenance probe: fixed provider host, no credentials or user content.
esp_err_t learning_provider_probe(unsigned region, int *http_status);
