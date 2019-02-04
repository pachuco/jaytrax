#include "stuff.h"

int16_t interp_buf[16];

int16_t samp_pre_get(Voice *v, int off) {
    int t;
    int16_t a;
    int16_t *dat = v->waveBuff;
    int pos = v->sampPos >> 8;
    uint strt    = v->smpLoopStart >> 8;
    uint end     = v->smpLoopEnd >> 8;
    uint loopLen = end - strt;
    
    t = pos + off;
    
    if ( v->isPlayingBackward ) {
        if ( t < strt ) {
            t = end - (strt - t) % loopLen;
        }
        a = dat[t];
    } else {
        if ( t < strt ) {
            if ( v->hasLooped ) {
                if ( v->hasBidiLoop) {
                    t = strt + (strt - t) % loopLen;
                } else {
                    t = end - (strt - t) % loopLen;
                }
                a = dat[t];
            } else {
                if ( t >= 0 ) {
                    a = dat[t];
                } else {
                    a = 0;
                }
            }
        } else {
            a = dat[t];
        }
    }
    return a;
}

int16_t samp_pos_get(Voice *v, int off) {
    int t;
    int16_t a;
    int16_t *dat = v->waveBuff;
    int pos = v->sampPos >> 8;
    uint strt    = v->smpLoopStart >> 8;
    uint end     = v->smpLoopEnd >> 8;
    uint loopLen = end - strt;
    
    t = pos + off;
    
    if ( v->isPlayingBackward ) {
        if ( t >= end ) {
            t = end - (t - end) % loopLen - 1;
        }
        a = dat[t];
    } else {
        if ( t >= end ) {
            if ( v->hasLoop ) {
                if ( v->hasBidiLoop) {
                    t = end - (t - end) % loopLen - 1;
                } else {
                    t = strt + (t - end) % loopLen - 1;
                }
                a = dat[t];
            } else {
                a = 0;
            }
        } else {
            a = dat[t];
        }
    }
    return a;
}

int16_t synth_bi_get(Voice *v, int off) {
    int16_t *dat = v->waveBuff;
    int pos = v->synthPos >> 8;
    int wl  = v->wavelength >> 8;
    
    return dat[(pos + off) & wl];
}
