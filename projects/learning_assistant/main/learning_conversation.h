// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once
#include "learning_model.h"
#include "esp_err.h"
enum { LEARN_HISTORY_CAP=LEARN_TEXT_CAP*2+LEARN_VOICE_TEXT_CAP*2+512 };
typedef struct learning_conversation_t *learning_conversation_handle_t;
// Worker-only owner of the source photo and bounded first/latest Q&A context.
esp_err_t learning_conversation_create(learning_conversation_handle_t *out);
void learning_conversation_delete(learning_conversation_handle_t c);
void learning_conversation_reset(learning_conversation_handle_t c);
esp_err_t learning_conversation_set_photo(learning_conversation_handle_t c,const void *photo,size_t bytes);
void learning_conversation_photo(learning_conversation_handle_t c,const void **photo,size_t *bytes);
// Reset answer history for a new selection while retaining the owned original.
void learning_conversation_select(learning_conversation_handle_t c,const char *selection);
void learning_conversation_answer(learning_conversation_handle_t c,const char *question,const char *answer);
void learning_conversation_history(learning_conversation_handle_t c,char *out,size_t cap);
