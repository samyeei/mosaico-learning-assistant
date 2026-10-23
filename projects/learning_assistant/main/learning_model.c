// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MIN_RECORD_MS = 300, LINE_COLUMNS = 18 };
struct learning_model_t {
    learning_snapshot_t view;
    bool simulated, key_held,conversation,awaiting_transcript;
    learning_page_t return_page;
    learning_action_t retry;
    learning_questions_t questions;
    bool identifying,solving;
    char answer[LEARN_TEXT_CAP];
    size_t offsets[LEARN_MAX_PAGES + 1],review_offsets[LEARN_MAX_PAGES + 1];
};
static bool valid_utf8(const char *text,size_t capacity)
{
    const unsigned char *p=(const unsigned char*)text;
    const char *end=memchr(text,0,capacity);if(!end)return false;
    while((const char*)p<end){
        unsigned c=*p++;if(c<0x80)continue;
        unsigned n=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;
        if(!n||p+n>(const unsigned char*)end)return false;
        if((c==0xe0&&p[0]<0xa0)||(c==0xed&&p[0]>=0xa0)||
            (c==0xf0&&p[0]<0x90)||(c==0xf4&&p[0]>=0x90))return false;
        for(unsigned i=0;i<n;++i)if((p[i]&0xc0)!=0x80)return false;
        p+=n;
    }
    return true;
}
static void hint(learning_model_handle_t m, const char *text)
{
    size_t n=strlen(text),cap=sizeof(m->view.hint)-1;
    if(n>cap){n=cap;while(n&&((unsigned char)text[n]&0xc0)==0x80)--n;}
    memcpy(m->view.hint,text,n);m->view.hint[n]=0;
}
static void command(learning_model_handle_t m, learning_command_t *out, learning_command_kind_t kind)
{ out->kind = kind; out->generation = m->view.generation; out->muted = m->view.muted;out->followup=m->conversation; }
static void cancel(learning_model_handle_t m, learning_command_t *out)
{
    ++m->view.generation; m->view.recording = false; m->view.speaking = false;m->view.light=false;
    m->view.tap_recording=false;m->awaiting_transcript=false;
    command(m, out, LEARN_CMD_CANCEL);
    out->keep_photo=m->questions.count>0;
    m->identifying=m->solving=false;
}
static void question_rows(learning_model_handle_t m)
{
    memset(m->view.question_rows,0,sizeof(m->view.question_rows));
    for(unsigned row=0;row<LEARN_QUESTION_ROWS;++row){unsigned i=m->view.question_page*LEARN_QUESTION_ROWS+row;
        if(i<m->questions.count)m->view.question_rows[row]=m->questions.items[i];
    }
}
static void fill_lines(learning_model_handle_t m,const char *p,const char *end)
{
    memset(m->view.lines,0,sizeof(m->view.lines));
    unsigned line=0,width=0;size_t used=0;
    while(p<end&&line<LEARN_LINES){
        unsigned char c=(unsigned char)*p;size_t bytes=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
        if(p+bytes>end)break;
        if(*p=='\r'){++p;continue;}
        if(*p=='\n'){++p;++line;width=0;used=0;continue;}
        if(width==LINE_COLUMNS||used+bytes>=LEARN_LINE_CAP){++line;width=0;used=0;if(line==LEARN_LINES)break;}
        memcpy(m->view.lines[line]+used,p,bytes);used+=bytes;++width;p+=bytes;
    }
}
static void page_lines(learning_model_handle_t m)
{
    if(m->view.page_count)fill_lines(m,m->answer+m->offsets[m->view.page_index],m->answer+m->offsets[m->view.page_index+1]);
}
static void review_lines(learning_model_handle_t m)
{fill_lines(m,m->view.transcript+m->review_offsets[m->view.review_index],m->view.transcript+m->review_offsets[m->view.review_index+1]);}
static unsigned paginate_offsets(const char *text,size_t *offsets)
{
    size_t i=0;unsigned line=0,width=0,pages=0;offsets[pages++]=0;
    while(text[i]){
        unsigned char c=(unsigned char)text[i];size_t bytes=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
        if(c=='\r'){++i;continue;}
        if(c=='\n'||width==LINE_COLUMNS){
            ++line;width=0;if(c=='\n')++i;
            if(line>=LEARN_LINES){if(pages>=LEARN_MAX_PAGES||!text[i])break;offsets[pages++]=i;line=0;}
            if(c=='\n')continue;
        }
        ++width;i+=bytes;
    }
    offsets[pages]=i;return pages;
}
static void paginate(learning_model_handle_t m)
{m->view.page_count=paginate_offsets(m->answer,m->offsets);m->view.page_index=0;page_lines(m);}
learning_model_handle_t learning_model_create(bool simulated)
{
    learning_model_handle_t m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->simulated = simulated; m->view.page = LEARN_HOME; m->view.generation = 1;
    m->view.revision = 1; m->return_page = LEARN_HOME;
    hint(m, simulated ? "模拟预览 · 不连接相机或云端" : "请先在设置中连接 Wi-Fi 和模型");
    return m;
}
void learning_model_delete(learning_model_handle_t m) { free(m); }
bool learning_model_action(learning_model_handle_t m, learning_action_t action, learning_command_t *out)
{
    if (!m || !out) return false;
    memset(out, 0, sizeof(*out));
    if(action==LEARN_VOICE_TOGGLE){
        if(m->key_held)return false;
        if(m->view.tap_recording){m->view.tap_recording=false;return learning_model_action(m,LEARN_KEY_UP,out);}
        if(m->view.page!=LEARN_VOICE)return false;
        bool ok=learning_model_action(m,LEARN_KEY_DOWN,out);
        if(m->view.recording){m->key_held=false;m->view.tap_recording=true;}
        return ok;
    }
    if (action == LEARN_KEY_UP) {
        if(m->view.tap_recording)return false;
        m->key_held = false;
        if (!m->view.recording) return false;
        m->view.recording = false;
        if (m->view.recording_ms < MIN_RECORD_MS) {
            cancel(m, out); m->view.page = LEARN_VOICE; hint(m, "请按住 AI 键说完一句话再松开");
        } else { command(m, out, LEARN_CMD_SEND_AUDIO); m->awaiting_transcript=true; m->view.page = LEARN_WAITING; hint(m, "正在识别语音，完成后请确认发送"); }
        ++m->view.revision; return true;
    }
    if (action == LEARN_KEY_DOWN) {
        if (m->key_held||m->view.recording) return false;
        if(m->identifying||m->solving||m->view.page==LEARN_QUESTIONS){hint(m,"请先选择要讲的题目");++m->view.revision;return true;}
        m->key_held = true;
        if (m->view.page == LEARN_SETTINGS || !m->view.configured || !m->view.online) {
            hint(m, "请先完成联网与模型配置"); ++m->view.revision; return true;
        }
        if(m->view.page==LEARN_HOME)m->conversation=false;
        ++m->view.generation; m->awaiting_transcript=false; m->view.speaking = false; m->view.page = LEARN_VOICE;
        m->retry=LEARN_ASK;
        m->view.light=false;m->view.recording = true; m->view.recording_ms = 0; m->view.transcript[0]=0;
        hint(m, "松开后识别文字 · 最长 30 秒"); command(m, out, LEARN_CMD_RECORD);
        ++m->view.revision; return true;
    }
    switch (action) {
    case LEARN_LIGHT:
    case LEARN_FLIP_CAMERA:
        if(m->view.page!=LEARN_CAMERA)return false;
        command(m,out,LEARN_CMD_CAMERA_OPTIONS);
        out->light=action==LEARN_LIGHT?!m->view.light:m->view.light;
        out->flipped=action==LEARN_FLIP_CAMERA?!m->view.flipped:m->view.flipped;
        break;
    case LEARN_PHOTO:
    case LEARN_RETAKE:
        memset(&m->questions,0,sizeof(m->questions));m->view.question_count=m->view.question_page=m->view.selected_question=0;
        if(action==LEARN_PHOTO)m->view.flipped=false;
        m->conversation=false; m->view.transcript[0]=0;
        cancel(m, out); command(m, out, LEARN_CMD_CAMERA);out->flipped=m->view.flipped; m->view.photo_ready = false;
        m->view.page = LEARN_CAMERA; hint(m, "拍清题号和完整题干，稍后选题"); m->retry = LEARN_PHOTO; break;
    case LEARN_CAPTURE:
        if (m->view.page != LEARN_CAMERA) return false;
        command(m, out, LEARN_CMD_CAPTURE); m->view.page = LEARN_WAITING;
        hint(m, "正在拍照，请保持稳定"); m->retry = LEARN_PHOTO; break;
    case LEARN_SEND:
        if (!m->view.photo_ready || m->view.page != LEARN_CONFIRM) return false;
        return learning_model_action(m,LEARN_SCAN,out);
    case LEARN_SCAN:
        if(!m->view.photo_ready||(m->view.page!=LEARN_CONFIRM&&m->view.page!=LEARN_ERROR))return false;
        if (!m->view.online || !m->view.configured) { hint(m, "请先完成联网与模型配置"); break; }
        ++m->view.generation;command(m, out, LEARN_CMD_IDENTIFY);m->identifying=true;m->solving=false;m->view.transcript[0]=0;
        m->view.page = LEARN_WAITING;
        hint(m, "正在识别题号，完成后请选择题目"); m->retry = LEARN_SCAN; break;
    case LEARN_PICK_0:
    case LEARN_PICK_1:
    case LEARN_PICK_2: {
        if(m->view.page!=LEARN_QUESTIONS)return false;
        unsigned i=m->view.question_page*LEARN_QUESTION_ROWS+(unsigned)(action-LEARN_PICK_0);
        if(i>=m->questions.count)return false;
        if(!m->view.online||!m->view.configured){hint(m,"请先连接网络后再选题");break;}
        const learning_question_t *q=&m->questions.items[i];
        ++m->view.generation;command(m,out,LEARN_CMD_SOLVE);out->keep_photo=true;
        snprintf(out->text,sizeof(out->text),"只讲解学生选中的这一题：索引%u，题号%s；题干片段：%s；原图位置：%s。请在原图核对题号、位置和完整题干，先复述这道题，再给提示、分步讲解与检验。不要讲解其他题目；定位不唯一或条件不清楚时要求重拍，不猜题。",i+1,q->number,q->summary,q->location);
        m->conversation=false;m->view.selected_question=i+1;m->view.speaking=false;m->view.transcript[0]=0;
        m->identifying=false;m->solving=true;m->view.page=LEARN_WAITING;m->retry=LEARN_CHOOSE_AGAIN;
        hint(m,"正在讲解你选择的题目，可以取消");break;
    }
    case LEARN_CHOOSE_AGAIN:
        if(!m->questions.count)return false;
        m->conversation=false;cancel(m,out);m->view.page=LEARN_QUESTIONS;m->view.transcript[0]=0;
        question_rows(m);hint(m,"请点击要讲解的题目");break;
    case LEARN_SEND_TEXT:
        if(m->view.page!=LEARN_VOICE_REVIEW||!m->view.transcript[0])return false;
        if(!m->view.online||!m->view.configured){hint(m,"请先完成联网与模型配置");++m->view.revision;return true;}
        ++m->view.generation;command(m,out,LEARN_CMD_SEND_TEXT);
        snprintf(out->text,sizeof(out->text),"%s",m->view.transcript);
        m->view.page=LEARN_WAITING;m->awaiting_transcript=false;
        hint(m,"已确认发送，正在思考");break;
    case LEARN_ASK:
        if(m->view.page==LEARN_HOME)m->conversation=false;
        cancel(m, out); m->view.page = LEARN_VOICE; m->view.transcript[0]=0;
        hint(m, m->conversation?"继续问这道题 · 按住 AI 键说话":"按住 AI 键或点声波录音，识别后确认"); break;
    case LEARN_BACK:
    case LEARN_CANCEL:
        {learning_page_t target=m->view.page==LEARN_SETTINGS?m->return_page:
            (m->questions.count&&(m->solving||m->view.page==LEARN_READING||m->view.page==LEARN_ERROR)?LEARN_QUESTIONS:
            (m->conversation&&(m->view.page==LEARN_VOICE||m->view.page==LEARN_WAITING||m->view.page==LEARN_VOICE_REVIEW)?LEARN_READING:LEARN_HOME));
        if(target==LEARN_HOME){m->conversation=false;memset(&m->questions,0,sizeof(m->questions));m->view.question_count=0;}
        if(target==LEARN_QUESTIONS)m->conversation=false;
        cancel(m, out);m->view.page=target;if(target==LEARN_READING)page_lines(m);}
        m->view.photo_ready = false; hint(m, m->simulated ? "模拟预览 · 不连接相机或云端" : "按住顶部 AI 键说话"); break;
    case LEARN_OPEN_SETTINGS:
        m->return_page = m->view.page==LEARN_READING||m->view.page==LEARN_VOICE_REVIEW||m->view.page==LEARN_QUESTIONS?m->view.page:LEARN_HOME;
        cancel(m, out); m->view.page = LEARN_SETTINGS; hint(m, "家长用手机连接设备热点后填写密钥"); break;
    case LEARN_SETUP:
        if (m->view.page != LEARN_SETTINGS) return false;
        command(m, out, LEARN_CMD_SETUP); hint(m, "正在开启手机配置，请稍候"); break;
    case LEARN_PREVIOUS:
    case LEARN_NEXT:
        if(m->view.page==LEARN_QUESTIONS){
            unsigned pages=(m->questions.count+LEARN_QUESTION_ROWS-1)/LEARN_QUESTION_ROWS;
            if(action==LEARN_NEXT&&m->view.question_page+1<pages)++m->view.question_page;
            else if(action==LEARN_PREVIOUS&&m->view.question_page)--m->view.question_page;
            else return false;
            question_rows(m);break;
        }
        if(m->view.page==LEARN_VOICE_REVIEW){
            if(action==LEARN_NEXT&&m->view.review_index+1<m->view.review_count)++m->view.review_index;
            else if(action==LEARN_PREVIOUS&&m->view.review_index)--m->view.review_index;
            else return false;
            review_lines(m);break;
        }
        if (m->view.page != LEARN_READING) return false;
        if (action == LEARN_NEXT && m->view.page_index + 1 < m->view.page_count) ++m->view.page_index;
        else if (action == LEARN_PREVIOUS && m->view.page_index) --m->view.page_index;
        else return false;
        cancel(m, out); page_lines(m); break;
    case LEARN_EXPLAIN:
        if (m->view.page != LEARN_READING) return false;
        ++m->view.generation; m->view.speaking = false;
        m->view.transcript[0]=0;
        command(m, out, LEARN_CMD_EXPLAIN);
        for (unsigned i = 0; i < LEARN_LINES; ++i) {
            size_t used = strlen(out->text);
            snprintf(out->text + used, sizeof(out->text) - used, "%s\n", m->view.lines[i]);
        }
        m->view.page = LEARN_WAITING; hint(m, "换一种方式解释这一页，可以取消"); m->retry = LEARN_ASK; break;
    case LEARN_READ:
        if (m->view.page != LEARN_READING) return false;
        if (m->view.speaking) { cancel(m, out); break; }
        // Each explicit read owns a fresh cancellation generation. Muted only
        // suppresses the UI's automatic read; it does not block this action.
        ++m->view.generation;
        command(m, out, LEARN_CMD_SPEAK);
        for (unsigned i = 0; i < LEARN_LINES; ++i) {
            size_t used = strlen(out->text);
            snprintf(out->text + used, sizeof(out->text) - used, "%s\n", m->view.lines[i]);
        }
        m->view.speaking = true; break;
    case LEARN_MUTE:
        m->view.muted = !m->view.muted; m->view.speaking = false;
        command(m, out, LEARN_CMD_MUTE); break;
    case LEARN_RETRY:
        if (m->view.page != LEARN_ERROR) return false;
        return learning_model_action(m, m->retry, out);
    default: return false;
    }
    ++m->view.revision; return true;
}
void learning_model_message(learning_model_handle_t m, const learning_message_t *msg)
{
    if (!m || !msg) return;
    if(msg->kind!=LEARN_MSG_QUESTIONS&&!valid_utf8(msg->text,sizeof(msg->text))){
        if(msg->generation==m->view.generation){m->view.page=LEARN_ERROR;m->view.recording=false;m->view.speaking=false;hint(m,"回答文字格式异常，请重新提问");++m->view.revision;}
        return;
    }
    if (msg->kind == LEARN_MSG_STATUS) {
        m->view.online = msg->online; m->view.configured = msg->configured;
        m->view.api_state=msg->online?msg->api_state:LEARN_API_UNKNOWN;
        if (msg->text[0]) hint(m, msg->text);
    } else {
        if (msg->generation != m->view.generation) return;
        switch (msg->kind) {
        case LEARN_MSG_QUESTIONS: {
            if(!m->identifying||m->view.page!=LEARN_WAITING)return;
            m->identifying=false;
            bool valid=msg->questions.count>0&&msg->questions.count<=LEARN_QUESTION_MAX;
            for(unsigned i=0;valid&&i<msg->questions.count;++i){const learning_question_t *q=&msg->questions.items[i];
                valid=q->number[0]&&q->summary[0]&&q->location[0]&&valid_utf8(q->number,sizeof(q->number))&&valid_utf8(q->summary,sizeof(q->summary))&&valid_utf8(q->location,sizeof(q->location));
            }
            if(!valid){m->view.page=LEARN_ERROR;hint(m,"没有识别到有效题目，请重试或重拍");break;}
            m->questions=msg->questions;m->view.question_count=m->questions.count;m->view.question_page=0;
            m->view.page=LEARN_QUESTIONS;question_rows(m);hint(m,"请点击要讲解的题目");break;
        }
        case LEARN_MSG_SETUP: {
            memset(m->view.setup_lines,0,sizeof(m->view.setup_lines));
            const char *p=msg->text;
            for(unsigned i=0;i<3&&*p;++i){
                const char *end=strchr(p,'\n');size_t n=end?(size_t)(end-p):strlen(p);
                if(n>=LEARN_LINE_CAP)n=LEARN_LINE_CAP-1;
                memcpy(m->view.setup_lines[i],p,n);p=end?end+1:p+n;
            }
            m->view.page=LEARN_SETTINGS;hint(m,"用手机连接此热点后打开配置页");break;
        }
        case LEARN_MSG_CAMERA_OPTIONS:
            if(m->view.page!=LEARN_CAMERA)return;
            m->view.light=msg->light;m->view.flipped=msg->flipped;
            if(msg->text[0])hint(m,msg->text);
            break;
        case LEARN_MSG_PHOTO: m->view.light=false;m->view.photo_ready = true; m->view.page = LEARN_CONFIRM; hint(m, "确认题目清楚后再发送"); break;
        case LEARN_MSG_ANSWER:
            if(m->identifying||m->view.page==LEARN_QUESTIONS||m->awaiting_transcript||m->view.page==LEARN_VOICE_REVIEW)return;
            m->solving=false;
            if (!msg->text[0]) { m->view.page = LEARN_ERROR; hint(m, "没有收到完整回答，请重试"); break; }
            snprintf(m->answer, sizeof(m->answer), "%s", msg->text); paginate(m);
            m->conversation=true;
            m->view.page = LEARN_READING; m->view.speaking = false; m->view.recording=false;
            hint(m, "可以翻页，也可以按住 AI 键追问"); break;
        case LEARN_MSG_TRANSCRIPT: {
            if(!m->awaiting_transcript||m->view.page!=LEARN_WAITING)return;
            m->awaiting_transcript=false;
            size_t n=strlen(msg->text);
            if(!n||n>=sizeof(m->view.transcript)){m->view.page=LEARN_ERROR;hint(m,"识别内容为空或过长，请分段重说");break;}
            memcpy(m->view.transcript,msg->text,n+1);
            m->view.review_count=paginate_offsets(m->view.transcript,m->review_offsets);m->view.review_index=0;
            if(m->review_offsets[m->view.review_count]!=n){m->view.page=LEARN_ERROR;hint(m,"识别内容分段过多，请重新简短说一次");break;}
            m->view.page=LEARN_VOICE_REVIEW;review_lines(m);
            hint(m,"请确认识别文字，再点击发送");break;
        }
        case LEARN_MSG_ERROR:
            m->identifying=m->solving=false;
            m->view.light=false;m->view.page = LEARN_ERROR; m->view.recording = false; m->view.speaking = false;
            hint(m, msg->text[0] ? msg->text : "操作失败，请重试"); break;
        case LEARN_MSG_SPEECH_DONE: m->view.speaking = false; if (msg->text[0]) hint(m, msg->text); break;
        default: break;
        }
    }
    ++m->view.revision;
}
bool learning_model_tick(learning_model_handle_t m, unsigned elapsed, learning_command_t *out)
{
    if (!m || !out || !m->view.recording) return false;
    m->view.recording_ms += elapsed;
    if (m->view.recording_ms >= LEARN_MAX_RECORD_MS) {
        cancel(m, out); m->view.page = LEARN_VOICE; m->view.transcript[0]=0;
        hint(m, "录音已达 30 秒，请松开后重新分段提问"); ++m->view.revision; return true;
    }
    return false;
}
void learning_model_snapshot(learning_model_handle_t m, learning_snapshot_t *out)
{ if (m && out) *out = m->view; }
