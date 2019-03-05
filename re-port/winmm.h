#ifndef WINMM_H
#define WINMM_H

#include <windows.h>
#include <stdint.h>

typedef void (*WinmmCallBack)(int16_t* buf, int numSamples, int sampleRate);

void winmm_closeMixer();
BOOL winmm_openMixer(WinmmCallBack cb, int freq, int buflen, int bufnum);
void winmm_enterCrit();
void winmm_leaveCrit();

#endif