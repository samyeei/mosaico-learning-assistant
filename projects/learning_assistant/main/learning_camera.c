// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_camera.h"
#include "learning_viewport.h"
#include "app_orientation.h"
#include "mosaico_module_camera.h"
#include "mosaico_module_mgr.h"
#include "esp_jpeg_enc.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
// Canvas preview targets the authored image bind.
#define GSP_BUNDLE_ENABLE_RAW_IDS
#include "bundle_gsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <linux/videodev2.h>

enum { PREVIEW_W = LEARN_PREVIEW_W, PREVIEW_H = LEARN_PREVIEW_H,
       JPEG_CAP = 256 * 1024, PREVIEW_BUFFERS = 2, CANVAS_FLUSH_MS = 2000 };
struct preview_buffer_t {
    uint16_t *pixels;
    atomic_bool borrowed;
    atomic_uint releases;
};
// Single worker owns the driver and conversion. GSP borrows immutable RGB565
// frames; only its release callback returns a slot to the worker. Stop + flush
// must return every slot before deleting the camera. No image goes to flash.
struct learning_camera_t {
    esp_gsp_handle_t ui;
    mosaico_camera_handle_t sensor;
    bool opened, streaming, frame_logged, canvas_active,light,flipped;
    uint8_t *jpeg;
    struct preview_buffer_t preview[PREVIEW_BUFFERS];
    unsigned submitted, dropped;
    size_t jpeg_size;
};
esp_err_t learning_camera_create(esp_gsp_handle_t ui,learning_camera_handle_t *out)
{
    if(!ui||!out)return ESP_ERR_INVALID_ARG;
    *out=calloc(1,sizeof(**out));if(!*out)return ESP_ERR_NO_MEM;
    (*out)->ui=ui;
    for(unsigned i=0;i<PREVIEW_BUFFERS;++i){
        atomic_init(&(*out)->preview[i].borrowed,false);
        atomic_init(&(*out)->preview[i].releases,0);
    }
    return ESP_OK;
}
static esp_err_t stop_sensor(learning_camera_handle_t c)
{
    if(!c)return ESP_OK;
    if(c->sensor&&c->light){esp_err_t err=mosaico_camera_flash_stop(c->sensor);if(err!=ESP_OK)return err;c->light=false;}
    if(c->streaming){esp_err_t err=mosaico_camera_stop_stream(c->sensor);if(err!=ESP_OK)return err;c->streaming=false;}
    if(c->opened){esp_err_t err=mosaico_camera_close(c->sensor);if(err!=ESP_OK)return err;c->opened=false;}
    return ESP_OK;
}
esp_err_t learning_camera_stop(learning_camera_handle_t c)
{
    esp_err_t err=stop_sensor(c);
    return err;
}
esp_err_t learning_camera_options(learning_camera_handle_t c,bool light,bool flipped)
{
    if(!c||!c->sensor||!c->streaming)return ESP_ERR_INVALID_STATE;
    if(light!=c->light){
        esp_err_t err=light?mosaico_camera_flash_trigger(c->sensor):mosaico_camera_flash_stop(c->sensor);
        if(err!=ESP_OK)return err;
        c->light=light;
    }
    c->flipped=flipped;return ESP_OK;
}
esp_err_t learning_camera_delete(learning_camera_handle_t c)
{
    if(!c)return ESP_OK;
    esp_err_t err=learning_camera_stop(c);if(err!=ESP_OK)return err;
    if(c->canvas_active){
        if(esp_gsp_canvas_stop(c->ui,GSP_LEARN_BIND_PREVIEW)!=ESP_GSP_OK||
           esp_gsp_flush(c->ui,CANVAS_FLUSH_MS)!=ESP_GSP_OK)return ESP_ERR_TIMEOUT;
        for(unsigned i=0;i<PREVIEW_BUFFERS;++i)
            if(atomic_load(&c->preview[i].borrowed))return ESP_ERR_TIMEOUT;
        c->canvas_active=false;
    }
    if(c->sensor){err=mosaico_camera_del(c->sensor);if(err!=ESP_OK)return err;}
    free(c->jpeg);
    for(unsigned i=0;i<PREVIEW_BUFFERS;++i)free(c->preview[i].pixels);
    free(c);return ESP_OK;
}
esp_err_t learning_camera_start(learning_camera_handle_t c)
{
    if(!c)return ESP_ERR_INVALID_ARG;
    c->frame_logged=false;
    free(c->jpeg);c->jpeg=NULL;c->jpeg_size=0;
    c->jpeg=heap_caps_malloc(JPEG_CAP,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!c->jpeg)return ESP_ERR_NO_MEM;
    for(unsigned i=0;i<PREVIEW_BUFFERS;++i){
        if(!c->preview[i].pixels)c->preview[i].pixels=heap_caps_malloc(
            PREVIEW_W*PREVIEW_H*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!c->preview[i].pixels)return ESP_ERR_NO_MEM;
    }
    ESP_LOGI("learning_camera","Starting: PSRAM free=%u largest=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    if(!c->sensor){
        const mosaico_module_mgr_config_t manager=MOSAICO_MODULE_MGR_DEFAULT_CONFIG();
        esp_err_t err=mosaico_module_mgr_init(&manager);if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)return err;
        mosaico_camera_config_t cfg=MOSAICO_CAMERA_DEFAULT_CONFIG();
        // A single borrowed frame is supported by the BSP. Preview is sampled
        // at 5 Hz, so dropping frames during conversion is preferable to holding
        // two 720p buffers beside the UI, font and audio buffers.
        cfg.buffer_count=1;
        // The sensor defaults select a supported format; never force an
        // unsupported width or drive an unidentified expansion board.
        err=mosaico_camera_new(&cfg,&c->sensor);if(err!=ESP_OK)return err;
    }
    if(!c->opened){esp_err_t err=mosaico_camera_open(c->sensor);if(err!=ESP_OK){ESP_LOGE("learning_camera","Open failed: %s",esp_err_to_name(err));return err;}c->opened=true;}
    if(!c->streaming){esp_err_t err=mosaico_camera_start_stream(c->sensor);if(err!=ESP_OK){learning_camera_stop(c);return err;}c->streaming=true;}
    return ESP_OK;
}
static uint8_t clamp(int x){return (uint8_t)(x<0?0:x>255?255:x);}
static void pixel(const mosaico_camera_frame_t *f,unsigned x,unsigned y,uint8_t *out)
{
    size_t stride=f->bytes_per_line?f->bytes_per_line:(size_t)f->width*2;
    const uint8_t *p=(const uint8_t*)f->data+(size_t)y*stride+(x&~1U)*2;
    int u=p[0]-128,v=p[2]-128,l=p[(x&1)?3:1]-16;
    out[0]=clamp((298*l+409*v+128)>>8);
    out[1]=clamp((298*l-100*u-208*v+128)>>8);
    out[2]=clamp((298*l+516*u+128)>>8);
}
static void viewport_pixel(const mosaico_camera_frame_t *f,const learning_viewport_t *v,
    unsigned x,unsigned y,unsigned w,unsigned h,uint8_t *out)
{unsigned sx,sy;learning_viewport_pixel(v,x,y,w,h,&sx,&sy);pixel(f,sx,sy,out);}
static void preview_released(void *ctx)
{
    struct preview_buffer_t *slot=ctx;
    atomic_fetch_add(&slot->releases,1);
    atomic_store(&slot->borrowed,false);
}
static esp_err_t present_preview(learning_camera_handle_t c,const mosaico_camera_frame_t *f,
                                const learning_viewport_t *v)
{
    struct preview_buffer_t *slot=NULL;
    for(unsigned i=0;i<PREVIEW_BUFFERS;++i)
        if(!atomic_load(&c->preview[i].borrowed)){slot=&c->preview[i];break;}
    // Keep the live frame immutable if rendering is slower than acquisition.
    if(!slot){++c->dropped;return ESP_OK;}
    for(unsigned y=0;y<PREVIEW_H;++y){
        for(unsigned x=0;x<PREVIEW_W;++x){
            uint8_t rgb[3];viewport_pixel(f,v,x,y,PREVIEW_W,PREVIEW_H,rgb);
            slot->pixels[(size_t)y*PREVIEW_W+x]=(uint16_t)(
                ((rgb[0]>>3)<<11)|((rgb[1]>>2)<<5)|(rgb[2]>>3));
        }
    }
    atomic_store(&slot->borrowed,true);
    esp_gsp_err_t err=esp_gsp_canvas_push(c->ui,GSP_LEARN_BIND_PREVIEW,
        slot->pixels,PREVIEW_W*sizeof(uint16_t),preview_released,slot);
    if(err!=ESP_GSP_OK){atomic_store(&slot->borrowed,false);return ESP_FAIL;}
    c->canvas_active=true;
    if(++c->submitted%30==0){
        unsigned released=0;
        for(unsigned i=0;i<PREVIEW_BUFFERS;++i)released+=atomic_load(&c->preview[i].releases);
        ESP_LOGI("learning_camera","Live preview: submitted=%u released=%u dropped=%u PSRAM=%u",
            c->submitted,released,c->dropped,(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
    return ESP_OK;
}
// The driver leaves the packed UYVY line stride zero; validate the implied
// packed stride and the actual borrowed buffer before any pixel access.
static esp_err_t validate_frame(const mosaico_camera_frame_t *f)
{
    size_t stride=f->bytes_per_line?f->bytes_per_line:(size_t)f->width*2;
    if(!f->data||!f->width||!f->height||f->width%2||f->pixel_format!=V4L2_PIX_FMT_UYVY||
        stride<(size_t)f->width*2||f->size<stride*f->height){
        ESP_LOGE("learning_camera","Unsupported frame: %ux%u stride=%u format=%08x size=%u",
            (unsigned)f->width,(unsigned)f->height,(unsigned)f->bytes_per_line,
            (unsigned)f->pixel_format,(unsigned)f->size);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return esp_cache_msync((void*)f->data,f->size,
        ESP_CACHE_MSYNC_FLAG_DIR_M2C|ESP_CACHE_MSYNC_FLAG_INVALIDATE);
}
static esp_err_t encode_frame(const mosaico_camera_frame_t *f,const learning_viewport_t *v,unsigned w,unsigned h,
                              uint8_t *out,size_t capacity,size_t *size)
{
    if(!w||!h)return ESP_ERR_INVALID_SIZE;
    jpeg_enc_config_t cfg=DEFAULT_JPEG_ENC_CONFIG();
    cfg.width=w;cfg.height=h;cfg.src_type=JPEG_PIXEL_FORMAT_RGB888;
    cfg.subsampling=JPEG_SUBSAMPLE_420;cfg.quality=85;
    jpeg_enc_handle_t encoder=NULL;
    int opened=jpeg_enc_open(&cfg,&encoder);
    if(opened!=JPEG_ERR_OK){ESP_LOGE("learning_camera","JPEG open failed: %d",opened);return opened==JPEG_ERR_NO_MEM?ESP_ERR_NO_MEM:ESP_FAIL;}
    int block=jpeg_enc_get_block_size(encoder),used=0;
    esp_err_t err=ESP_ERR_NO_MEM;uint8_t *strip=NULL;
    if(block<=0||block%3||(w*h*3)%(unsigned)block){err=ESP_ERR_NOT_SUPPORTED;goto done;}
    strip=heap_caps_aligned_alloc(16,(size_t)block,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!strip)goto done;
    // DVP owns a second full backup frame. Keep only one encoder strip, rather
    // than a full RGB image, beside those two DMA buffers and the rendered UI.
    for(size_t base=0;base<(size_t)w*h;base+=(unsigned)block/3){
        memset(strip,230,(size_t)block);
        for(unsigned i=0;i<(unsigned)block/3;++i){
            size_t index=base+i;unsigned x=index%w,y=index/w;
            viewport_pixel(f,v,x,y,w,h,strip+(size_t)i*3);
        }
        int encoded=jpeg_enc_process_with_block(encoder,strip,block,out,(int)capacity,&used);
        if(encoded<JPEG_ERR_OK){ESP_LOGE("learning_camera","JPEG block failed: code=%d size=%ux%u output=%d capacity=%u",encoded,w,h,used,(unsigned)capacity);err=encoded==JPEG_ERR_NO_MEM?ESP_ERR_NO_MEM:ESP_FAIL;goto done;}
    }
    err=used>0?ESP_OK:ESP_FAIL;
    if(err==ESP_OK)*size=(size_t)used;
done:
    free(strip);jpeg_enc_close(encoder);return err;
}
static esp_err_t frame(learning_camera_handle_t c,bool photo)
{
    if(!c||!c->streaming)return ESP_ERR_INVALID_STATE;
    app_orientation_frame_lock();
    mosaico_camera_frame_t f={0};esp_err_t err=mosaico_camera_get_frame(c->sensor,&f);
    if(err!=ESP_OK){app_orientation_frame_unlock();return err;}
    learning_viewport_t viewport;
    unsigned rotation=app_orientation_get();
    err=validate_frame(&f);
    if(err==ESP_OK&&!learning_viewport_init_facing(&viewport,f.width,f.height,rotation,c->flipped))err=ESP_ERR_INVALID_ARG;
    if(err==ESP_OK)err=present_preview(c,&f,&viewport);
    if(photo){
        unsigned w=LEARN_PHOTO_W,h=LEARN_PHOTO_H;
        if(err==ESP_OK){
            ESP_LOGI("learning_camera","Capture budget: free=%u largest=%u",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
            err=c->jpeg?encode_frame(&f,&viewport,w,h,c->jpeg,JPEG_CAP,&c->jpeg_size):ESP_ERR_NO_MEM;
        }
    }
    esp_err_t returned=mosaico_camera_return_frame(c->sensor,&f);
    if(err==ESP_OK)err=returned;
    if(photo){
        esp_err_t stopped=stop_sensor(c);if(err==ESP_OK)err=stopped;
    }
    app_orientation_frame_unlock();
    if(err==ESP_OK&&(!c->frame_logged||photo)){
        ESP_LOGI("learning_camera","%s ready: %ux%u canvas=%u photo=%u bytes rotation=%u crop=%u,%u %ux%u",
            photo?"Capture":"Preview",(unsigned)f.width,(unsigned)f.height,
            c->submitted,(unsigned)(photo?c->jpeg_size:0),rotation,
            viewport.x,viewport.y,viewport.w,viewport.h);
        c->frame_logged=true;
    }
    if(err!=ESP_OK)ESP_LOGE("learning_camera","%s failed: %s",photo?"Capture":"Preview",esp_err_to_name(err));
    return err;
}
esp_err_t learning_camera_preview(learning_camera_handle_t c){return frame(c,false);}
esp_err_t learning_camera_capture(learning_camera_handle_t c,const void **jpeg,size_t *size)
{
    if(!jpeg||!size)return ESP_ERR_INVALID_ARG;
    *jpeg=NULL;*size=0;
    esp_err_t err=frame(c,true),stopped=learning_camera_stop(c);
    if(err==ESP_OK)err=stopped;
    if(err==ESP_OK){*jpeg=c->jpeg;*size=c->jpeg_size;}return err;
}
