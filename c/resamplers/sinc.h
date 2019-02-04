#ifndef RESAMPLER_SINC_H
#define RESAMPLER_SINC_H

#include "../syntrax.h"
#include "resampler.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

Resampler * resamp_sinc_create(void);

#ifdef __cplusplus
}
#endif

#endif
