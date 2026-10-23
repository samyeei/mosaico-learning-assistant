// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_controls_device.h"
#include "learning_platform.h"
#include "app_orientation.h"
#include "bsp/esp_mosaico.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

// App-lifetime singleton. Render callbacks publish a packed atomic snapshot;
// this internal-stack task alone owns brightness changes and NVS writes.
// Speaker gain is consumed by the audio worker at each PCM block boundary.
static atomic_uint desired;
static TaskHandle_t task;
void learning_platform_controls_get(learning_controls_t *out)
{if(!learning_controls_unpack(atomic_load(&desired),out))*out=learning_controls_defaults();}
bool learning_platform_controls_set(learning_control_t control,int value)
{
    unsigned old=atomic_load(&desired),next;learning_controls_t v;
    do{
        if(!learning_controls_unpack(old,&v))v=learning_controls_defaults();
        if(!learning_controls_change(&v,control,value,app_orientation_get()))return false;
        next=learning_controls_pack(&v);
    }while(!atomic_compare_exchange_weak(&desired,&old,next));
    if(control==LEARN_CONTROL_AUTO)app_orientation_set_mode(v.automatic,v.fixed_rotation);
    return true;
}
static esp_err_t brightness(esp_gsp_handle_t ui,unsigned value)
{
    app_orientation_frame_lock();
    esp_gsp_esp_lcd_pause_t *pause=NULL;
    esp_err_t err=esp_gsp_esp_lcd_pause(ui,1000,&pause);
    if(err==ESP_OK){
        err=bsp_display_brightness_set((int)value);
        esp_gsp_handle_t resumed=NULL;unsigned retries=0;
        while(esp_gsp_esp_lcd_resume_paused(pause,&resumed)!=ESP_OK){
            if(retries++%20==0)ESP_LOGW("learning_controls","Waiting to resume after brightness change");
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    app_orientation_frame_unlock();return err;
}
static esp_err_t save(unsigned value)
{
    nvs_handle_t nvs;esp_err_t err=nvs_open("learn_ui",NVS_READWRITE,&nvs);
    if(err!=ESP_OK)return err;
    err=nvs_set_u32(nvs,"controls",value);if(err==ESP_OK)err=nvs_commit(nvs);
    nvs_close(nvs);return err;
}
static void controls_task(void *ctx)
{
    esp_gsp_handle_t ui=ctx;
    unsigned applied_brightness=0,last_seen=atomic_load(&desired),saved=last_seen;
    TickType_t changed=xTaskGetTickCount();
    for(;;){
        unsigned word=atomic_load(&desired);learning_controls_t v;
        if(!learning_controls_unpack(word,&v)){vTaskDelay(pdMS_TO_TICKS(80));continue;}
        if(v.brightness!=applied_brightness){
            esp_err_t err=brightness(ui,v.brightness);
            if(err==ESP_OK)applied_brightness=v.brightness;
            else ESP_LOGW("learning_controls","Brightness change failed: %s",esp_err_to_name(err));
        }
        TickType_t now=xTaskGetTickCount();
        if(word!=last_seen){last_seen=word;changed=now;}
        if(word!=saved&&now-changed>=pdMS_TO_TICKS(750)){
            esp_err_t err=save(word);
            if(err==ESP_OK){saved=word;ESP_LOGI("learning_controls","Saved: brightness=%u volume=%u speech=%u auto=%u fixed=%u",
                v.brightness,v.volume,v.speech_enabled,v.automatic,v.fixed_rotation);}
            else{changed=now;ESP_LOGW("learning_controls","Save failed: %s",esp_err_to_name(err));}
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}
esp_err_t learning_controls_start(esp_gsp_handle_t ui)
{
    if(!ui||task)return ESP_ERR_INVALID_STATE;
    learning_controls_t v=learning_controls_defaults();unsigned word=learning_controls_pack(&v);uint32_t loaded=0;
    nvs_handle_t nvs;
    if(nvs_open("learn_ui",NVS_READONLY,&nvs)==ESP_OK){
        if(nvs_get_u32(nvs,"controls",&loaded)==ESP_OK&&learning_controls_unpack(loaded,&v))word=loaded;
        nvs_close(nvs);
    }
    atomic_store(&desired,word);app_orientation_set_mode(v.automatic,v.fixed_rotation);
    if(xTaskCreate(controls_task,"learn_controls",6144,ui,4,&task)!=pdPASS)return ESP_ERR_NO_MEM;
    return ESP_OK;
}
