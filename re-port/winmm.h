#ifndef WINMM_H
#define WINMM_H

#include <windows.h>
#include <stdint.h>

typedef void (*WinmmCallBack)(int16_t* buf, int numSamples, int sampleRate);

void winmm_closeMixer(void);
BOOL winmm_openMixer(WinmmCallBack cb, int freq, int buflen, int bufnum);

#endif