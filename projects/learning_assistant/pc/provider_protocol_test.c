// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Actual provider adapter + real cJSON, isolated from IDF and the internet.
#include "learning_provider.h"
#include "learning_network.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct fake_http_t { char request[32768],url[512],auth[512]; size_t used,offset;
    const char *response; int status; bool complete; };
static struct fake_http_t client;
static const char *response="{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"content\":\"先减去5，再除以3，x=5\"}}]}";
static int status_code=200,downloads;
static int open_result=ESP_OK,tls_failure_code;
static bool is_current=true,incomplete;
static unsigned header_waits,read_waits,header_calls,uploads;
static bool cancel_in_wait,header_failure;
static int64_t fake_time;
void *esp_crt_bundle_attach=(void*)1;
int64_t esp_timer_get_time(void){return fake_time+=1000;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg)
{
    assert(cfg->crt_bundle_attach);assert(cfg->disable_auto_redirect);
    memset(&client,0,sizeof(client));snprintf(client.url,sizeof(client.url),"%s",cfg->url);
    header_calls=0;uploads=0;
    client.response=response;client.status=status_code;
    if(strstr(cfg->url,".oss-cn-beijing.aliyuncs.com")){++downloads;client.response="RIFFfakeaudio";}
    return &client;
}
esp_err_t esp_http_client_set_method(esp_http_client_handle_t c,int method){(void)c;assert(method==HTTP_METHOD_POST);return ESP_OK;}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *key,const char *value)
{if(!strcmp(key,"Authorization"))snprintf(c->auth,sizeof(c->auth),"%s",value);return ESP_OK;}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int size){(void)c;assert(size>=0);return open_result;}
int esp_http_client_write(esp_http_client_handle_t c,const char *data,int size)
{++uploads;assert(c->used+(size_t)size<sizeof(c->request));memcpy(c->request+c->used,data,(size_t)size);c->used+=(size_t)size;return size;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c){
    ++header_calls;
    if(header_failure)return ESP_FAIL;
    if(header_waits){--header_waits;fake_time+=5000000;if(cancel_in_wait)is_current=false;return -ESP_ERR_HTTP_EAGAIN;}
    return (int64_t)strlen(c->response);
}
int esp_http_client_get_status_code(esp_http_client_handle_t c){return c->status;}
int esp_http_client_read(esp_http_client_handle_t c,char *out,int size)
{
    if(read_waits){--read_waits;fake_time+=5000000;return -ESP_ERR_HTTP_EAGAIN;}
    size_t n=strlen(c->response)-c->offset;if(n>(size_t)size)n=(size_t)size;
    memcpy(out,c->response+c->offset,n);c->offset+=n;c->complete=c->offset==strlen(c->response)&&!incomplete;return (int)n;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c){return c->complete;}
