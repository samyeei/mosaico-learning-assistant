// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "learning_viewport.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    learning_viewport_t v;
    assert(!learning_viewport_init(&v,1280,720,45));
    assert(!learning_viewport_init(&v,0,720,0));
    // Landmark positions for the actual 1280x720 sensor. These identify which
    // sensor edge becomes each displayed edge, and catch mirrors/wrong rotation.
    const unsigned first[4][2]={{833,0},{1279,704},{446,719},{0,15}};
    for(unsigned front=0;front<2;++front)for(unsigned r=0;r<4;++r){
        assert(learning_viewport_init_facing(&v,1280,720,r*90,front!=0));
        unsigned sx,sy;
        learning_viewport_pixel(&v,0,0,LEARN_PREVIEW_W,LEARN_PREVIEW_H,&sx,&sy);
        unsigned expected=(r+(front?2:0))%4;
        assert(sx==first[expected][0]&&sy==first[expected][1]);
        assert(abs((int)(v.w*LEARN_PREVIEW_H)-(int)(v.h*LEARN_PREVIEW_W))<LEARN_PREVIEW_W);
        for(unsigned y=0;y<LEARN_PREVIEW_H;++y){
            for(unsigned x=0;x<LEARN_PREVIEW_W;++x){
                unsigned px,py;
                learning_viewport_pixel(&v,x,y,LEARN_PREVIEW_W,LEARN_PREVIEW_H,&sx,&sy);
                learning_viewport_pixel(&v,2*x,2*y,LEARN_PHOTO_W,LEARN_PHOTO_H,&px,&py);
                assert(sx<1280&&sy<720&&sx==px&&sy==py);
            }
        }
    }
    puts("PASS: four screen orientations x two camera facings, crop landmarks and preview/photo field match");
    return 0;
}
