// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_speech.h"
#include "learning_conversation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t wav[56];
static char history[LEARN_HISTORY_CAP];
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static void audio(void){
    memset(wav,0,sizeof(wav));memcpy(wav,"RIFF",4);put32(wav+4,48);memcpy(wav+8,"WAVEfmt ",8);
    put32(wav+16,16);wav[20]=1;wav[22]=1;put32(wav+24,24000);wav[32]=2;wav[34]=16;
    memcpy(wav+36,"data",4);put32(wav+40,12);
    for(unsigned i=0;i<6;++i){wav[44+2*i]=(uint8_t)(i*10);}
}
int main(void){
    learning_speech_info_t info;audio();
    assert(learning_speech_prepare(wav,sizeof(wav),&info)==ESP_OK&&info.bytes==8&&info.offset==44);
    assert(wav[44]==0&&wav[46]==15&&wav[48]==30&&wav[50]==45);
    const uint32_t markers[]={UINT32_MAX,0x7fffffffU,2147483547U};
    for(unsigned i=0;i<3;++i){audio();put32(wav+40,markers[i]);
        assert(learning_speech_prepare(wav,sizeof(wav),&info)==ESP_OK&&info.streaming_length&&info.bytes==8);}
    audio();put32(wav+40,14);assert(learning_speech_prepare(wav,sizeof(wav),&info)==ESP_ERR_INVALID_SIZE);
    audio();wav[22]=2;assert(learning_speech_prepare(wav,sizeof(wav),&info)==ESP_ERR_NOT_SUPPORTED);
    audio();put32(wav+16,100);assert(learning_speech_prepare(wav,sizeof(wav),&info)==ESP_ERR_INVALID_SIZE);
    learning_conversation_handle_t c;assert(learning_conversation_create(&c)==ESP_OK);
    uint8_t photo[]={1,2,3};assert(learning_conversation_set_photo(c,photo,3)==ESP_OK);photo[0]=9;
    const void *p;size_t n;learning_conversation_photo(c,&p,&n);assert(n==3&&((const uint8_t*)p)[0]==1);
    learning_conversation_answer(c,"原题问题","原题讲解");
    learning_conversation_answer(c,"为什么要除以3","因为系数是3");
    learning_conversation_history(c,history,sizeof(history));
    assert(strstr(history,"原题讲解")&&strstr(history,"为什么要除以3")&&strstr(history,"因为系数是3"));
    learning_conversation_select(c,"只讲第9题，右上方三角形");
    learning_conversation_photo(c,&p,&n);assert(n==3&&((const uint8_t*)p)[0]==1);
    learning_conversation_history(c,history,sizeof(history));assert(strstr(history,"第9题")&&!strstr(history,"原题讲解"));
    learning_conversation_answer(c,"第9题","第9题新讲解");learning_conversation_answer(c,"为什么","因为平行");
    learning_conversation_history(c,history,sizeof(history));
    assert(strstr(history,"第9题")&&strstr(history,"右上方三角形")&&strstr(history,"因为平行")&&!strstr(history,"因为系数是3"));
    learning_conversation_reset(c);learning_conversation_photo(c,&p,&n);assert(!p&&!n);
    learning_conversation_history(c,history,sizeof(history));assert(!history[0]);learning_conversation_delete(c);
    puts("PASS: WAV streaming markers, PCM conversion, truncation rejection, owned original photo and bounded follow-up context");
}
