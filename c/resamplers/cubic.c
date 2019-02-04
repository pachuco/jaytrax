#include "../syntrax.h"
#include "resampler.h"
#include "stuff.h"

#include <stdlib.h>
#include <stdint.h>



void resamp_cubic_destroy(Resampler *r) {
    free(r);
}
void resamp_cubic_changeRate(Resampler *r, int rate) {}


int32_t resamp_cubic_interpSynt(Resampler *r, Voice *v) {
    int x;
    int16_t* p = &interp_buf[0];
    //int frac;
    double frac;

	//frac = (v->synthPos & 0xFF);
  frac = (double)(v->synthPos & 0xFF)/255;
    
    p[0] = synth_bi_get(v, -1);
    p[1] = v->waveBuff[v->synthPos >> 8];
    p[2] = synth_bi_get(v, 1);
    p[3] = synth_bi_get(v, 2);
    
    x = p[1] + 0.5 * frac*(p[2] - p[0] + frac*(2.0*p[0] - 5.0*p[1] + 4.0*p[2] - p[3] + frac*(3.0*(p[1] - p[2]) + p[3] - p[0])));
    return x;
}

int32_t resamp_cubic_interpSamp(Resampler *r, Voice *v) {
    int x;
    int16_t* p = &interp_buf[0];
    double frac;

	frac = (double)(v->sampPos & 0xFF)/255;
    
    p[0] = samp_pre_get(v, -1);
    p[1] = v->waveBuff[v->sampPos >> 8];
    p[2] = samp_pos_get(v, 1);
    p[3] = samp_pos_get(v, 2);
    
    x = p[1] + 0.5 * frac*(p[2] - p[0] + frac*(2.0*p[0] - 5.0*p[1] + 4.0*p[2] - p[3] + frac*(3.0*(p[1] - p[2]) + p[3] - p[0])));
    //x = 1.5*(-0.333333*p[0]*frac*frac*frac + 0.666667*p[0]*frac*frac - 0.333333*p[0]*frac + p[1]*frac*frac*frac - 1.66667*p[1]*frac*frac + 0.666667*p[1] - p[2]*frac*frac*frac + 1.33333*p[2]*frac*frac + 0.333333*p[2]*frac + 0.333333*p[3]*frac*frac*frac - 0.333333*p[3]*frac*frac);
    return x;
}

Resampler * resamp_cubic_create(void) {
    Resampler *r = malloc(sizeof(Resampler));
    if(!r) return NULL;
    strcpy(&r->name, "cubic");
    r->in  = 4;
    r->out = 1;
    
    r->destroy    = &resamp_cubic_destroy;
    r->changeRate = &resamp_cubic_changeRate;
    r->interpSynt = &resamp_cubic_interpSynt;
    r->interpSamp = &resamp_cubic_interpSamp;
    
    return r;
}
