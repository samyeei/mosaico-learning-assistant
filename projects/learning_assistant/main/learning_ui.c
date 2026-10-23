// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_ui.h"
#include "learning_model.h"
#include "learning_platform.h"
#include "bundle_gsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TICK_MS = 50, PANEL_HEIGHT=480, PANEL_EDGE=24, PANEL_COMMIT=80, GRADE_ROWS=4, GRADE_ROW_HEIGHT=48,
       GRADE_TOP=246, GRADE_LEFT=24, GRADE_WIDTH=432, WAVE_PHASES=8, WAVE_STEP_MS=100 };
struct learning_ui_t {
    esp_gsp_handle_t ui;
    learning_model_handle_t model;
    learning_snapshot_t view;
    learning_message_t message;
    learning_command_t command;
    char error_text[384];
    void *timer;
    bool key, rendered;
    bool controls_rendered;
    bool panel_open,pointer_down,panel_drag,drag_was_open,grade_open;
    int press_y,last_y;
    unsigned wave_phase,wave_elapsed;
    unsigned grade_offset;
    int press_x;bool grade_drag;
    learning_controls_t controls;
    esp_gsp_err_t error;
};
static void checked(learning_ui_handle_t s, esp_gsp_err_t err)
{ if (err != ESP_GSP_OK) { s->error = err; fprintf(stderr, "learning_ui: error=%d\n", err); } }
static void render_grades(learning_ui_handle_t s)
{
    static esp_gsp_err_t (*const text[])(esp_gsp_handle_t,const char*)={gsp_learn_grade_item_0_set_text,gsp_learn_grade_item_1_set_text,gsp_learn_grade_item_2_set_text,gsp_learn_grade_item_3_set_text};
    static esp_gsp_err_t (*const color[])(esp_gsp_handle_t,uint32_t)={gsp_learn_grade_pick_0_set_color_rgb888,gsp_learn_grade_pick_1_set_color_rgb888,gsp_learn_grade_pick_2_set_color_rgb888,gsp_learn_grade_pick_3_set_color_rgb888};
    learning_controls_t v;learning_platform_controls_get(&v);
    for(unsigned i=0;i<GRADE_ROWS;++i){unsigned grade=s->grade_offset+i+1;
        checked(s,text[i](s->ui,learning_grade_name(grade)));
        checked(s,color[i](s->ui,grade==v.grade?0xC9F4E9:0xF4FCFD));
    }
}
// UI-thread owned overlay: visibility switching avoids Drawer's retained
// two-screen backing cache (~900 KiB), leaving the DVP camera frame budget.
static void panel_target(learning_ui_handle_t s,bool open)
{
    if(open){s->grade_open=false;checked(s,gsp_learn_grade_menu_set_visible(s->ui,false));}
    if(s->panel_open!=open)fprintf(stderr,"learning_panel: open=%u\n",(unsigned)open);
    s->panel_open=open;
    checked(s,gsp_learn_quick_settings_set_visible(s->ui,open));
}
static void pointer(esp_gsp_handle_t ui,int32_t x,int32_t y,bool pressed,void *ctx)
{
    (void)ui;(void)x;learning_ui_handle_t s=ctx;
    if(pressed){
        if(!s->pointer_down){s->pointer_down=true;s->press_y=y;s->press_x=x;s->grade_drag=false;s->drag_was_open=s->panel_open;}
        s->last_y=y;int dy=y-s->press_y;
        bool edge=!s->drag_was_open?s->press_y<=PANEL_EDGE&&dy>PANEL_EDGE:
            s->press_y>=PANEL_HEIGHT-60&&dy<-PANEL_EDGE;
        if(edge)s->panel_drag=true;
        if(s->grade_open&&s->press_x>=GRADE_LEFT&&s->press_x<=GRADE_LEFT+GRADE_WIDTH&&s->press_y>=GRADE_TOP&&s->press_y<GRADE_TOP+GRADE_ROWS*GRADE_ROW_HEIGHT&&(dy>16||dy<-16))s->grade_drag=true;

    }else if(s->pointer_down){
        if(s->grade_open&&s->grade_drag){int dy=s->last_y-s->press_y;int rows=dy>0?-(dy+GRADE_ROW_HEIGHT/2)/GRADE_ROW_HEIGHT:-(dy-GRADE_ROW_HEIGHT/2)/GRADE_ROW_HEIGHT;
            int offset=(int)s->grade_offset+rows;s->grade_offset=(unsigned)(offset<0?0:offset>LEARN_GRADE_MAX-GRADE_ROWS?LEARN_GRADE_MAX-GRADE_ROWS:offset);render_grades(s);
        }
        if(s->panel_drag){int dy=s->last_y-s->press_y;panel_target(s,s->drag_was_open?dy>-PANEL_COMMIT:dy>=PANEL_COMMIT);}
        s->pointer_down=false;s->panel_drag=false;
    }
}
bool learning_ui_quick_is_open(learning_ui_handle_t s){return s&&s->panel_open;}
static void animate_recording(learning_ui_handle_t s)
{
    static esp_gsp_err_t (*const visible[])(esp_gsp_handle_t,bool)={
        gsp_learn_wave_0_set_visible,gsp_learn_wave_1_set_visible,gsp_learn_wave_2_set_visible,
        gsp_learn_wave_3_set_visible,gsp_learn_wave_4_set_visible,gsp_learn_wave_5_set_visible,
        gsp_learn_wave_6_set_visible,gsp_learn_wave_7_set_visible,gsp_learn_wave_8_set_visible};
    unsigned next=s->wave_phase;
    if(s->view.page==LEARN_VOICE&&s->view.recording){
        s->wave_elapsed+=TICK_MS;
        if(s->wave_elapsed>=WAVE_STEP_MS){s->wave_elapsed=0;next=1+s->wave_phase%WAVE_PHASES;}
    }else{next=0;s->wave_elapsed=0;}
    if(next!=s->wave_phase){
        checked(s,visible[s->wave_phase](s->ui,false));
        checked(s,visible[next](s->ui,true));s->wave_phase=next;
    }
}
static void render_controls(learning_ui_handle_t s,const learning_controls_t *v)
{
    char text[16];
    if(!s->controls_rendered||v->brightness!=s->controls.brightness){
        checked(s,gsp_learn_quick_brightness_set_value(s->ui,(int32_t)v->brightness));
        snprintf(text,sizeof(text),"%u%%",v->brightness);checked(s,gsp_learn_brightness_percent_set_text(s->ui,text));
    }
    if(!s->controls_rendered||v->volume!=s->controls.volume){
        checked(s,gsp_learn_quick_volume_set_value(s->ui,(int32_t)v->volume));
        snprintf(text,sizeof(text),"%u%%",v->volume);checked(s,gsp_learn_volume_percent_set_text(s->ui,text));
    }
    if(!s->controls_rendered||v->speech_enabled!=s->controls.speech_enabled){
        checked(s,gsp_learn_speech_on_set_visible(s->ui,v->speech_enabled));
        checked(s,gsp_learn_speech_off_set_visible(s->ui,!v->speech_enabled));
    }
    if(!s->controls_rendered||v->automatic!=s->controls.automatic){
        checked(s,gsp_learn_direction_auto_set_visible(s->ui,v->automatic));
        checked(s,gsp_learn_direction_locked_set_visible(s->ui,!v->automatic));
    }
    if(!s->controls_rendered||v->grade!=s->controls.grade){
        checked(s,gsp_learn_grade_label_set_text(s->ui,learning_grade_name(v->grade)));
    }
    s->controls=*v;s->controls_rendered=true;
}
// Labels use explicit bounded lines, matching the answer's 18-column layout.
static void wrap_progress(char *out,size_t capacity,const char *text)
{
    const unsigned char *p=(const unsigned char*)text;size_t used=0;unsigned col=0,lines=1;
    while(*p&&used+8<capacity){
        size_t n=*p<0x80?1:*p<0xe0?2:*p<0xf0?3:4;
        if(*p=='\r'){++p;continue;}
        if(col==18||*p=='\n'){
            if(lines==3){memcpy(out+used,"…",3);used+=3;break;}
            out[used++]='\n';col=0;++lines;if(*p=='\n'){++p;continue;}
        }
        memcpy(out+used,p,n);used+=n;p+=n;++col;
    }
    out[used]=0;
}
// Model/provider validate UTF-8 before presentation; shorten only at boundaries.
static void short_text(char *out,const char *text,unsigned max_columns)
{
    const char *p=text;size_t used=0;unsigned cols=0;
    while(*p&&cols<max_columns){size_t n=(unsigned char)*p<0x80?1:(unsigned char)*p<0xe0?2:(unsigned char)*p<0xf0?3:4;
        memcpy(out+used,p,n);used+=n;p+=n;++cols;}
    if(*p){memcpy(out+used,"…",3);used+=3;}out[used]=0;
}
static void render(learning_ui_handle_t s)
{
    learning_snapshot_t *v = &s->view;
    if(v->page!=LEARN_SETTINGS&&s->grade_open){s->grade_open=false;checked(s,gsp_learn_grade_menu_set_visible(s->ui,false));}
    checked(s,gsp_learn_camera_tools_set_visible(s->ui,v->page==LEARN_CAMERA));
    checked(s,gsp_learn_light_label_set_text(s->ui,v->light?"补光已开":"补光"));
    checked(s,gsp_learn_flip_label_set_text(s->ui,v->flipped?"已翻转180°":""));
    checked(s, gsp_learn_home_set_visible(s->ui, v->page == LEARN_HOME));
    checked(s, gsp_learn_camera_set_visible(s->ui, v->page == LEARN_CAMERA || v->page == LEARN_CONFIRM));
    checked(s, gsp_learn_confirm_set_visible(s->ui, v->page == LEARN_CONFIRM));
    checked(s, gsp_learn_reading_set_visible(s->ui, v->page == LEARN_READING));
    checked(s,gsp_learn_questions_set_visible(s->ui,v->page==LEARN_QUESTIONS));
    checked(s,gsp_learn_choose_again_set_visible(s->ui,v->question_count>0));
    checked(s, gsp_learn_voice_set_visible(s->ui, v->page == LEARN_VOICE));
    checked(s,gsp_learn_voice_review_set_visible(s->ui,v->page==LEARN_VOICE_REVIEW));
    checked(s, gsp_learn_waiting_set_visible(s->ui, v->page == LEARN_WAITING));
    checked(s, gsp_learn_settings_panel_set_visible(s->ui, v->page == LEARN_SETTINGS));
    checked(s, gsp_learn_error_panel_set_visible(s->ui, v->page == LEARN_ERROR));
    checked(s, gsp_learn_status_set_text(s->ui, v->page==LEARN_QUESTIONS||v->page==LEARN_SETTINGS||v->page==LEARN_HOME||v->page==LEARN_VOICE||v->page==LEARN_CAMERA||v->page==LEARN_CONFIRM||v->page==LEARN_VOICE_REVIEW?"":v->hint));
    if(v->page==LEARN_QUESTIONS){
        static esp_gsp_err_t (*const number[])(esp_gsp_handle_t,const char*)={gsp_learn_question_number_0_set_text,gsp_learn_question_number_1_set_text,gsp_learn_question_number_2_set_text};
        static esp_gsp_err_t (*const location[])(esp_gsp_handle_t,const char*)={gsp_learn_question_location_0_set_text,gsp_learn_question_location_1_set_text,gsp_learn_question_location_2_set_text};
        static esp_gsp_err_t (*const summary[])(esp_gsp_handle_t,const char*)={gsp_learn_question_summary_0_set_text,gsp_learn_question_summary_1_set_text,gsp_learn_question_summary_2_set_text};
        static esp_gsp_err_t (*const visible[])(esp_gsp_handle_t,bool)={gsp_learn_question_card_0_set_visible,gsp_learn_question_card_1_set_visible,gsp_learn_question_card_2_set_visible};
        char text[128];snprintf(text,sizeof(text),"识别到 %u 道题",v->question_count);checked(s,gsp_learn_question_count_set_text(s->ui,text));
        checked(s,gsp_learn_question_instruction_set_text(s->ui,v->online?"核对题号和题干，点击后开始讲解":"网络未连接，请联网后再选择题目"));
        for(unsigned i=0;i<LEARN_QUESTION_ROWS;++i){
            checked(s,visible[i](s->ui,v->question_page*LEARN_QUESTION_ROWS+i<v->question_count));
            short_text(text,v->question_rows[i].number,11);checked(s,number[i](s->ui,text));
            short_text(text,v->question_rows[i].location,6);checked(s,location[i](s->ui,text));
            short_text(text,v->question_rows[i].summary,20);
            checked(s,summary[i](s->ui,text));
        }
        unsigned pages=(v->question_count+LEARN_QUESTION_ROWS-1)/LEARN_QUESTION_ROWS;
        snprintf(text,sizeof(text),"%u / %u",v->question_page+1,pages);checked(s,gsp_learn_question_page_set_text(s->ui,text));
        checked(s,gsp_learn_question_previous_set_enabled(s->ui,v->question_page>0));
        checked(s,gsp_learn_question_next_set_enabled(s->ui,v->question_page+1<pages));
    }
    checked(s,gsp_learn_wifi_dot_set_color_rgb888(s->ui,v->online?0x22C55E:0xA4AFB8));
    checked(s,gsp_learn_api_dot_set_color_rgb888(s->ui,v->online&&v->api_state==LEARN_API_READY?0x22C55E:
        v->online&&v->api_state==LEARN_API_ERROR?0xD79A32:0xA4AFB8));
    if (v->page == LEARN_READING) {
        checked(s,gsp_learn_read_label_set_text(s->ui,v->speaking?"停止":"朗读"));
        char text[48]; snprintf(text, sizeof(text), "%u / %u", v->page_index+1, v->page_count);
        checked(s, gsp_learn_page_hint_set_text(s->ui, text));
        checked(s,gsp_learn_previous_set_enabled(s->ui,v->page_index>0));
        checked(s,gsp_learn_next_set_enabled(s->ui,v->page_index+1<v->page_count));
        checked(s, gsp_learn_line_0_set_text(s->ui, v->lines[0]));
        checked(s, gsp_learn_line_1_set_text(s->ui, v->lines[1]));
        checked(s, gsp_learn_line_2_set_text(s->ui, v->lines[2]));
        checked(s, gsp_learn_line_3_set_text(s->ui, v->lines[3]));
        checked(s, gsp_learn_line_4_set_text(s->ui, v->lines[4]));
        checked(s, gsp_learn_line_5_set_text(s->ui, v->lines[5]));
    }
    if (v->page == LEARN_WAITING) {
        wrap_progress(s->error_text,sizeof(s->error_text),v->transcript[0]?v->transcript:v->hint);
        checked(s,gsp_learn_waiting_text_set_text(s->ui,s->error_text));
    }
    if(v->page==LEARN_VOICE){
        checked(s,gsp_learn_voice_record_label_set_text(s->ui,v->recording?"结束录音":"开始说话"));
    }
    if(v->page==LEARN_VOICE_REVIEW){
        checked(s,gsp_learn_review_instruction_set_text(s->ui,v->hint));
        char text[48];snprintf(text,sizeof(text),"%u / %u",v->review_index+1,v->review_count);
        checked(s,gsp_learn_review_page_set_text(s->ui,text));
        checked(s,gsp_learn_review_previous_set_enabled(s->ui,v->review_index>0));
        checked(s,gsp_learn_review_next_set_enabled(s->ui,v->review_index+1<v->review_count));
        checked(s,gsp_learn_review_line_0_set_text(s->ui,v->lines[0]));
        checked(s,gsp_learn_review_line_1_set_text(s->ui,v->lines[1]));
        checked(s,gsp_learn_review_line_2_set_text(s->ui,v->lines[2]));
        checked(s,gsp_learn_review_line_3_set_text(s->ui,v->lines[3]));
        checked(s,gsp_learn_review_line_4_set_text(s->ui,v->lines[4]));
        checked(s,gsp_learn_review_line_5_set_text(s->ui,v->lines[5]));
    }
    if (v->page == LEARN_SETTINGS) {
        checked(s, gsp_learn_network_set_text(s->ui, v->online ? "网络：已连接" : "网络：未连接"));
        checked(s, gsp_learn_model_status_set_text(s->ui, !v->configured?"API：尚未配置":v->api_state==LEARN_API_READY?"API：已连通":v->api_state==LEARN_API_ERROR?"API：连接异常":"API：已配置，待验证"));
        checked(s, gsp_learn_setup_a_set_text(s->ui, v->setup_lines[0]));
        checked(s, gsp_learn_setup_b_set_text(s->ui, v->setup_lines[1]));
        checked(s, gsp_learn_setup_c_set_text(s->ui, v->setup_lines[2]));
    }
    if (v->page == LEARN_ERROR) {
        const unsigned char *p=(const unsigned char*)v->hint;size_t used=0;unsigned count=0;
        while(*p&&used+6<sizeof(s->error_text)){
            size_t n=*p<0x80?1:*p<0xe0?2:*p<0xf0?3:4;
            if(count==18){s->error_text[used++]='\n';count=0;}
            memcpy(s->error_text+used,p,n);used+=n;p+=n;++count;
        }
        s->error_text[used]=0;
        checked(s,gsp_learn_error_text_set_text(s->ui,s->error_text));
    }
}
static void submit(learning_ui_handle_t s)
{
    if (s->command.kind != LEARN_CMD_NONE && !learning_platform_submit(&s->command)) {
        memset(&s->message, 0, sizeof(s->message));
        s->message.kind = LEARN_MSG_ERROR; s->message.generation = s->command.generation;
        snprintf(s->message.text, sizeof(s->message.text), "设备忙，请稍后重试");
        learning_model_message(s->model, &s->message);
    }
}
static void action(learning_ui_handle_t s, learning_action_t a)
{ if (learning_model_action(s->model, a, &s->command)) submit(s); }
static void event(esp_gsp_handle_t ui, const esp_gsp_event_t *e, void *ctx)
{
    (void)ui; learning_ui_handle_t s = ctx;
    if (e->type != ESP_GSP_EVENT_CALL) return;
    if(gsp_learn_event_is_grade_toggle(e)){
        s->grade_open=!s->grade_open;checked(s,gsp_learn_grade_menu_set_visible(s->ui,s->grade_open));
        if(s->grade_open){learning_controls_t v;learning_platform_controls_get(&v);s->grade_offset=v.grade>GRADE_ROWS?v.grade-GRADE_ROWS:0;render_grades(s);}
        return;
    }
    int grade_row=gsp_learn_event_is_grade_pick_0(e)?0:gsp_learn_event_is_grade_pick_1(e)?1:gsp_learn_event_is_grade_pick_2(e)?2:gsp_learn_event_is_grade_pick_3(e)?3:-1;
    if(grade_row>=0){
        if(!s->grade_open||s->grade_drag)return;
        learning_platform_controls_set(LEARN_CONTROL_GRADE,(int)s->grade_offset+grade_row+1);
        s->grade_open=false;checked(s,gsp_learn_grade_menu_set_visible(s->ui,false));return;
    }
    if(s->grade_open){s->grade_open=false;checked(s,gsp_learn_grade_menu_set_visible(s->ui,false));return;}
    if(gsp_learn_event_is_quick_close(e)){panel_target(s,false);return;}
    if(gsp_learn_event_is_quick_brightness(e)){learning_platform_controls_set(LEARN_CONTROL_BRIGHTNESS,(int32_t)e->arg);return;}
    if(gsp_learn_event_is_quick_volume(e)){learning_platform_controls_set(LEARN_CONTROL_VOLUME,(int32_t)e->arg);return;}
    if(gsp_learn_event_is_quick_speech(e)){action(s,LEARN_MUTE);return;}
    if(gsp_learn_event_is_direction(e)){
        learning_controls_t v;learning_platform_controls_get(&v);
        learning_platform_controls_set(LEARN_CONTROL_AUTO,!v.automatic);return;
    }
    if(gsp_learn_event_is_pick_0(e))action(s,LEARN_PICK_0);
    else if(gsp_learn_event_is_pick_1(e))action(s,LEARN_PICK_1);
    else if(gsp_learn_event_is_pick_2(e))action(s,LEARN_PICK_2);
    else if(gsp_learn_event_is_choose_again(e))action(s,LEARN_CHOOSE_AGAIN);
    else if(gsp_learn_event_is_question_previous(e))action(s,LEARN_PREVIOUS);
    else if(gsp_learn_event_is_question_next(e))action(s,LEARN_NEXT);
    else if(gsp_learn_event_is_question_retake(e))action(s,LEARN_RETAKE);
    else if(gsp_learn_event_is_light(e))action(s,LEARN_LIGHT);
    else if(gsp_learn_event_is_flip_camera(e))action(s,LEARN_FLIP_CAMERA);
    else if(gsp_learn_event_is_voice_toggle(e))action(s,LEARN_VOICE_TOGGLE);
    else if(gsp_learn_event_is_send_text(e))action(s,LEARN_SEND_TEXT);
    else if(gsp_learn_event_is_voice_retake(e))action(s,LEARN_ASK);
    else if (gsp_learn_event_is_photo(e)) action(s, LEARN_PHOTO);
    else if (gsp_learn_event_is_capture(e)) action(s, LEARN_CAPTURE);
    else if (gsp_learn_event_is_retake(e)) action(s, LEARN_RETAKE);
    else if (gsp_learn_event_is_send(e)) action(s, LEARN_SEND);
    else if (gsp_learn_event_is_ask(e) || gsp_learn_event_is_followup(e)) action(s, LEARN_ASK);
    else if (gsp_learn_event_is_settings(e)){panel_target(s,false);action(s, LEARN_OPEN_SETTINGS);}
    else if (gsp_learn_event_is_next(e)) action(s, LEARN_NEXT);
    else if (gsp_learn_event_is_previous(e)) action(s, LEARN_PREVIOUS);
    else if (gsp_learn_event_is_explain(e)) action(s, LEARN_EXPLAIN);
    else if (gsp_learn_event_is_read(e)) action(s, LEARN_READ);
    else if (gsp_learn_event_is_setup(e)) action(s, LEARN_SETUP);
    else if (gsp_learn_event_is_retry(e)) action(s, LEARN_RETRY);
    else action(s, LEARN_BACK);
}
static void tick(esp_gsp_handle_t ui, void *ctx)
{
    (void)ui; learning_ui_handle_t s = ctx;
    uint32_t previous = s->view.revision;
    learning_model_snapshot(s->model,&s->view);
    learning_controls_t controls;learning_platform_controls_get(&controls);
    if(s->view.muted==controls.speech_enabled)action(s,LEARN_MUTE);
    bool key = learning_platform_key();
    if (key != s->key) { s->key = key; action(s, key ? LEARN_KEY_DOWN : LEARN_KEY_UP); }
    if (learning_model_tick(s->model, TICK_MS, &s->command)) submit(s);
    for (unsigned i = 0; i < 4 && learning_platform_poll(&s->message); ++i) {
        learning_model_message(s->model, &s->message);
        if(s->message.kind==LEARN_MSG_ANSWER){
            learning_model_snapshot(s->model,&s->view);
            if(s->message.generation==s->view.generation&&s->view.page==LEARN_READING&&!s->view.muted)action(s,LEARN_READ);
        }
    }
    learning_model_snapshot(s->model, &s->view);
    if (!s->rendered || previous != s->view.revision) { render(s); s->rendered = true; }
    learning_platform_controls_get(&controls);render_controls(s,&controls);
    animate_recording(s);
    learning_platform_presented(&s->view);
}
esp_gsp_err_t learning_ui_create(esp_gsp_handle_t ui, learning_ui_handle_t *out)
{
    if (!ui || !out) return ESP_GSP_ERR_INVALID_ARG;
    *out = NULL; learning_ui_handle_t s = calloc(1, sizeof(*s));
    if (!s) return ESP_GSP_ERR_NO_MEM;
    s->ui = ui; s->model = learning_model_create(learning_platform_simulated());
    if (!s->model) { free(s); return ESP_GSP_ERR_NO_MEM; }
    esp_gsp_err_t err = esp_gsp_on_event(ui, event, s);
    if (err != ESP_GSP_OK) { learning_ui_delete(s); return err; }
    err=esp_gsp_set_pointer_observer(ui,pointer,s);
    if(err!=ESP_GSP_OK){learning_ui_delete(s);return err;}
    s->timer = esp_gsp_timer_create(ui, TICK_MS, tick, s);
    if (!s->timer) { learning_ui_delete(s); return ESP_GSP_ERR_NO_MEM; }
    *out = s; return ESP_GSP_OK;
}
void learning_ui_delete(learning_ui_handle_t s)
{
    if (!s) return;
    if (s->timer) esp_gsp_timer_delete(s->ui, s->timer);
    esp_gsp_set_pointer_observer(s->ui,NULL,NULL);
    esp_gsp_on_event(s->ui, NULL, NULL);
    learning_model_delete(s->model); free(s);
}
