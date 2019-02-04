#include "../syntrax.h"
#include "resampler.h"
#include "stuff.h"

#include <stdlib.h>
#include <stdint.h>



void resamp_linear_destroy(Resampler *r) {
    free(r);
}
void resamp_linear_changeRate(Resampler *r, int rate) {}


int32_t resamp_linear_interpSynt(Resampler *r, Voice *v) {
    int x;
    int16_t* p = &interp_buf[0];
    int frac;

	  frac = v->synthPos & 0xFF;
    
    p[0] = synth_bi_get(v, -1);
    p[1] = v->waveBuff[v->synthPos >> 8];
    
    x = p[0] * (0x100-frac) + p[1] * frac;
    return x>>8;
}

int32_t resamp_linear_interpSamp(Resampler *r, Voice *v) {
    int x;
    int16_t* p = &interp_buf[0];
    int frac;

	  frac = v->sampPos & 0xFF;
    
    p[0] = samp_pre_get(v, -1);
    p[1] = v->waveBuff[v->sampPos >> 8];
    
    x = p[0] * (0x100-frac) + p[1] * frac;
    return x>>8;
}

Resampler * resamp_linear_create(void) {
    Resampler *r = malloc(sizeof(Resampler));
    if(!r) return NULL;
    strcpy(&r->name, "linear");
    r->in  = 2;
    r->out = 1;
    
    r->destroy    = &resamp_linear_destroy;
    r->changeRate = &resamp_linear_changeRate;
    r->interpSynt = &resamp_linear_interpSynt;
    r->interpSamp = &resamp_linear_interpSamp;
    
    return r;
}
