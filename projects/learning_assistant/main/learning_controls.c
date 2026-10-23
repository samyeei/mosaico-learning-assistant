// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_controls.h"
learning_controls_t learning_controls_defaults(void)
{return (learning_controls_t){.brightness=80,.volume=50,.speech_enabled=true,.automatic=true,.grade=LEARN_GRADE_DEFAULT};}
const char *learning_grade_name(unsigned grade)
{
    static const char *const names[]={"一年级","二年级","三年级","四年级","五年级","六年级","初一","初二","初三","高一","高二","高三"};
    return names[(grade>=LEARN_GRADE_MIN&&grade<=LEARN_GRADE_MAX?grade:LEARN_GRADE_DEFAULT)-1];
}
bool learning_controls_change(learning_controls_t *v,learning_control_t c,int value,unsigned rotation)
{
    if(!v)return false;
    switch(c){
    case LEARN_CONTROL_BRIGHTNESS:if(value<10||value>100)return false;v->brightness=(unsigned)value;break;
    case LEARN_CONTROL_VOLUME:if(value<0||value>100)return false;v->volume=(unsigned)value;break;
    case LEARN_CONTROL_SPEECH:if(value!=0&&value!=1)return false;v->speech_enabled=value!=0;break;
    case LEARN_CONTROL_GRADE:if(value<LEARN_GRADE_MIN||value>LEARN_GRADE_MAX)return false;v->grade=(unsigned)value;break;
    case LEARN_CONTROL_AUTO:
        if((value!=0&&value!=1)||rotation>270||rotation%90)return false;
        v->automatic=value!=0;if(!v->automatic)v->fixed_rotation=rotation;break;
    default:return false;
    }
    return true;
}
uint32_t learning_controls_pack(const learning_controls_t *v)
{return 0xa5000000U|v->brightness|(v->volume<<7)|((uint32_t)v->speech_enabled<<14)|
    ((uint32_t)v->automatic<<15)|((v->fixed_rotation/90)<<16)|(v->grade<<18);}
bool learning_controls_unpack(uint32_t p,learning_controls_t *v)
{
    if(!v||(p&0xffc00000U)!=0xa5000000U)return false;
    learning_controls_t n={.brightness=p&127U,.volume=(p>>7)&127U,
        .speech_enabled=(p&(1U<<14))!=0,.automatic=(p&(1U<<15))!=0,.fixed_rotation=((p>>16)&3U)*90};
    n.grade=(p>>18)&15U;if(!n.grade)n.grade=LEARN_GRADE_DEFAULT;
    if(n.brightness<10||n.brightness>100||n.volume>100||n.grade>LEARN_GRADE_MAX)return false;
    *v=n;return true;
}