esp_err_t esp_http_client_close(esp_http_client_handle_t c){(void)c;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c){(void)c;return ESP_OK;}
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t c,int *code,int *flags)
{(void)c;*code=tls_failure_code;*flags=0;return ESP_OK;}
int mbedtls_base64_encode(unsigned char *out,size_t cap,size_t *used,const unsigned char *data,size_t n)
{
    static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t need=4*((n+2)/3);if(cap<=need)return -1;size_t written=0;
    for(size_t i=0;i<n;i+=3){unsigned x=(unsigned)data[i]<<16;
        if(i+1<n)x|=(unsigned)data[i+1]<<8;if(i+2<n)x|=data[i+2];
        out[written++]=chars[(x>>18)&63];out[written++]=chars[(x>>12)&63];
        out[written++]=i+1<n?chars[(x>>6)&63]:'=';out[written++]=i+2<n?chars[x&63]:'=';
    }out[written]=0;*used=written;return 0;
}
static bool current(void *ctx,uint32_t generation){(void)ctx;return is_current&&generation==12;}
static learning_config_t config={.version=1,.ssid="home",.password="12345678",.key="sk-test-placeholder",.region=0};
static char answer[8192];
static const uint8_t image[]={0,1,2};
static learning_questions_t questions;
static void identify_reply(learning_provider_handle_t p,const char *json,bool succeeds)
{
    const char *old=response;cJSON *root=cJSON_CreateObject(),*choices=cJSON_AddArrayToObject(root,"choices");
    cJSON *choice=cJSON_CreateObject();assert(cJSON_AddItemToArray(choices,choice));
    cJSON_AddStringToObject(choice,"finish_reason","stop");cJSON *message=cJSON_AddObjectToObject(choice,"message");
    cJSON_AddStringToObject(message,"content",json);char *wire=cJSON_PrintUnformatted(root);cJSON_Delete(root);response=wire;
    esp_err_t err=learning_provider_identify(p,&config,12,image,sizeof(image),&questions);
    assert((err==ESP_OK)==succeeds);
    if(!succeeds)assert(!questions.count);
    free(wire);response=old;
}
int main(void)
{
    assert(learning_config_valid(&config));
    int probe_status=0;status_code=401;
    assert(learning_provider_probe(0,&probe_status)==ESP_OK&&probe_status==401);
    assert(!client.auth[0]&&!client.request[0]);
    assert(strstr(client.url,"https://dashscope.aliyuncs.com/"));
    assert(learning_provider_probe(1,&probe_status)==ESP_OK&&strstr(client.url,"https://dashscope-intl.aliyuncs.com/"));
    assert(learning_provider_probe(2,&probe_status)==ESP_ERR_INVALID_ARG);status_code=200;
    open_result=ESP_FAIL;tls_failure_code=-135;
    assert(learning_provider_probe(0,&probe_status)==ESP_FAIL&&probe_status==0);
    assert(learning_provider_test(&config,answer,sizeof(answer))==ESP_FAIL);
    assert(strstr(answer,"安全连接失败")&&!strstr(answer,config.key));
    open_result=ESP_OK;tls_failure_code=0;
    strcpy(config.key,"sk-format.with+punctuation=");assert(learning_config_valid(&config));strcpy(config.key,"sk-test-placeholder");
    config.region=2;assert(!learning_config_valid(&config));config.region=0;
    strcpy(config.key,"sk-test\r\nInjected");assert(!learning_config_valid(&config));strcpy(config.key,"sk-test-placeholder");
    memset(config.ssid,'x',sizeof(config.ssid));assert(!learning_config_valid(&config));strcpy(config.ssid,"home");
    learning_provider_handle_t provider=NULL;assert(learning_provider_create(current,NULL,&provider)==ESP_OK);
    assert(learning_provider_answer(provider,&config,12,"请讲这题",image,sizeof(image),"上一题的上下文",answer,sizeof(answer))==ESP_OK);
    assert(strstr(answer,"x=5"));assert(!strcmp(client.auth,"Bearer sk-test-placeholder"));
    assert(!strcmp(client.url,"https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"));
    cJSON *root=cJSON_Parse(client.request);assert(root);
    assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(root,"model")),"qwen3-vl-plus"));
    cJSON *messages=cJSON_GetObjectItem(root,"messages");assert(cJSON_GetArraySize(messages)==4);
    assert(strstr(cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(messages,2),"content")),"上下文"));
    cJSON *content=cJSON_GetObjectItem(cJSON_GetArrayItem(messages,3),"content");
    assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(content,1),"image_url"),"url")),"data:image/jpeg;base64,AAEC"));cJSON_Delete(root);
    assert(!learning_provider_set_grade(provider,0)&&!learning_provider_set_grade(provider,13));
    assert(learning_provider_set_grade(provider,12));
    identify_reply(provider,"{\"overflow\":false,\"questions\":[{\"number\":\"第8题\",\"summary\":\"解方程3x+5=20\",\"location\":\"左上\"},{\"number\":\"\",\"summary\":\"求角度\",\"location\":\"右下\"}]}",true);
    assert(questions.count==2&&!strcmp(questions.items[0].number,"第8题")&&strstr(questions.items[1].number,"无题号"));
    assert(strstr(client.request,"索引器")&&!strstr(client.request,"按该年级知识水平讲解"));
    assert(strstr(client.request,"data:image/jpeg;base64,AAEC"));
    identify_reply(provider,"{\"overflow\":false,\"questions\":[{\"number\":\"第8题\",\"summary\":\"左题\",\"location\":\"左上\"},{\"number\":\"第8题\",\"summary\":\"右题\",\"location\":\"右上\"}]}",true);
    identify_reply(provider,"{\"overflow\":false,\"questions\":[{\"number\":\"第8题（1）\",\"summary\":\"化简\",\"location\":\"左上\"}]}",true);
    assert(questions.count==1&&strstr(questions.items[0].number,"（1）"));
    identify_reply(provider,"{\"overflow\":false,\"questions\":[]}",false);
    identify_reply(provider,"{\"overflow\":true,\"questions\":[]}",false);
    identify_reply(provider,"{\"questions\":[]}",false);
    identify_reply(provider,"```json\n{}\n```",false);
    identify_reply(provider,"{\"overflow\":false,\"questions\":[{\"number\":8,\"summary\":\"化简\",\"location\":\"左上\"}]}",false);
    identify_reply(provider,"{\"overflow\":false,\"questions\":[{\"number\":\"第8题\",\"summary\":\" \"}]}",false);
    identify_reply(provider,"{\"overflow\":false,\"questions\":[{\"number\":\"8\",\"summary\":\"题干\\n换行\",\"location\":\"左上\"}]}",false);
    identify_reply(provider,"{\"overflow\":false,\"questions\":[{\"number\":\"8\",\"summary\":\"题干\",\"location\":\"左上\"},{\"number\":\"8\",\"summary\":\"题干\",\"location\":\"左上\"}]}",false);
    {
        char *json=calloc(1,12000);assert(json);strcpy(json,"{\"overflow\":false,\"questions\":[");
        for(unsigned i=0;i<25;++i){char item[192];snprintf(item,sizeof(item),"%s{\"number\":\"%u\",\"summary\":\"test\",\"location\":\"row%u\"}",i?",":"",i+1,i+1);strcat(json,item);}
        strcat(json,"]}");identify_reply(provider,json,false);free(json);
    }
    identify_reply(provider,"{\"overflow\":false,\"questions\":[{\"number\":\"12345678901234567\",\"summary\":\"test\",\"location\":\"top\"}]}",false);
    assert(learning_provider_answer(provider,&config,12,"测试年级",NULL,0,NULL,answer,sizeof(answer))==ESP_OK);
    assert(strstr(client.request,"高三")&&!strstr(client.request,"初一"));
    // A model that takes 15 seconds to return headers, then pauses its body,
    // succeeds without retransmitting the image or losing response bytes.
    header_waits=3;read_waits=2;
    assert(learning_provider_answer(provider,&config,12,"慢响应",image,sizeof(image),NULL,answer,sizeof(answer))==ESP_OK);
    assert(header_calls==4&&uploads==1&&strstr(answer,"x=5"));
    header_waits=100;
    assert(learning_provider_answer(provider,&config,12,"超时",image,sizeof(image),NULL,answer,sizeof(answer))!=ESP_OK);
    assert(header_calls<=18&&uploads==1&&strstr(learning_provider_error(provider),"模型响应超时"));
    header_waits=3;cancel_in_wait=true;
    assert(learning_provider_answer(provider,&config,12,"取消",image,sizeof(image),NULL,answer,sizeof(answer))!=ESP_OK);
    assert(header_calls==1&&uploads==1);cancel_in_wait=false;is_current=true;header_waits=0;
    header_failure=true;
    assert(learning_provider_answer(provider,&config,12,"断线",image,sizeof(image),NULL,answer,sizeof(answer))!=ESP_OK);
    assert(header_calls==1);header_failure=false;
    config.region=1;
    assert(learning_provider_transcribe(provider,&config,12,image,sizeof(image),answer,sizeof(answer))==ESP_OK);
    assert(strstr(client.url,"dashscope-intl.aliyuncs.com"));assert(strstr(client.request,"data:audio/wav;base64,AAEC"));assert(strstr(client.request,"qwen3-asr-flash"));
    for(unsigned i=0;i<4;++i){status_code=(int[]){401,403,404,429}[i];
        assert(learning_provider_answer(provider,&config,12,"问题",NULL,0,NULL,answer,sizeof(answer))!=ESP_OK);
        assert(!strstr(learning_provider_error(provider),config.key));}
    status_code=200;response="invalid JSON";
    assert(learning_provider_answer(provider,&config,12,"问题",NULL,0,NULL,answer,sizeof(answer))!=ESP_OK);
    response="{\"choices\":[{\"finish_reason\":\"length\",\"message\":{\"content\":\"partial\"}}]}";
    assert(learning_provider_answer(provider,&config,12,"问题",NULL,0,NULL,answer,sizeof(answer))!=ESP_OK);
    response="{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"content\":\"内容\"}}]}";
    incomplete=true;assert(learning_provider_answer(provider,&config,12,"问题",NULL,0,NULL,answer,sizeof(answer))!=ESP_OK);incomplete=false;
    is_current=false;assert(learning_provider_answer(provider,&config,12,"问题",NULL,0,NULL,answer,sizeof(answer))!=ESP_OK);is_current=true;
    assert(learning_provider_answer(provider,&config,12,"问题",NULL,0,NULL,answer,2)!=ESP_OK);
    uint8_t *wav=NULL;size_t size=0;
    response="{\"output\":{\"audio\":{\"url\":\"http://test.oss-cn-beijing.aliyuncs.com/output.wav\"}}}";
    assert(learning_provider_audio(provider,&config,12,"你好",&wav,&size)==ESP_OK);
    assert(downloads==1&&size==13&&wav);assert(!client.auth[0]);assert(!strncmp(client.url,"https://",8));free(wav);
    response="{\"output\":{\"audio\":{\"url\":\"https://example.com/audio.wav\"}}}";
    assert(learning_provider_audio(provider,&config,12,"你好",&wav,&size)!=ESP_OK);assert(downloads==1);
    learning_provider_delete(provider);
    puts("PASS: config validation, image/audio request JSON, region, context, auth, HTTP errors, malformed/incomplete/oversized answers, cancellation and credential-free HTTPS audio download");return 0;
}
