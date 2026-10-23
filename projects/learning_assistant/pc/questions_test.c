// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static learning_command_t cmd;
static learning_message_t msg;
static learning_snapshot_t v;
static void state(learning_model_handle_t m,learning_page_t page)
{learning_model_snapshot(m,&v);assert(v.page==page);}
static void scan(learning_model_handle_t m)
{
    assert(learning_model_action(m,LEARN_PHOTO,&cmd));
    assert(learning_model_action(m,LEARN_CAPTURE,&cmd));
    memset(&msg,0,sizeof(msg));msg.kind=LEARN_MSG_PHOTO;msg.generation=cmd.generation;
    learning_model_message(m,&msg);state(m,LEARN_CONFIRM);
    assert(learning_model_action(m,LEARN_SEND,&cmd)&&cmd.kind==LEARN_CMD_IDENTIFY);
    assert(!learning_model_action(m,LEARN_SEND,&cmd));state(m,LEARN_WAITING);
}
static void candidates(learning_model_handle_t m,unsigned count)
{
    learning_model_snapshot(m,&v);memset(&msg,0,sizeof(msg));
    msg.kind=LEARN_MSG_QUESTIONS;msg.generation=v.generation;msg.questions.count=count;
    for(unsigned i=0;i<count&&i<LEARN_QUESTION_MAX;++i){learning_question_t *q=&msg.questions.items[i];
        snprintf(q->number,sizeof(q->number),"第%u题",i+8);
        snprintf(q->summary,sizeof(q->summary),"原题片段%u",i);
        snprintf(q->location,sizeof(q->location),"右列第%u行",i+1);
    }
    learning_model_message(m,&msg);
}
int main(void)
{
    learning_model_handle_t m=learning_model_create(false);assert(m);
    memset(&msg,0,sizeof(msg));msg.kind=LEARN_MSG_STATUS;msg.online=msg.configured=true;learning_model_message(m,&msg);
    assert(!learning_model_action(m,LEARN_PICK_0,&cmd));scan(m);
    unsigned gen=v.generation;
    msg.kind=LEARN_MSG_ANSWER;msg.generation=gen;strcpy(msg.text,"不能直接讲题");learning_model_message(m,&msg);state(m,LEARN_WAITING);
    assert(learning_model_action(m,LEARN_KEY_DOWN,&cmd)&&cmd.kind==LEARN_CMD_NONE);state(m,LEARN_WAITING);assert(!v.recording);
    candidates(m,7);state(m,LEARN_QUESTIONS);assert(v.question_count==7);
    assert(!learning_model_action(m,LEARN_PREVIOUS,&cmd));
    assert(learning_model_action(m,LEARN_NEXT,&cmd)&&cmd.kind==LEARN_CMD_NONE);
    assert(learning_model_action(m,LEARN_NEXT,&cmd));state(m,LEARN_QUESTIONS);assert(v.question_page==2);
    assert(!strcmp(v.question_rows[0].number,"第14题"));assert(!v.question_rows[1].number[0]);
    assert(!learning_model_action(m,LEARN_PICK_1,&cmd));assert(!learning_model_action(m,LEARN_NEXT,&cmd));
    assert(learning_model_action(m,LEARN_OPEN_SETTINGS,&cmd)&&cmd.keep_photo);
    assert(learning_model_action(m,LEARN_BACK,&cmd)&&cmd.keep_photo);state(m,LEARN_QUESTIONS);
    assert(learning_model_action(m,LEARN_PICK_0,&cmd)&&cmd.kind==LEARN_CMD_SOLVE);
    assert(strstr(cmd.text,"第14题")&&strstr(cmd.text,"右列第7行"));gen=cmd.generation;
    assert(!learning_model_action(m,LEARN_PICK_0,&cmd));
    assert(learning_model_action(m,LEARN_CANCEL,&cmd)&&cmd.keep_photo);state(m,LEARN_QUESTIONS);
    memset(&msg,0,sizeof(msg));msg.kind=LEARN_MSG_ANSWER;msg.generation=gen;strcpy(msg.text,"旧讲解");learning_model_message(m,&msg);state(m,LEARN_QUESTIONS);
    assert(learning_model_action(m,LEARN_PICK_0,&cmd));
    msg.generation=cmd.generation;strcpy(msg.text,"第14题的讲解");learning_model_message(m,&msg);state(m,LEARN_READING);
    assert(learning_model_action(m,LEARN_ASK,&cmd)&&cmd.followup);state(m,LEARN_VOICE);
    assert(learning_model_action(m,LEARN_CANCEL,&cmd)&&cmd.keep_photo&&cmd.followup);state(m,LEARN_READING);
    assert(learning_model_action(m,LEARN_CHOOSE_AGAIN,&cmd)&&cmd.keep_photo&&!cmd.followup);state(m,LEARN_QUESTIONS);
    assert(learning_model_action(m,LEARN_BACK,&cmd)&&!cmd.keep_photo);state(m,LEARN_HOME);
    assert(!learning_model_action(m,LEARN_CHOOSE_AGAIN,&cmd));
    scan(m);candidates(m,1);state(m,LEARN_QUESTIONS);
    assert(!learning_model_action(m,LEARN_PICK_1,&cmd));assert(!learning_model_action(m,LEARN_NEXT,&cmd));
    assert(learning_model_action(m,LEARN_PICK_0,&cmd));state(m,LEARN_WAITING);
    memset(&msg,0,sizeof(msg));msg.kind=LEARN_MSG_ERROR;msg.generation=cmd.generation;strcpy(msg.text,"模拟超时");learning_model_message(m,&msg);
    assert(learning_model_action(m,LEARN_RETRY,&cmd)&&cmd.kind==LEARN_CMD_CANCEL&&cmd.keep_photo);state(m,LEARN_QUESTIONS);
    scan(m);candidates(m,0);state(m,LEARN_ERROR);
    assert(learning_model_action(m,LEARN_RETRY,&cmd)&&cmd.kind==LEARN_CMD_IDENTIFY);state(m,LEARN_WAITING);
    candidates(m,LEARN_QUESTION_MAX+1);state(m,LEARN_ERROR);
    assert(learning_model_action(m,LEARN_RETRY,&cmd));gen=cmd.generation;
    assert(learning_model_action(m,LEARN_CANCEL,&cmd));candidates(m,1);state(m,LEARN_HOME);
    msg.generation=gen;learning_model_message(m,&msg);state(m,LEARN_HOME);
    scan(m);candidates(m,1);state(m,LEARN_QUESTIONS);
    assert(learning_model_action(m,LEARN_RETAKE,&cmd)&&!cmd.keep_photo);state(m,LEARN_CAMERA);assert(v.question_count==0);
    learning_model_delete(m);
    puts("PASS: identification never solves, explicit single/multiple selection, paging, cancellation, retry, settings and selected follow-up");
    return 0;
}
