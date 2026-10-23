// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "gsp_sim_bridge.h"
#include "learning_ui.h"
#include "learning_platform.h"
#define GSP_BUNDLE_ENABLE_RAW_IDS
#include "bundle_gsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Explicit PC fixtures. No network, microphone or device credentials are used.
static learning_ui_handle_t app;
static esp_gsp_handle_t app_ui;
static learning_message_t pending;
static bool available = true;
static unsigned polls;
static int connection_mode=2;
static unsigned record_requests,transcribe_requests,answer_requests,speech_requests,identify_requests;
static learning_controls_t controls;
static bool controls_ready;
void learning_platform_controls_get(learning_controls_t *out)
{if(!controls_ready){controls=learning_controls_defaults();controls_ready=true;}*out=controls;}
bool learning_platform_controls_set(learning_control_t c,int value)
{learning_controls_t v;learning_platform_controls_get(&v);bool ok=learning_controls_change(&v,c,value,0);if(ok)controls=v;return ok;}
bool learning_platform_simulated(void) { return true; }
bool learning_platform_key(void) {
    const char *path=getenv("LEARNING_SIM_KEY");if(!path)return false;
    FILE *f=fopen(path,"rb");if(!f)return false;int c=fgetc(f);fclose(f);return c=='1';
}
void learning_platform_presented(const learning_snapshot_t *s) {
    const char *path=getenv("LEARNING_SIM_TRACE");if(!path)return;
    FILE *f=fopen(path,"wb");if(!f)return;
    learning_controls_t v;learning_platform_controls_get(&v);bool drawer=false;
    drawer=learning_ui_quick_is_open(app);
    fprintf(f,"{\"page\":%u,\"generation\":%u,\"page_index\":%u,\"page_count\":%u,\"recording\":%s,\"muted\":%s,\"brightness\":%u,\"volume\":%u,\"automatic\":%s,\"drawer\":%s,\"online\":%s,\"api_state\":%u,\"review_index\":%u,\"review_count\":%u,\"record_requests\":%u,\"transcribe_requests\":%u,\"answer_requests\":%u,\"speech_requests\":%u,\"grade\":%u,\"light\":%s,\"flipped\":%s,\"question_count\":%u,\"question_page\":%u,\"selected_question\":%u,\"identify_requests\":%u}\n",
        (unsigned)s->page,(unsigned)s->generation,s->page_index,s->page_count,s->recording?"true":"false",s->muted?"true":"false",v.brightness,v.volume,v.automatic?"true":"false",drawer?"true":"false",s->online?"true":"false",(unsigned)s->api_state,s->review_index,s->review_count,record_requests,transcribe_requests,answer_requests,speech_requests,v.grade,s->light?"true":"false",s->flipped?"true":"false",s->question_count,s->question_page,s->selected_question,identify_requests);fclose(f);
}
bool learning_platform_submit(const learning_command_t *c)
{
    learning_platform_controls_set(LEARN_CONTROL_SPEECH,c->muted?0:1);
    if(c->kind==LEARN_CMD_MUTE)return true;
    memset(&pending, 0, sizeof(pending)); pending.generation = c->generation;
    if(c->kind==LEARN_CMD_RECORD)++record_requests;
    if(c->kind==LEARN_CMD_SPEAK)++speech_requests;
    if(c->kind==LEARN_CMD_SEND_AUDIO)++transcribe_requests;
    if(c->kind==LEARN_CMD_SEND_TEXT||c->kind==LEARN_CMD_SOLVE||c->kind==LEARN_CMD_EXPLAIN)++answer_requests;
    switch(c->kind) {
    case LEARN_CMD_CAMERA_OPTIONS:
        if(getenv("LEARNING_SIM_FAILURE")){
            FILE *f=fopen(getenv("LEARNING_SIM_FAILURE"),"rb");
            if(f){int active=fgetc(f);fclose(f);if(active=='2'){
                pending.kind=LEARN_MSG_ERROR;snprintf(pending.text,sizeof(pending.text),"补光或翻转失败，请重试");break;
            }}
        }
        pending.kind=LEARN_MSG_CAMERA_OPTIONS;pending.light=c->light;pending.flipped=c->flipped;break;
    case LEARN_CMD_RECORD:available=false;return true;
    case LEARN_CMD_SEND_AUDIO:
        pending.kind=LEARN_MSG_TRANSCRIPT;
        snprintf(pending.text,sizeof(pending.text),"这道方程为什么要先除以三？请结合原题，解释等式两边为什么要做同样的运算。识别文字可以翻页查看，必须点击确认发送才会问模型。\n先确认第一步运算。\n再核对第二步。\n检查是否有识别错误。\n确认全部文字后发送。");break;
    case LEARN_CMD_SEND_TEXT:
        pending.kind=LEARN_MSG_ANSWER;
        snprintf(pending.text,sizeof(pending.text),"【模拟语音回答】\n为什么移项要变号？\n这是把等式两边同时减去\n同一个数后，简写的结果。\n例如：x + 5 = 8\n两边同时减去 5，得到\nx = 8 - 5，也就是 x = 3。");break;
    case LEARN_CMD_CAMERA:
        if(getenv("LEARNING_SIM_FAILURE")){
            FILE *failure=fopen(getenv("LEARNING_SIM_FAILURE"),"rb");
            if(failure){int active=fgetc(failure);fclose(failure);if(active=='1'){
                pending.kind=LEARN_MSG_ERROR;snprintf(pending.text,sizeof(pending.text),"模拟相机错误，请检查左侧插件后重试");break;}}
        }
        {
            FILE *f=fopen(LEARNING_SIM_PHOTO,"rb");
            if(f){fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
                if(n>0&&n<65536){void *data=malloc((size_t)n);if(data){
                    if(fread(data,1,(size_t)n,f)==(size_t)n)esp_gsp_set_image(app_ui,GSP_LEARN_BIND_PREVIEW,data,(size_t)n);
                    free(data);}}
                fclose(f);}
        }
        pending.kind = LEARN_MSG_STATUS; pending.online = pending.configured = true; pending.api_state=LEARN_API_READY;
        snprintf(pending.text, sizeof(pending.text), "模拟相机 · 点击拍照查看演示题"); break;
    case LEARN_CMD_CAPTURE: pending.kind = LEARN_MSG_PHOTO; break;
    case LEARN_CMD_IDENTIFY: {
        ++identify_requests;pending.kind=LEARN_MSG_QUESTIONS;
        int mode=0;const char *path=getenv("LEARNING_SIM_QUESTIONS");
        if(path){FILE *f=fopen(path,"rb");if(f){mode=fgetc(f)-'0';fclose(f);}}
        if(mode==2){pending.kind=LEARN_MSG_ERROR;snprintf(pending.text,sizeof(pending.text),"没有识别到清晰完整的题目，请靠近重拍");break;}
        pending.questions.count=mode==1?1:6;
        for(unsigned i=0;i<pending.questions.count;++i){learning_question_t *q=&pending.questions.items[i];
            snprintf(q->number,sizeof(q->number),"第%u题",8+i);
            snprintf(q->summary,sizeof(q->summary),"%s",i==0?"解方程：3x + 5 = 20":i==1?"如图，已知 AB 平行 CD，求角度":"先化简，再求代数式的值");
            snprintf(q->location,sizeof(q->location),"第%u行，左列",i+1);
        }
        break;
    }
    case LEARN_CMD_SOLVE:
        pending.kind = LEARN_MSG_ANSWER;
        snprintf(pending.text, sizeof(pending.text),
            "【模拟题目与回答】\n解方程：3x + 5 = 20\n\n第 1 步：两边同时减去 5\n3x = 20 - 5\n3x = 15\n"
            "第 2 步：两边同时除以 3\nx = 15 ÷ 3\nx = 5\n\n第 3 步：代入检验\n3 × 5 + 5 = 20\n等式成立，所以 x = 5。\n"
            "想一想：为什么等式两边\n要做相同的运算？"); break;
    case LEARN_CMD_EXPLAIN:
        pending.kind = LEARN_MSG_ANSWER;
        snprintf(pending.text, sizeof(pending.text), "【模拟追问回答】\n把等式想成一架天平。\n两边同时拿走相同的重量，\n天平仍然保持平衡。\n所以，两边同时减去 5，\n等式仍然成立。\n这就是这一页的原因。"); break;
    case LEARN_CMD_SETUP:
        pending.kind = LEARN_MSG_STATUS; pending.online = pending.configured = true; pending.api_state=LEARN_API_READY;
        snprintf(pending.text, sizeof(pending.text), "模拟预览没有热点 · 真机将显示连接信息"); break;
    case LEARN_CMD_SPEAK:
        pending.kind = LEARN_MSG_SPEECH_DONE;
        snprintf(pending.text, sizeof(pending.text), "模拟预览没有播放声音"); break;
    case LEARN_CMD_CANCEL: case LEARN_CMD_MUTE: case LEARN_CMD_NONE:
        available = false; return true;
    default:
        pending.kind = LEARN_MSG_ERROR; snprintf(pending.text, sizeof(pending.text), "语音输入需要真机 AI 键与麦克风"); break;
    }
    available = true; polls = 0; return true;
}
bool learning_platform_poll(learning_message_t *out)
{
    const char *connection=getenv("LEARNING_SIM_CONNECTION");
    if(connection){FILE *f=fopen(connection,"rb");if(f){int mode=fgetc(f)-'0';fclose(f);
        if(mode>=0&&mode<=3&&mode!=connection_mode){connection_mode=mode;
            memset(out,0,sizeof(*out));out->kind=LEARN_MSG_STATUS;out->configured=true;out->online=mode!=0;
            out->api_state=mode==2?LEARN_API_READY:mode==3?LEARN_API_ERROR:LEARN_API_UNKNOWN;return true;
        }
    }}
    if (!available || ++polls < 4) return false;
    *out = pending; available = false; return true;
}
esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    app_ui=ui;
    pending.kind = LEARN_MSG_STATUS; pending.online = pending.configured = true; pending.api_state=LEARN_API_READY;
    snprintf(pending.text, sizeof(pending.text), "模拟预览 · 不连接相机或云端");
    return learning_ui_create(ui, &app);
}
void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{ (void)ui; learning_ui_delete(app); fprintf(stderr, "learning_backend: stopped\n"); }
