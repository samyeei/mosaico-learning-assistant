// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_network.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_sntp.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <net/if.h>

enum { BODY_CAP = 2048, CONNECT_WAIT_MS = 25000, HTTP_STACK = 6144, SCAN_LIMIT = 20 };
// App-lifetime network singleton. Boot config is immutable. HTTP owns body;
// test worker owns candidate after saving=true; status text has a mutex.
struct network_t {
    learning_config_t config, candidate;
    bool configured, ap_enabled, save_after_test;
    atomic_bool online, saving;
    char ssid[32], password[17], nonce[33], body[BODY_CAP+1], result[192];
    SemaphoreHandle_t lock;
    esp_netif_t *sta, *ap;
    httpd_handle_t server;
    wifi_config_t wifi;
    struct ifreq ap_interface;
    unsigned reconnects;
    esp_event_handler_instance_t wifi_event, ip_event;
};
static struct network_t *net;
extern const char setup_html_start[] asm("_binary_setup_html_start");
extern const char setup_html_end[] asm("_binary_setup_html_end");
static void result(struct network_t *s, const char *text)
{ xSemaphoreTake(s->lock, portMAX_DELAY); snprintf(s->result,sizeof(s->result),"%s",text); xSemaphoreGive(s->lock); }
static void wifi_event(void *ctx, esp_event_base_t base, int32_t id, void *data)
{
    (void)data; struct network_t *s = ctx;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) { atomic_store(&s->online, true); s->reconnects=0; }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        atomic_store(&s->online, false);
        if(s->configured&&!atomic_load(&s->saving)&&s->reconnects++<5)esp_wifi_connect();
    }
}
static esp_err_t join(struct network_t *s, const learning_config_t *c)
{
    atomic_store(&s->online, false); esp_wifi_disconnect();
    memset(&s->wifi,0,sizeof(s->wifi));
    memcpy(s->wifi.sta.ssid,c->ssid,strlen(c->ssid));
    memcpy(s->wifi.sta.password,c->password,strlen(c->password));
    s->wifi.sta.pmf_cfg.capable = true;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA,&s->wifi);
    return err == ESP_OK ? esp_wifi_connect() : err;
}
static esp_err_t respond(httpd_req_t *req, const char *status, const char *body)
{
    httpd_resp_set_status(req,status); httpd_resp_set_type(req,"application/json; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
    return httpd_resp_send(req,body,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t home(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; frame-ancestors 'none'");
    return httpd_resp_send(req,setup_html_start,setup_html_end-setup_html_start-1);
}
static esp_err_t session(httpd_req_t *req)
{
    struct network_t *s = req->user_ctx; char reply[64];
    snprintf(reply,sizeof(reply),"{\"nonce\":\"%s\"}",s->nonce);
    return respond(req,"200 OK",reply);
}
static esp_err_t status(httpd_req_t *req)
{
    struct network_t *s = req->user_ctx; cJSON *root=cJSON_CreateObject();
    if(!root) return respond(req,"503 Service Unavailable","{\"error\":\"memory\"}");
    xSemaphoreTake(s->lock,portMAX_DELAY);
    bool ok = cJSON_AddStringToObject(root,"message",s->result) &&
        cJSON_AddBoolToObject(root,"busy",atomic_load(&s->saving)) &&
        cJSON_AddBoolToObject(root,"online",atomic_load(&s->online));
    xSemaphoreGive(s->lock);
    char *json=ok?cJSON_PrintUnformatted(root):NULL; cJSON_Delete(root);
    if(!json)return respond(req,"503 Service Unavailable","{\"error\":\"memory\"}");
    esp_err_t err=respond(req,"200 OK",json); free(json); return err;
}
static esp_err_t store(struct network_t *s)
{
    nvs_handle_t h; esp_err_t err=nvs_open("learning",NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=nvs_set_blob(h,"config",&s->candidate,sizeof(s->candidate));
    if(err==ESP_OK)err=nvs_commit(h);
    nvs_close(h); return err;
}
static esp_err_t scan_wifi(httpd_req_t *req)
{
    struct network_t *s=req->user_ctx;
    if(atomic_load(&s->saving))return respond(req,"409 Conflict","{\"error\":\"正在测试连接，请稍后扫描\"}");
    wifi_ap_record_t *records=calloc(SCAN_LIMIT,sizeof(*records));
    if(!records)return respond(req,"503 Service Unavailable","{\"error\":\"内存不足，请稍后重试\"}");
    wifi_scan_config_t scan={.show_hidden=false};
    scan.scan_time.active.min=30;scan.scan_time.active.max=80;
    esp_err_t err=esp_wifi_scan_start(&scan,true);
    uint16_t count=SCAN_LIMIT;
    if(err==ESP_OK)err=esp_wifi_scan_get_ap_records(&count,records);
    if(err!=ESP_OK){esp_wifi_clear_ap_list();free(records);return respond(req,"503 Service Unavailable","{\"error\":\"扫描失败，请重试或手动输入网络名称\"}");}
    cJSON *root=cJSON_CreateObject(),*list=root?cJSON_AddArrayToObject(root,"networks"):NULL;
    bool ok=list!=NULL;
    for(unsigned i=0;ok&&i<count;++i){
        records[i].ssid[sizeof(records[i].ssid)-1]=0;
        if(!records[i].ssid[0])continue;
        bool duplicate=false;
        for(unsigned j=0;j<i;++j)if(!strcmp((char*)records[j].ssid,(char*)records[i].ssid)){duplicate=true;break;}
        if(duplicate)continue;
        cJSON *item=cJSON_CreateObject();
        if(!item){ok=false;break;}
        ok=cJSON_AddStringToObject(item,"ssid",(char*)records[i].ssid)&&
            cJSON_AddNumberToObject(item,"rssi",records[i].rssi)&&
            cJSON_AddBoolToObject(item,"secure",records[i].authmode!=WIFI_AUTH_OPEN);
        if(!ok||!cJSON_AddItemToArray(list,item)){cJSON_Delete(item);ok=false;break;}
    }
    free(records);
    char *json=ok?cJSON_PrintUnformatted(root):NULL;cJSON_Delete(root);
    if(!json)return respond(req,"503 Service Unavailable","{\"error\":\"无法读取扫描结果，请重试\"}");
    err=respond(req,"200 OK",json);free(json);return err;
}
static void verify(void *ctx)
{
    struct network_t *s=ctx; esp_err_t err=join(s,&s->candidate);
    result(s,"正在连接家庭 Wi-Fi…");
    for(unsigned elapsed=0;err==ESP_OK&&!atomic_load(&s->online)&&elapsed<CONNECT_WAIT_MS;elapsed+=100)
        vTaskDelay(pdMS_TO_TICKS(100));
    if(err==ESP_OK&&!atomic_load(&s->online)){err=ESP_ERR_TIMEOUT;result(s,"Wi-Fi 未连上，请检查网络名称、密码和信号");}
    else if(err!=ESP_OK) result(s,"无法启动 Wi-Fi 连接，请重新提交");
    if(err==ESP_OK){
        result(s,"Wi-Fi 已连接，正在同步时间并测试模型…");
        // Certificate validation needs a sane wall clock. Never disable it.
        for(unsigned i=0;time(NULL)<1700000000 && i<100;++i)vTaskDelay(pdMS_TO_TICKS(100));
        char *error=calloc(1,192);
        if(!error){err=ESP_ERR_NO_MEM;result(s,"设备内存不足，请稍后重试");}
        else { err=learning_provider_test(&s->candidate,error,192); if(err!=ESP_OK)result(s,error); free(error); }
    }
    if(err==ESP_OK&&s->save_after_test){
        err=store(s);
        if(err==ESP_OK){result(s,"配置和模型测试成功，设备即将重新启动；手机可切回家庭 Wi-Fi");vTaskDelay(pdMS_TO_TICKS(3500));esp_restart();}
        else result(s,"测试成功但保存失败，旧配置保留，请重试");
    }
    if(err==ESP_OK&&!s->save_after_test)result(s,"Wi-Fi 和 Token 测试通过，可以点击保存；尚未保存配置");
    memset(&s->candidate,0,sizeof(s->candidate));
    if(s->configured)join(s,&s->config);
    atomic_store(&s->saving,false);vTaskDelete(NULL);
}
static bool copy_field(cJSON *root,const char *name,char *out,size_t cap)
{
    const char *v=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root,name));
    if(!v||strlen(v)>=cap)return false;
    memcpy(out,v,strlen(v)+1); return true;
}
static esp_err_t configure(httpd_req_t *req)
{
    struct network_t *s=req->user_ctx; char nonce[40],type[48];
    if(atomic_load(&s->saving))return respond(req,"409 Conflict","{\"error\":\"正在测试，请稍候\"}");
    if(httpd_req_get_hdr_value_str(req,"X-Setup-Nonce",nonce,sizeof(nonce))!=ESP_OK||strcmp(nonce,s->nonce))
        return respond(req,"403 Forbidden","{\"error\":\"配置会话已失效，请刷新页面\"}");
    if(httpd_req_get_hdr_value_str(req,"Content-Type",type,sizeof(type))!=ESP_OK||
        strcmp(type,"application/json")||!req->content_len||req->content_len>BODY_CAP)
        return respond(req,"400 Bad Request","{\"error\":\"请求格式不正确\"}");
    size_t used=0;
    while(used<req->content_len){int n=httpd_req_recv(req,s->body+used,req->content_len-used);
        if(n<=0){memset(s->body,0,sizeof(s->body));return respond(req,"408 Request Timeout","{\"error\":\"接收超时，请重试\"}");} used+=(size_t)n;}
    s->body[used]=0; cJSON *root=cJSON_ParseWithLengthOpts(s->body,used+1,NULL,true);
    memset(&s->candidate,0,sizeof(s->candidate));s->candidate.version=1;
    cJSON *region=cJSON_GetObjectItemCaseSensitive(root,"region");
    bool valid=cJSON_IsObject(root)&&cJSON_IsNumber(region)&&region->valuedouble==region->valueint&&region->valueint>=0&&region->valueint<=1&&
        copy_field(root,"ssid",s->candidate.ssid,sizeof(s->candidate.ssid))&&
        copy_field(root,"password",s->candidate.password,sizeof(s->candidate.password))&&
        copy_field(root,"key",s->candidate.key,sizeof(s->candidate.key));
    if(valid)s->candidate.region=(unsigned)region->valueint;
    valid=valid&&learning_config_valid(&s->candidate);
    cJSON_Delete(root);memset(s->body,0,sizeof(s->body));
    if(!valid){memset(&s->candidate,0,sizeof(s->candidate));return respond(req,"400 Bad Request","{\"error\":\"请检查 Wi-Fi、密码和 sk- 开头的密钥\"}");}
    s->save_after_test=strcmp(req->uri,"/api/config")==0;
    atomic_store(&s->saving,true);result(s,"正在测试 Wi-Fi 和 Token，请保持连接设备热点");
    if(xTaskCreate(verify,"learn_config",HTTP_STACK,s,3,NULL)!=pdPASS){
        atomic_store(&s->saving,false);memset(&s->candidate,0,sizeof(s->candidate));
        return respond(req,"503 Service Unavailable","{\"error\":\"设备忙，请重试\"}");}
    return respond(req,"202 Accepted","{\"accepted\":true}");
}
esp_err_t learning_network_setup(void)
{
    if(!net)return ESP_ERR_INVALID_STATE;
    if(net->server)return ESP_OK;
    if(!net->ap){net->ap=esp_netif_create_default_wifi_ap();if(!net->ap)return ESP_ERR_NO_MEM;}
    esp_err_t err=esp_wifi_set_mode(WIFI_MODE_APSTA);if(err!=ESP_OK)return err;
    memset(&net->wifi,0,sizeof(net->wifi));
    memcpy(net->wifi.ap.ssid,net->ssid,strlen(net->ssid));net->wifi.ap.ssid_len=strlen(net->ssid);
    net->wifi.ap.authmode=WIFI_AUTH_OPEN;net->wifi.ap.channel=1;net->wifi.ap.max_connection=1;
    err=esp_wifi_set_config(WIFI_IF_AP,&net->wifi);if(err!=ESP_OK)return err;
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=HTTP_STACK;cfg.max_open_sockets=3;
    cfg.lru_purge_enable=true;cfg.recv_wait_timeout=5;cfg.send_wait_timeout=5;
    err=esp_netif_get_netif_impl_name(net->ap,net->ap_interface.ifr_name);if(err!=ESP_OK)return err;
    cfg.if_name=&net->ap_interface;
    err=httpd_start(&net->server,&cfg);if(err!=ESP_OK)return err;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=home,.user_ctx=net},
        {.uri="/api/session",.method=HTTP_GET,.handler=session,.user_ctx=net},
        {.uri="/api/status",.method=HTTP_GET,.handler=status,.user_ctx=net},
        {.uri="/api/wifi",.method=HTTP_GET,.handler=scan_wifi,.user_ctx=net},
        {.uri="/api/test",.method=HTTP_POST,.handler=configure,.user_ctx=net},
        {.uri="/api/config",.method=HTTP_POST,.handler=configure,.user_ctx=net}};
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i){err=httpd_register_uri_handler(net->server,&routes[i]);if(err!=ESP_OK){httpd_stop(net->server);net->server=NULL;return err;}}
    net->ap_enabled=true;return ESP_OK;
}
esp_err_t learning_network_start(void)
{
    if(net)return ESP_ERR_INVALID_STATE;
    struct network_t *s=calloc(1,sizeof(*s));if(!s)return ESP_ERR_NO_MEM;
    s->lock=xSemaphoreCreateMutex();if(!s->lock){free(s);return ESP_ERR_NO_MEM;}
    atomic_init(&s->online,false);atomic_init(&s->saving,false);
    nvs_handle_t h;size_t size=sizeof(s->config);
    if(nvs_open("learning",NVS_READONLY,&h)==ESP_OK){
        s->configured=nvs_get_blob(h,"config",&s->config,&size)==ESP_OK&&size==sizeof(s->config)&&learning_config_valid(&s->config);nvs_close(h);}
    uint8_t mac[6];esp_err_t err=esp_read_mac(mac,ESP_MAC_WIFI_STA);if(err!=ESP_OK)goto fail;
    snprintf(s->ssid,sizeof(s->ssid),"Learn-Setup-%02X%02X",mac[4],mac[5]);
    uint8_t random[16];esp_fill_random(random,sizeof(random));
    for(unsigned i=0;i<16;++i)snprintf(s->nonce+2*i,3,"%02x",random[i]);
    err=esp_netif_init();if(err!=ESP_OK)goto fail;
    err=esp_event_loop_create_default();if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)goto fail;
    s->sta=esp_netif_create_default_wifi_sta();if(!s->sta){err=ESP_ERR_NO_MEM;goto fail;}
    // Boot-only initialization aggregate fits the main task's 20 KiB budget.
    const wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    err=esp_wifi_init(&init);if(err!=ESP_OK)goto fail;
    err=esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,s,&s->wifi_event);if(err!=ESP_OK)goto fail;
    err=esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,s,&s->ip_event);if(err!=ESP_OK)goto fail;
    err=esp_wifi_set_storage(WIFI_STORAGE_RAM);if(err!=ESP_OK)goto fail;
    err=esp_wifi_set_mode(WIFI_MODE_STA);if(err!=ESP_OK)goto fail;
    err=esp_wifi_start();if(err!=ESP_OK)goto fail;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"ntp.aliyun.com");esp_sntp_init();
    net=s;result(s,"填写 Wi-Fi 和 Qwen API Key 后测试连接");
    if(s->configured)return join(s,&s->config);
    return learning_network_setup();
fail:
    // No other app owns this process' Wi-Fi. Roll back registered callbacks
    // before freeing context; default loop itself is a shared SDK service.
    if(s->ip_event)esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,s->ip_event);
    if(s->wifi_event)esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,s->wifi_event);
    esp_wifi_stop();esp_wifi_deinit();
    if(s->sta)esp_netif_destroy_default_wifi(s->sta);
    vSemaphoreDelete(s->lock);memset(s,0,sizeof(*s));free(s);return err;
}
bool learning_network_online(void){return net&&atomic_load(&net->online);}
bool learning_network_configured(void){return net&&net->configured;}
const learning_config_t *learning_network_config(void){return net&&net->configured?&net->config:NULL;}
bool learning_network_setup_info(const char **ssid,const char **password)
{if(!net||!net->ap_enabled)return false;if(ssid)*ssid=net->ssid;if(password)*password=net->password;return true;}
