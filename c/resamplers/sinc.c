#include "../syntrax.h"
#include "resampler.h"
#include "stuff.h"

#include <stdlib.h>
#include <stdint.h>


//http://musicdsp.org/showone.php?id=212

#define FP_SHIFT       8
#define FP_ONE         (1 << (FP_SHIFT))
#define FP_MASK        ((FP_ONE) - 1)
// 16 points
#define POINT_SHIFT    4
//16x oversampling
#define OVER_SHIFT     4

#define POINTS         (1 << (POINT_SHIFT))
#define INTERP_SHIFT   ((FP_SHIFT) - (OVER_SHIFT))
#define INTERP_BITMASK ((1 << (INTERP_SHIFT)) - 1)
        
        
int16_t sinc_table[272] = {
     0, -7,  27, -71,  142, -227,  299,  32439,   299,  -227,  142,  -71,  27,  -7,  0,  0,
     0,  0,  -5,  36, -142,  450, -1439, 32224,  2302,  -974,  455, -190,  64, -15,  2,  0,
     0,  6, -33, 128, -391, 1042, -2894, 31584,  4540, -1765,  786, -318, 105, -25,  3,  0,
     0, 10, -55, 204, -597, 1533, -4056, 30535,  6977, -2573, 1121, -449, 148, -36,  5,  0,
    -1, 13, -71, 261, -757, 1916, -4922, 29105,  9568, -3366, 1448, -578, 191, -47,  7,  0,
    -1, 15, -81, 300, -870, 2185, -5498, 27328, 12263, -4109, 1749, -698, 232, -58,  9,  0,
    -1, 15, -86, 322, -936, 2343, -5800, 25249, 15006, -4765, 2011, -802, 269, -68, 10,  0,
    -1, 15, -87, 328, -957, 2394, -5849, 22920, 17738, -5298, 2215, -885, 299, -77, 12,  0,
     0, 14, -83, 319, -938, 2347, -5671, 20396, 20396, -5671, 2347, -938, 319, -83, 14,  0,
     0, 12, -77, 299, -885, 2215, -5298, 17738, 22920, -5849, 2394, -957, 328, -87, 15, -1,
     0, 10, -68, 269, -802, 2011, -4765, 15006, 25249, -5800, 2343, -936, 322, -86, 15, -1,
     0,  9, -58, 232, -698, 1749, -4109, 12263, 27328, -5498, 2185, -870, 300, -81, 15, -1,
     0,  7, -47, 191, -578, 1448, -3366,  9568, 29105, -4922, 1916, -757, 261, -71, 13, -1,
     0,  5, -36, 148, -449, 1121, -2573,  6977, 30535, -4056, 1533, -597, 204, -55, 10,  0,
     0,  3, -25, 105, -318,  786, -1765,  4540, 31584, -2894, 1042, -391, 128, -33,  6,  0,
     0,  2, -15,  64, -190,  455,  -974,  2302, 32224, -1439,  450, -142,  36,  -5,  0,  0,
     0,  0,  -7,  27,  -71,  142,  -227,   299, 32439,   299, -227,  142, -71,  27, -7,  0
};


void resamp_sinc_destroy(Resampler *r) {
    free(r);
}
void resamp_sinc_changeRate(Resampler *r, int rate) {}


int32_t resamp_sinc_interpSynt(Resampler *r, Voice *v) {
    int x;
    int16_t* p = &interp_buf[0];
    int frac;
    int t1, t2;
    int a1, a2, b1;
    int16_t *tb = &sinc_table;

	  frac = v->synthPos & 0xFF;
	  t1 = (frac >> INTERP_SHIFT) << POINT_SHIFT;
    t2 = t1 + POINTS;
    
    p[ 0] = synth_bi_get(v, -8);
    p[ 1] = synth_bi_get(v, -7);
    p[ 2] = synth_bi_get(v, -6);
    p[ 3] = synth_bi_get(v, -5);
    p[ 4] = synth_bi_get(v, -4);
    p[ 5] = synth_bi_get(v, -3);
    p[ 6] = synth_bi_get(v, -2);
    p[ 7] = synth_bi_get(v, -1);
    p[ 8] = v->waveBuff[v->synthPos >> 8];
    p[ 9] = synth_bi_get(v,  1);
    p[10] = synth_bi_get(v,  2);
    p[11] = synth_bi_get(v,  3);
    p[12] = synth_bi_get(v,  4);
    p[13] = synth_bi_get(v,  5);
    p[14] = synth_bi_get(v,  6);
    p[15] = synth_bi_get(v,  7);
    
    a1 = 0;
    a2 = 0;
    b1 = p[ 0];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 1];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 2];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 3];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 4];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 5];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 6];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 7];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 8];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 9];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[10];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[11];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[12];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[13];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[14];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[15];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    
    x = a1 + ( ( a2 - a1 ) * ( frac & INTERP_BITMASK ) >> INTERP_SHIFT );
    return x >> (FP_SHIFT - 1);
}

int32_t resamp_sinc_interpSamp(Resampler *r, Voice *v) {
    int x;
    int16_t* p = &interp_buf[0];
    int frac;
    int t1, t2;
    int a1, a2, b1;
    int16_t *tb = &sinc_table;

	  frac = v->sampPos & 0xFF;
	  t1 = (frac >> INTERP_SHIFT) << POINT_SHIFT;
    t2 = t1 + POINTS;
    
    p[ 0] = samp_pre_get(v, -8);
    p[ 1] = samp_pre_get(v, -7);
    p[ 2] = samp_pre_get(v, -6);
    p[ 3] = samp_pre_get(v, -5);
    p[ 4] = samp_pre_get(v, -4);
    p[ 5] = samp_pre_get(v, -3);
    p[ 6] = samp_pre_get(v, -2);
    p[ 7] = samp_pre_get(v, -1);
    p[ 8] = v->waveBuff[v->sampPos >> 8];
    p[ 9] = samp_pos_get(v,  1);
    p[10] = samp_pos_get(v,  2);
    p[11] = samp_pos_get(v,  3);
    p[12] = samp_pos_get(v,  4);
    p[13] = samp_pos_get(v,  5);
    p[14] = samp_pos_get(v,  6);
    p[15] = samp_pos_get(v,  7);
    
    a1 = 0;
    a2 = 0;
    b1 = p[ 0];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 1];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 2];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 3];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 4];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 5];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 6];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 7];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 8];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[ 9];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[10];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[11];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[12];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[13];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[14];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    b1 = p[15];
    a1 += tb[ t1++ ] * b1 >> FP_SHIFT;
    a2 += tb[ t2++ ] * b1 >> FP_SHIFT;
    
    x = a1 + ( ( a2 - a1 ) * ( frac & INTERP_BITMASK ) >> INTERP_SHIFT );
    return x >> (FP_SHIFT - 1);
}

Resampler * resamp_sinc_create(void) {
    Resampler *r = malloc(sizeof(Resampler));
    if(!r) return NULL;
    strcpy(&r->name, "half-decent sinc");
    r->in  = 16;
    r->out = 1;
    
    r->destroy    = &resamp_sinc_destroy;
    r->changeRate = &resamp_sinc_changeRate;
    r->interpSynt = &resamp_sinc_interpSynt;
    r->interpSamp = &resamp_sinc_interpSamp;
    
    return r;
}
