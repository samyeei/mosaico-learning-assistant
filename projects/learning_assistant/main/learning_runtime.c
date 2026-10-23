// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_runtime.h"
#include "learning_platform.h"
#include "learning_provider.h"
#include "learning_network.h"
#include "learning_camera.h"
#include "learning_speech.h"
#include "learning_conversation.h"
#include "bsp/esp_mosaico.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_codec_dev.h"
#include "esp_timer.h"
#include "esp_iris.h"
#include "esp_log.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum { RATE=16000, SAMPLE_BYTES=2, AUDIO_CHUNK=512,
       WAV_HEADER=44, AUDIO_CAP=RATE*SAMPLE_BYTES*30,
       COMMAND_DEPTH=8, MESSAGE_DEPTH=4, WORKER_STACK=8192,
       DIAGNOSTIC_SERVICE=0x4c41 };
struct runtime_t {
    QueueHandle_t commands,messages;
    SemaphoreHandle_t audio_lock;
    learning_provider_handle_t provider;
    learning_camera_handle_t camera;
    learning_conversation_handle_t conversation;
    esp_gsp_handle_t ui;
    esp_codec_dev_handle_t mic,speaker;
    button_handle_t buttons[BSP_BUTTON_NUM];
    TaskHandle_t capture_task,worker_task;
    atomic_uint generation;
    atomic_bool key,recording,wants_record,overflow,ready,speech_stopped;
    atomic_uint record_after_ms;
    atomic_uint visible_page,diag_request,diag_bytes;
    atomic_bool diag_busy;
    atomic_int diag_result,diag_preview,diag_capture,diag_http;
    unsigned diag_registered;
    bool mic_open,speaker_open,previewing,last_online,last_configured;
    unsigned applied_volume;
    learning_api_state_t api_state,last_api_state;
    uint8_t *wav;
    size_t recorded;
    int16_t samples[AUDIO_CHUNK/2];
    learning_command_t command;
    learning_message_t message;
    // Includes room for the explanation prompt prefix; ASR itself stays bounded.
    char transcript[LEARN_VOICE_TEXT_CAP+128];
    const void *photo;size_t photo_size;
};
// App-lifetime singleton. UI owns model; worker owns cloud, speaker and camera;
// capture owns mic. Atomics, queues and audio_lock are the only cross-task state.
static struct runtime_t *runtime;
// RPC only schedules diagnostics; the existing worker retains exclusive camera
// ownership. Status contains errors/byte counts only, never credentials or images.
static esp_err_t diagnostic_rpc(const esp_iris_rpc_request_t *req,uint8_t *out,
    size_t capacity,size_t *used,void *ctx)
{
    struct runtime_t *r=ctx;
    if(!atomic_load(&r->ready))return ESP_ERR_INVALID_STATE;
    if(req->method_id==1){
        if(req->payload_size!=1||req->payload[0]<1||req->payload[0]>4)return ESP_ERR_INVALID_ARG;
        unsigned page=atomic_load(&r->visible_page);
        if(page!=LEARN_HOME&&page!=LEARN_SETTINGS)return ESP_ERR_INVALID_STATE;
        bool expected=false;
        if(!atomic_compare_exchange_strong(&r->diag_busy,&expected,true))return ESP_ERR_INVALID_STATE;
        atomic_store(&r->diag_result,ESP_ERR_NOT_FINISHED);
        atomic_store(&r->diag_bytes,0);atomic_store(&r->diag_http,0);
        atomic_store(&r->diag_preview,ESP_ERR_NOT_FINISHED);atomic_store(&r->diag_capture,ESP_ERR_NOT_FINISHED);
        atomic_store(&r->diag_request,req->payload[0]);
    }
    int n=snprintf((char*)out,capacity,
        "{\"busy\":%s,\"result\":%d,\"preview\":%d,\"capture\":%d,\"jpeg_bytes\":%u,\"http_status\":%d}",
        atomic_load(&r->diag_busy)?"true":"false",atomic_load(&r->diag_result),
        atomic_load(&r->diag_preview),atomic_load(&r->diag_capture),
        atomic_load(&r->diag_bytes),atomic_load(&r->diag_http));
    if(n<0||(size_t)n>=capacity)return ESP_ERR_INVALID_SIZE;
    *used=(size_t)n;return ESP_OK;
}
static void diagnostic_run(struct runtime_t *r,unsigned action)
{
    esp_err_t err=ESP_ERR_INVALID_STATE;
    unsigned page=atomic_load(&r->visible_page);
    if((page!=LEARN_HOME&&page!=LEARN_SETTINGS)||r->previewing||atomic_load(&r->recording))goto done;
    if(action==4){
        const learning_config_t *config=learning_network_config();
        if(config&&learning_network_online()){
            err=learning_provider_test(config,r->transcript,sizeof(r->transcript));
            r->api_state=err==ESP_OK?LEARN_API_READY:LEARN_API_ERROR;
        }
        goto done;
    }
    if(action>1){
        if(learning_network_online()){
            int status=0;err=learning_provider_probe(action-2,&status);atomic_store(&r->diag_http,status);
        }
        goto done;
    }
    r->photo=NULL;r->photo_size=0;
    err=learning_camera_delete(r->camera);if(err!=ESP_OK)goto done;r->camera=NULL;
    err=learning_camera_create(r->ui,&r->camera);
    if(err==ESP_OK)err=learning_camera_start(r->camera);
    // Exercise repeated buffer replacement/release, not just frame acquisition.
    for(unsigned i=0;err==ESP_OK&&i<30;++i){
        err=learning_camera_preview(r->camera);atomic_store(&r->diag_preview,err);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if(err==ESP_OK){
        const void *jpeg=NULL;size_t bytes=0;
        err=learning_camera_capture(r->camera,&jpeg,&bytes);
        atomic_store(&r->diag_capture,err);atomic_store(&r->diag_bytes,(unsigned)bytes);
    }
    {esp_err_t cleaned=learning_camera_delete(r->camera);if(cleaned==ESP_OK)r->camera=NULL;if(err==ESP_OK)err=cleaned;}
done:
    ESP_LOGI("learning_diag","Action %u result=%s jpeg=%u http=%d",action,esp_err_to_name(err),
        atomic_load(&r->diag_bytes),atomic_load(&r->diag_http));
    atomic_store(&r->diag_result,err);atomic_store(&r->diag_busy,false);
}
static bool current(void *ctx,uint32_t gen)
{struct runtime_t *r=ctx;return atomic_load(&r->generation)==gen;}
static void publish(struct runtime_t *r,learning_message_kind_t kind,uint32_t gen,const char *text)
{
    memset(&r->message,0,sizeof(r->message));r->message.kind=kind;r->message.generation=gen;
    if(text)snprintf(r->message.text,sizeof(r->message.text),"%s",text);
    if(xQueueSend(r->messages,&r->message,pdMS_TO_TICKS(100))!=pdTRUE){
        // Preserve a terminal event by dropping the oldest progress item.
        learning_message_t *discard=malloc(sizeof(*discard));
        if(discard){xQueueReceive(r->messages,discard,0);free(discard);xQueueSend(r->messages,&r->message,0);}
    }
}
static void failure(struct runtime_t *r,uint32_t gen,const char *text)
{if(current(r,gen))publish(r,LEARN_MSG_ERROR,gen,text);}
static void put16(uint8_t *p,unsigned x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void put32(uint8_t *p,uint32_t x){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(x>>(8*i));}
static void header(struct runtime_t *r)
{
    uint8_t *p=r->wav;memset(p,0,WAV_HEADER);
    memcpy(p,"RIFF",4);put32(p+4,(uint32_t)r->recorded+36);memcpy(p+8,"WAVEfmt ",8);
    put32(p+16,16);put16(p+20,1);put16(p+22,1);put32(p+24,RATE);
    put32(p+28,RATE*2);put16(p+32,2);put16(p+34,16);memcpy(p+36,"data",4);put32(p+40,(uint32_t)r->recorded);
}
static void capture(void *ctx)
{
    struct runtime_t *r=ctx;ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    for(;;){
        if(!r->mic_open){vTaskDelay(pdMS_TO_TICKS(100));continue;}
        uint32_t gen=atomic_load(&r->generation);bool rec=atomic_load(&r->recording);
        int err=esp_codec_dev_read(r->mic,r->samples,sizeof(r->samples));
        if(err!=ESP_CODEC_DEV_OK){if(rec)atomic_store(&r->overflow,true);vTaskDelay(pdMS_TO_TICKS(20));continue;}
        if(!rec||(int32_t)((uint32_t)(esp_timer_get_time()/1000)-atomic_load(&r->record_after_ms))<0)continue;
        xSemaphoreTake(r->audio_lock,portMAX_DELAY);
        if(atomic_load(&r->recording)&&current(r,gen)){
            if(r->recorded+sizeof(r->samples)>AUDIO_CAP)atomic_store(&r->overflow,true);
            else {memcpy(r->wav+WAV_HEADER+r->recorded,r->samples,sizeof(r->samples));r->recorded+=sizeof(r->samples);}
        }
        xSemaphoreGive(r->audio_lock);
    }
}
static esp_err_t play(struct runtime_t *r,uint32_t gen,uint8_t *wav,size_t size)
{
    learning_speech_info_t info;
    esp_err_t parsed=learning_speech_prepare(wav,size,&info);
    ESP_LOGI("learning_speech","WAV: total=%u format=%u channels=%u rate=%u bits=%u declared=%u streaming=%u pcm=%u result=%s",
        (unsigned)size,info.format,info.channels,info.rate,info.bits,(unsigned)info.declared_bytes,
        info.streaming_length,(unsigned)info.bytes,esp_err_to_name(parsed));
    if(parsed!=ESP_OK)return parsed;
    size_t pcm=info.offset,bytes=info.bytes;
    for(size_t sent=0;sent<bytes;){
        if(!current(r,gen)||atomic_load(&r->speech_stopped))return ESP_ERR_INVALID_STATE;
        learning_controls_t controls;learning_platform_controls_get(&controls);
        if(controls.volume!=r->applied_volume){
            if(esp_codec_dev_set_out_vol(r->speaker,(int)controls.volume)!=ESP_CODEC_DEV_OK)return ESP_FAIL;
            r->applied_volume=controls.volume;
            ESP_LOGI("learning_speech","Volume applied: %u",r->applied_volume);
        }
        size_t chunk=bytes-sent>1024?1024:bytes-sent;
        int written=esp_codec_dev_write(r->speaker,wav+pcm+sent,(int)chunk);
        if(written!=ESP_CODEC_DEV_OK){ESP_LOGE("learning_speech","Speaker write failed: code=%d offset=%u",written,(unsigned)sent);return ESP_FAIL;}
        sent+=chunk;
    }
    return ESP_OK;
}
static void speak(struct runtime_t *r,const learning_command_t *c,const learning_config_t *config)
{
    if(!r->speaker_open){publish(r,LEARN_MSG_SPEECH_DONE,c->generation,"扬声器未就绪，文字已保留");return;}
    uint8_t *wav=NULL;size_t size=0;
    esp_err_t err=learning_provider_audio(r->provider,config,c->generation,c->text,&wav,&size);
    if(err==ESP_OK)err=play(r,c->generation,wav,size);
    free(wav);
    ESP_LOGI("learning_speech","Playback finished: result=%s",esp_err_to_name(err));
    if(current(r,c->generation))publish(r,LEARN_MSG_SPEECH_DONE,c->generation,
        err==ESP_OK||err==ESP_ERR_INVALID_STATE||atomic_load(&r->speech_stopped)?NULL:"朗读失败，文字已保留，可重试");
}
static void network_status(struct runtime_t *r,bool force)
{
    bool online=learning_network_online(),configured=learning_network_configured();
    if(!force&&online==r->last_online&&configured==r->last_configured&&r->api_state==r->last_api_state)return;
    if(!online)r->api_state=LEARN_API_UNKNOWN;
    r->last_online=online;r->last_configured=configured;r->last_api_state=r->api_state;
    memset(&r->message,0,sizeof(r->message));r->message.kind=LEARN_MSG_STATUS;
    r->message.online=online;r->message.configured=configured;r->message.api_state=r->api_state;
    if(!online)snprintf(r->message.text,sizeof(r->message.text),"%s",configured?"网络未连接，可在设置中重新配置":"请先用手机配置 Wi-Fi 和模型");
    xQueueSend(r->messages,&r->message,pdMS_TO_TICKS(100));
}
static void setup_message(struct runtime_t *r,uint32_t gen)
{
    const char *ssid=NULL,*password=NULL;
    if(learning_network_setup_info(&ssid,&password)){
        memset(&r->message,0,sizeof(r->message));r->message.kind=LEARN_MSG_SETUP;r->message.generation=gen;
        snprintf(r->message.text,sizeof(r->message.text),"%s\n无需密码，手机直接连接\n打开 192.168.4.1",ssid);
        xQueueSend(r->messages,&r->message,pdMS_TO_TICKS(100));
    }
}
static void worker(void *ctx)
{
    struct runtime_t *r=ctx;ulTaskNotifyTake(pdTRUE,portMAX_DELAY);network_status(r,true);setup_message(r,1);
    for(;;){
        network_status(r,false);
        // Also clean up after a failed UI command submission. That path has
        // no queued cancel, but must never leave the camera LED running.
        if(r->previewing&&atomic_load(&r->visible_page)==LEARN_ERROR){
            r->previewing=false;
            esp_err_t stopped=learning_camera_stop(r->camera);
            if(stopped!=ESP_OK)ESP_LOGW("learning_camera","Error cleanup: %s",esp_err_to_name(stopped));
        }
        unsigned diagnostic=atomic_exchange(&r->diag_request,0);
        if(diagnostic)diagnostic_run(r,diagnostic);
        if(xQueueReceive(r->commands,&r->command,pdMS_TO_TICKS(r->previewing?200:100))!=pdTRUE){
            if(r->previewing&&learning_camera_preview(r->camera)!=ESP_OK){r->previewing=false;learning_camera_stop(r->camera);failure(r,atomic_load(&r->generation),"相机取帧失败，请检查左侧插件后重试");}
            continue;
        }
        learning_command_t *c=&r->command;
        if(!current(r,c->generation))continue;
        const learning_config_t *config=learning_network_config();
        if(c->kind==LEARN_CMD_CANCEL){
            if(!c->followup&&!c->keep_photo)learning_conversation_reset(r->conversation);
            r->previewing=false;r->photo=NULL;r->photo_size=0;
            if(learning_camera_delete(r->camera)==ESP_OK)r->camera=NULL;
            else failure(r,c->generation,"相机停止失败，请稍后重试或重新启动");
            continue;
        }
        if(c->kind==LEARN_CMD_SETUP){
            if(learning_network_setup()!=ESP_OK)failure(r,c->generation,"无法开启配置热点，请稍后重试");
            else setup_message(r,c->generation);
            continue;
        }
        if(c->kind==LEARN_CMD_MUTE){continue;}
        if(c->kind==LEARN_CMD_CAMERA_OPTIONS){
            esp_err_t err=learning_camera_options(r->camera,c->light,c->flipped);
            memset(&r->message,0,sizeof(r->message));r->message.kind=LEARN_MSG_CAMERA_OPTIONS;
            r->message.generation=c->generation;
            if(err==ESP_OK){r->message.light=c->light;r->message.flipped=c->flipped;}
            else{
                learning_camera_options(r->camera,false,false);
                r->message.kind=LEARN_MSG_ERROR;
                snprintf(r->message.text,sizeof(r->message.text),"补光或翻转失败，请重试");
            }
            xQueueSend(r->messages,&r->message,pdMS_TO_TICKS(100));continue;
        }
        if(c->kind==LEARN_CMD_CAMERA){
            learning_conversation_reset(r->conversation);
            r->photo=NULL;r->photo_size=0;r->previewing=false;
            if(!r->camera&&learning_camera_create(r->ui,&r->camera)!=ESP_OK){failure(r,c->generation,"相机内存不足，请重试");continue;}
            esp_err_t err=learning_camera_start(r->camera);
            if(err==ESP_OK)err=learning_camera_options(r->camera,false,c->flipped);
            if(err!=ESP_OK)failure(r,c->generation,err==ESP_ERR_NO_MEM?"相机内存不足，请返回后重试":"相机启动失败，请检查左侧插件后重试");else r->previewing=true;continue;
        }
        if(c->kind==LEARN_CMD_CAPTURE){
            r->previewing=false;
            if(learning_camera_capture(r->camera,&r->photo,&r->photo_size)!=ESP_OK)failure(r,c->generation,"拍照失败，请保持稳定后重试");
            else publish(r,LEARN_MSG_PHOTO,c->generation,NULL);
            continue;
        }
        if(!config||!learning_network_online()){failure(r,c->generation,"网络或模型未配置，请先打开连接设置");continue;}
        if(c->kind==LEARN_CMD_IDENTIFY){
            if(r->photo&&r->photo_size){
                if(learning_conversation_set_photo(r->conversation,r->photo,r->photo_size)!=ESP_OK){failure(r,c->generation,"保存原题内存不足，请重拍");continue;}
                r->photo=NULL;r->photo_size=0;
            }
            if(learning_camera_delete(r->camera)!=ESP_OK){failure(r,c->generation,"相机停止失败，请重试");continue;}r->camera=NULL;
            const void *original=NULL;size_t bytes=0;learning_conversation_photo(r->conversation,&original,&bytes);
            if(!original||!bytes){failure(r,c->generation,"照片已失效，请重新拍照");continue;}
            memset(&r->message,0,sizeof(r->message));r->message.kind=LEARN_MSG_QUESTIONS;r->message.generation=c->generation;
            esp_err_t err=learning_provider_identify(r->provider,config,c->generation,original,bytes,&r->message.questions);
            if(current(r,c->generation)){
                r->api_state=err==ESP_OK?LEARN_API_READY:LEARN_API_ERROR;
                if(err==ESP_OK){
                    ESP_LOGI("learning_questions","Identified %u candidates; waiting for selection",r->message.questions.count);
                    if(xQueueSend(r->messages,&r->message,pdMS_TO_TICKS(100))!=pdTRUE)failure(r,c->generation,"题目列表暂时无法显示，请重试");
                }else failure(r,c->generation,learning_provider_error(r->provider));
            }
            continue;
        }
        if(c->kind==LEARN_CMD_RECORD){
            if(!c->followup)learning_conversation_reset(r->conversation);
            r->previewing=false;r->photo=NULL;r->photo_size=0;
            if(learning_camera_delete(r->camera)==ESP_OK)r->camera=NULL;
            else{atomic_store(&r->recording,false);failure(r,c->generation,"相机仍在停止，请稍后重新提问");continue;}
            if(!r->mic_open){failure(r,c->generation,"麦克风未就绪，请稍后重试");continue;}
            // Capture starts at nonblocking submit, even if an interrupted
            // HTTP request is still leaving its bounded socket timeout.
            continue;
        }
        if(c->kind==LEARN_CMD_SPEAK){if(!atomic_load(&r->speech_stopped))speak(r,c,config);continue;}
        const char *question=NULL;const void *photo=NULL;size_t photo_size=0;
        if(c->kind==LEARN_CMD_SEND_AUDIO){
            atomic_store(&r->recording,false);
            xSemaphoreTake(r->audio_lock,portMAX_DELAY);
            if(!current(r,c->generation)){xSemaphoreGive(r->audio_lock);continue;}
            bool valid=!atomic_load(&r->overflow)&&r->recorded>=3200;
            header(r);size_t size=r->recorded+WAV_HEADER;
            uint8_t *recording=valid?malloc(size):NULL;
            if(recording)memcpy(recording,r->wav,size);
            xSemaphoreGive(r->audio_lock);
            if(!valid){failure(r,c->generation,"录音不完整或太短，请重新提问");continue;}
            if(!recording){failure(r,c->generation,"录音内存不足，请稍后重试");continue;}
            esp_err_t transcribed=learning_provider_transcribe(r->provider,config,c->generation,recording,size,r->transcript,LEARN_VOICE_TEXT_CAP);
            free(recording);
            if(transcribed!=ESP_OK){if(current(r,c->generation))r->api_state=LEARN_API_ERROR;failure(r,c->generation,learning_provider_error(r->provider));continue;}
            r->api_state=LEARN_API_READY;
            publish(r,LEARN_MSG_TRANSCRIPT,c->generation,r->transcript);continue;
        }else if(c->kind==LEARN_CMD_SEND_TEXT){
            question=c->text;
        }else if(c->kind==LEARN_CMD_SOLVE){
            learning_conversation_photo(r->conversation,&photo,&photo_size);
            if(!photo||!photo_size||!c->text[0]){failure(r,c->generation,"所选题目已失效，请重新拍照");continue;}
            learning_conversation_select(r->conversation,c->text);question=c->text;
        }else if(c->kind==LEARN_CMD_EXPLAIN){
            snprintf(r->transcript,sizeof(r->transcript),"我没听懂以下这一页，请结合刚才的题目换一种方式讲：\n%s",c->text);question=r->transcript;
        }
        if(!question)continue;
        learning_conversation_photo(r->conversation,&photo,&photo_size);
        char *answer=calloc(1,LEARN_TEXT_CAP),*history=calloc(1,LEARN_HISTORY_CAP);
        if(!answer||!history){free(answer);free(history);failure(r,c->generation,"设备内存不足，请稍后重试");continue;}
        learning_conversation_history(r->conversation,history,LEARN_HISTORY_CAP);
        ESP_LOGI("learning_conversation","Question: followup=%u image_bytes=%u history_bytes=%u",
            c->followup,(unsigned)photo_size,(unsigned)strlen(history));
        learning_controls_t controls;learning_platform_controls_get(&controls);
        if(!learning_provider_set_grade(r->provider,controls.grade)){
            free(answer);free(history);failure(r,c->generation,"年级设置异常，请重新选择");continue;
        }
        esp_err_t err=learning_provider_answer(r->provider,config,c->generation,question,photo,photo_size,history,answer,LEARN_TEXT_CAP);
        if(current(r,c->generation))r->api_state=err==ESP_OK?LEARN_API_READY:LEARN_API_ERROR;
        if(err==ESP_OK&&current(r,c->generation)){
            learning_conversation_answer(r->conversation,question,answer);publish(r,LEARN_MSG_ANSWER,c->generation,answer);
        }else failure(r,c->generation,learning_provider_error(r->provider));
        free(answer);free(history);
    }
}
static void key_changed(void *button,void *ctx)
{struct runtime_t *r=ctx;atomic_store(&r->key,iot_button_get_event(button)==BUTTON_PRESS_DOWN);}
bool learning_platform_simulated(void){return false;}
void learning_platform_presented(const learning_snapshot_t *snapshot)
{if(runtime&&snapshot)atomic_store(&runtime->visible_page,snapshot->page);}
bool learning_platform_key(void){return runtime&&atomic_load(&runtime->key);}
bool learning_platform_poll(learning_message_t *out)
{return runtime&&xQueueReceive(runtime->messages,out,0)==pdTRUE;}
bool learning_platform_submit(const learning_command_t *c)
{
    struct runtime_t *r=runtime;if(!r||!atomic_load(&r->ready))return false;
    if(c->generation<atomic_load(&r->generation))return true;
    if(c->kind==LEARN_CMD_MUTE){
        atomic_store(&r->speech_stopped,true);
        learning_platform_controls_set(LEARN_CONTROL_SPEECH,c->muted?0:1);return true;
    }
    atomic_store(&r->recording,false);
    atomic_store(&r->wants_record,c->kind==LEARN_CMD_RECORD);
    atomic_store(&r->generation,c->generation);
    // A new explicit read has a new generation, so it cannot revive an old
    // stream when clearing this latch. Mute never cancels recording/ASR.
    if(c->kind==LEARN_CMD_SPEAK)atomic_store(&r->speech_stopped,false);
    learning_platform_controls_set(LEARN_CONTROL_SPEECH,c->muted?0:1);
    if(c->kind==LEARN_CMD_RECORD){
        if(!r->mic_open||xSemaphoreTake(r->audio_lock,0)!=pdTRUE)return false;
        r->recorded=0;atomic_store(&r->overflow,false);
        atomic_store(&r->record_after_ms,(uint32_t)(esp_timer_get_time()/1000)+150);
        atomic_store(&r->recording,true);xSemaphoreGive(r->audio_lock);
    }
    bool queued=xQueueSend(r->commands,c,0)==pdTRUE;
    if(!queued)atomic_store(&r->recording,false);
    return queued;
}
esp_err_t learning_runtime_start(esp_gsp_handle_t ui)
{
    if(runtime||!ui)return ESP_ERR_INVALID_ARG;
    struct runtime_t *r=calloc(1,sizeof(*r));if(!r)return ESP_ERR_NO_MEM;
    r->ui=ui;
    atomic_init(&r->generation,1);atomic_init(&r->key,false);atomic_init(&r->recording,false);
    atomic_init(&r->overflow,false);atomic_init(&r->ready,false);atomic_init(&r->speech_stopped,true);atomic_init(&r->wants_record,false);
    atomic_init(&r->record_after_ms,0);
    atomic_init(&r->visible_page,LEARN_SETTINGS);atomic_init(&r->diag_request,0);atomic_init(&r->diag_bytes,0);
    atomic_init(&r->diag_busy,false);atomic_init(&r->diag_result,ESP_ERR_NOT_FINISHED);
    atomic_init(&r->diag_preview,ESP_ERR_NOT_FINISHED);atomic_init(&r->diag_capture,ESP_ERR_NOT_FINISHED);atomic_init(&r->diag_http,0);
    r->commands=xQueueCreate(COMMAND_DEPTH,sizeof(learning_command_t));
    r->messages=xQueueCreate(MESSAGE_DEPTH,sizeof(learning_message_t));r->audio_lock=xSemaphoreCreateMutex();
    r->wav=heap_caps_malloc(AUDIO_CAP+WAV_HEADER,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    esp_err_t err=ESP_ERR_NO_MEM;
    if(!r->commands||!r->messages||!r->audio_lock||!r->wav)goto fail;
    err=learning_provider_create(current,r,&r->provider);if(err!=ESP_OK)goto fail;
    err=learning_camera_create(ui,&r->camera);if(err!=ESP_OK)goto fail;
    err=learning_conversation_create(&r->conversation);if(err!=ESP_OK)goto fail;
    r->mic=bsp_audio_codec_microphone_init();r->speaker=bsp_audio_codec_speaker_init();
    esp_codec_dev_sample_info_t format={.sample_rate=RATE,.bits_per_sample=16,.channel=1};
    if(r->mic&&esp_codec_dev_open(r->mic,&format)==ESP_CODEC_DEV_OK){
        r->mic_open=true;
        if(esp_codec_dev_set_in_gain(r->mic,24.0f)!=ESP_CODEC_DEV_OK){esp_codec_dev_close(r->mic);r->mic_open=false;}}
    if(r->speaker&&esp_codec_dev_open(r->speaker,&format)==ESP_CODEC_DEV_OK){
        r->speaker_open=true;
        learning_controls_t controls;learning_platform_controls_get(&controls);r->applied_volume=controls.volume;
        if(esp_codec_dev_set_out_vol(r->speaker,(int)controls.volume)!=ESP_CODEC_DEV_OK){esp_codec_dev_close(r->speaker);r->speaker_open=false;}}
    int count=0;err=bsp_iot_button_create(r->buttons,&count,BSP_BUTTON_NUM);if(err!=ESP_OK)goto fail;
    err=iot_button_register_cb(r->buttons[BSP_BUTTON_AI],BUTTON_PRESS_DOWN,NULL,key_changed,r);if(err!=ESP_OK)goto fail;
    err=iot_button_register_cb(r->buttons[BSP_BUTTON_AI],BUTTON_PRESS_UP,NULL,key_changed,r);if(err!=ESP_OK)goto fail;
    if(xTaskCreate(capture,"learn_mic",4096,r,5,&r->capture_task)!=pdPASS){err=ESP_ERR_NO_MEM;goto fail;}
    if(xTaskCreate(worker,"learn_worker",WORKER_STACK,r,4,&r->worker_task)!=pdPASS){err=ESP_ERR_NO_MEM;goto fail;}
    err=esp_iris_rpc_register(DIAGNOSTIC_SERVICE,1,diagnostic_rpc,r);if(err!=ESP_OK)goto fail;r->diag_registered=1;
    err=esp_iris_rpc_register(DIAGNOSTIC_SERVICE,2,diagnostic_rpc,r);if(err!=ESP_OK)goto fail;r->diag_registered=2;
    runtime=r;atomic_store(&r->ready,true);xTaskNotifyGive(r->capture_task);xTaskNotifyGive(r->worker_task);return ESP_OK;
fail:
    if(r->diag_registered>1)esp_iris_rpc_unregister(DIAGNOSTIC_SERVICE,2);
    if(r->diag_registered)esp_iris_rpc_unregister(DIAGNOSTIC_SERVICE,1);
    if(r->capture_task)vTaskDelete(r->capture_task);
    if(r->worker_task)vTaskDelete(r->worker_task);
    for(unsigned i=0;i<BSP_BUTTON_NUM;++i)if(r->buttons[i])iot_button_delete(r->buttons[i]);
    if(r->mic_open)esp_codec_dev_close(r->mic);
    if(r->speaker_open)esp_codec_dev_close(r->speaker);
    learning_camera_delete(r->camera);learning_provider_delete(r->provider);
    learning_conversation_delete(r->conversation);
    if(r->commands)vQueueDelete(r->commands);
    if(r->messages)vQueueDelete(r->messages);
    if(r->audio_lock)vSemaphoreDelete(r->audio_lock);
    free(r->wav);free(r);return err;
}
