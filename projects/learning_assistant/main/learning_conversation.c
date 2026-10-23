// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_conversation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct learning_conversation_t { void *photo;size_t bytes;
    char first[LEARN_TEXT_CAP],last[LEARN_TEXT_CAP],question[1024],selection[1024]; };
esp_err_t learning_conversation_create(learning_conversation_handle_t *out)
{if(!out)return ESP_ERR_INVALID_ARG;*out=calloc(1,sizeof(**out));return *out?ESP_OK:ESP_ERR_NO_MEM;}
void learning_conversation_reset(learning_conversation_handle_t c)
{if(c){free(c->photo);memset(c,0,sizeof(*c));}}
void learning_conversation_delete(learning_conversation_handle_t c)
{learning_conversation_reset(c);free(c);}
esp_err_t learning_conversation_set_photo(learning_conversation_handle_t c,const void *p,size_t n)
{
    if(!c||!p||!n)return ESP_ERR_INVALID_ARG;
    void *copy=malloc(n);if(!copy)return ESP_ERR_NO_MEM;
    memcpy(copy,p,n);free(c->photo);c->photo=copy;c->bytes=n;return ESP_OK;
}
void learning_conversation_photo(learning_conversation_handle_t c,const void **p,size_t *n)
{*p=c->photo;*n=c->bytes;}
void learning_conversation_select(learning_conversation_handle_t c,const char *selection)
{c->first[0]=c->last[0]=c->question[0]=0;snprintf(c->selection,sizeof(c->selection),"%s",selection);}
void learning_conversation_answer(learning_conversation_handle_t c,const char *q,const char *a)
{
    if(!c->first[0])snprintf(c->first,sizeof(c->first),"%s",a);
    snprintf(c->last,sizeof(c->last),"%s",a);snprintf(c->question,sizeof(c->question),"%s",q);
}
void learning_conversation_history(learning_conversation_handle_t c,char *out,size_t cap)
{
    if(!c->first[0]){if(cap)snprintf(out,cap,"%s",c->selection);return;}
    snprintf(out,cap,"所选题目：\n%s\n原题首次讲解：\n%s\n最近提问：\n%s\n最近回答：\n%s",c->selection,c->first,c->question,c->last);
}
