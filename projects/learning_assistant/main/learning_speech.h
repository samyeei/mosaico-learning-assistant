// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { unsigned format,channels,rate,bits; uint32_t declared_bytes;
    size_t offset,bytes; bool streaming_length; } learning_speech_info_t;
// Input must be a completely received HTTP entity. PCM remains inside wav.
esp_err_t learning_speech_prepare(uint8_t *wav,size_t size,learning_speech_info_t *info);
