#include "../syntrax.h"
#include "resampler.h"
#include "stuff.h"

#include <stdlib.h>
#include <stdint.h>


void resamp_nearest_destroy(Resampler *r) {
    free(r);
}
void resamp_nearest_changeRate(Resampler *r, int rate) {}


int32_t resamp_nearest_interpSynt(Resampler *r, Voice *v) {
    return v->waveBuff[v->synthPos >> 8];
}

int32_t resamp_nearest_interpSamp(Resampler *r, Voice *v) {
    return v->waveBuff[v->sampPos >> 8];
}

Resampler * resamp_nearest_create(void) {
    Resampler *r = malloc(sizeof(Resampler));
    if(!r) return NULL;
    strcpy(&r->name, "nearest");
    r->in  = 1;
    r->out = 1;
    
    r->destroy    = &resamp_nearest_destroy;
    r->changeRate = &resamp_nearest_changeRate;
    r->interpSynt = &resamp_nearest_interpSynt;
    r->interpSamp = &resamp_nearest_interpSamp;
    
    return r;
}
