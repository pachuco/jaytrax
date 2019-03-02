#ifndef MIXCORE_H
#define MIXCORE_H
#include <stdint.h>
#include "jaytrax.h"

#define MAX_TAPS (32)

#define CLAMP(x, low, high) (x = ((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
//#define CLAMP16(x) if ((int16_t)(x) != x) x = 0x7FFF ^ (x >> 31) //fast 32 -> 16 bit clamp
#define CLAMP16(x) (int16_t)(x) != x ? 0x7FFF ^ (x >> 31) : x //fast 32 -> 16 bit clamp
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void jaymix_mixCore(JayPlayer* THIS, int16_t numSamples);

#endif