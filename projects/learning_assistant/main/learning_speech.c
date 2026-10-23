// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include <stdbool.h>
#include "learning_speech.h"
#include <string.h>
static unsigned u16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static uint32_t u32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
// Observed in a fully received Qwen-TTS WAV: nominal streaming data length,
// not the downloaded entity size. Accept this exact marker, not arbitrary
// oversized chunks, and bound playback by the complete HTTP body.
#define QWEN_STREAM_DATA_LENGTH 0x7fffff9bU
esp_err_t learning_speech_prepare(uint8_t *wav,size_t size,learning_speech_info_t *v)
{
    if(!wav||!v)return ESP_ERR_INVALID_ARG;
    memset(v,0,sizeof(*v));
    if(size<44||memcmp(wav,"RIFF",4)||memcmp(wav+8,"WAVE",4))return ESP_ERR_NOT_SUPPORTED;
    bool fmt=false;
    for(size_t pos=12;pos+8<=size;){
        uint32_t n=u32(wav+pos+4);size_t remain=size-pos-8;
        if(!memcmp(wav+pos,"data",4)){
            v->offset=pos+8;v->declared_bytes=n;
            if(n>remain){
                // Streaming WAV writers cannot seek back to patch these
                // explicit unknown-length markers. HTTP completeness was
                // checked before this parser; never read past that body.
                if(n!=UINT32_MAX&&n!=0x7fffffffU&&n!=QWEN_STREAM_DATA_LENGTH)return ESP_ERR_INVALID_SIZE;
                v->streaming_length=true;v->bytes=remain;
            }else v->bytes=n;
            break;
        }
        if(n>remain)return ESP_ERR_INVALID_SIZE;
        if(!memcmp(wav+pos,"fmt ",4)){
            if(n<16)return ESP_ERR_INVALID_SIZE;
            v->format=u16(wav+pos+8);v->channels=u16(wav+pos+10);
            v->rate=u32(wav+pos+12);v->bits=u16(wav+pos+22);fmt=true;
        }
        pos+=8+(size_t)n+(n&1U);
    }
    if(!fmt||!v->offset||!v->bytes||v->bytes%2)return ESP_ERR_INVALID_SIZE;
    if(v->format!=1||v->channels!=1||v->bits!=16||(v->rate!=16000&&v->rate!=24000))return ESP_ERR_NOT_SUPPORTED;
    if(v->rate==24000){
        uint8_t *pcm=wav+v->offset;size_t used=0;
        for(size_t i=0;i+6<=v->bytes;i+=6){
            int16_t a=(int16_t)u16(pcm+i),b=(int16_t)u16(pcm+i+2),c=(int16_t)u16(pcm+i+4);
            pcm[used++]=(uint8_t)a;pcm[used++]=(uint8_t)((uint16_t)a>>8);
            int16_t mean=(int16_t)(((int)b+c)/2);pcm[used++]=(uint8_t)mean;pcm[used++]=(uint8_t)((uint16_t)mean>>8);
        }
        v->bytes=used;
    }
    return v->bytes?ESP_OK:ESP_ERR_INVALID_SIZE;
}
