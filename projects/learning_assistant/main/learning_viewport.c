// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_viewport.h"
#include <stddef.h>
bool learning_viewport_init_facing(learning_viewport_t *v,unsigned sw,unsigned sh,unsigned rotation,bool flipped)
{
    if(rotation>270||rotation%90)return false;
    return learning_viewport_init(v,sw,sh,(rotation+(flipped?180U:0U))%360U);
}

bool learning_viewport_init(learning_viewport_t *v,unsigned sw,unsigned sh,unsigned rotation)
{
    if(!v||!sw||!sh||rotation>270||rotation%90)return false;
    // BSP panel rotation is clockwise. Counter-rotate camera content within
    // the logical UI, in addition to its fixed CCW 90-degree mounting correction.
    unsigned turns=(1+rotation/90)%4;
    unsigned w=turns%2?sh:sw,h=turns%2?sw:sh;
    *v=(learning_viewport_t){.sensor_w=sw,.sensor_h=sh,.ccw_turns=turns,.w=w,.h=h};
    if((uint64_t)w*LEARN_PREVIEW_H>(uint64_t)h*LEARN_PREVIEW_W)
        v->w=(unsigned)((uint64_t)h*LEARN_PREVIEW_W/LEARN_PREVIEW_H);
    else v->h=(unsigned)((uint64_t)w*LEARN_PREVIEW_H/LEARN_PREVIEW_W);
    v->x=(w-v->w)/2;v->y=(h-v->h)/2;
    return v->w&&v->h;
}
void learning_viewport_pixel(const learning_viewport_t *v,unsigned x,unsigned y,
                            unsigned width,unsigned height,unsigned *sx,unsigned *sy)
{
    unsigned ox=v->x+(unsigned)((uint64_t)x*v->w/width);
    unsigned oy=v->y+(unsigned)((uint64_t)y*v->h/height);
    switch(v->ccw_turns){
    case 1:*sx=v->sensor_w-1-oy;*sy=ox;break;
    case 2:*sx=v->sensor_w-1-ox;*sy=v->sensor_h-1-oy;break;
    case 3:*sx=oy;*sy=v->sensor_h-1-ox;break;
    default:*sx=ox;*sy=oy;break;
    }
}
