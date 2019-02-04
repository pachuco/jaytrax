#ifndef RESAMPLER_STUFF_H
#define RESAMPLER_STUFF_H

#include "../syntrax.h"

#if __STDC_VERSION__ >= 199901L
   #define INLINE inline
#else
   #define INLINE
#endif

int16_t interp_buf[16];

int16_t samp_pre_get(Voice *v, int off);
int16_t samp_pos_get(Voice *v, int off);
int16_t synth_bi_get(Voice *v, int off);

#endif
