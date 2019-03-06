#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <windows.h> // for mixer stream
#include <mmsystem.h> // for mixer stream

#include "winmm.h"

//static guarantees clean initial state
#define MAXBUF 8
static char isAudioRunning;
static HANDLE hThread, hAudioSem;
static CRITICAL_SECTION critDothings;
static WinmmCallBack callBack;
static HWAVEOUT hWaveOut;
static WAVEFORMATEX wfx;
static int16_t *mixBuffer[MAXBUF];
static WAVEHDR header[MAXBUF];
static int bufCnt;

static int sampleRate;
static int bufferLength;
static int bufferNum;

static DWORD WINAPI mixThread(LPVOID lpParam) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while(isAudioRunning) {
        if(TryEnterCriticalSection(&critDothings)) {
            callBack(mixBuffer[bufCnt], bufferLength, sampleRate);
            LeaveCriticalSection(&critDothings);
            
            waveOutPrepareHeader(hWaveOut, &header[bufCnt], sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header[bufCnt], sizeof(WAVEHDR));
            bufCnt = (bufCnt + 1) % bufferNum;
        }
        WaitForSingleObject(hAudioSem, INFINITE);
    }
    return (0);
}

static void CALLBACK waveProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (uMsg == WOM_DONE) ReleaseSemaphore(hAudioSem, 1, NULL);
}

void winmm_enterCrit() {
    EnterCriticalSection(&critDothings);
}

void winmm_leaveCrit() {
    LeaveCriticalSection(&critDothings);
}

void winmm_closeMixer() {
    int i;
    
    isAudioRunning = FALSE;
    if (hThread) {
        ReleaseSemaphore(hAudioSem, 1, NULL);
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = NULL;
    }
    if (hAudioSem) {
        CloseHandle(hAudioSem);
        hAudioSem = NULL;
    }
    DeleteCriticalSection(&critDothings);
    if (hWaveOut) {
        waveOutReset(hWaveOut);
        for (i = 0; i < bufferNum; ++i) {
            if (header[i].dwUser != 0xFFFF) waveOutUnprepareHeader(hWaveOut, &header[i], sizeof (WAVEHDR));
        }
        waveOutClose(hWaveOut);
        for (i = 0; i < bufferNum; ++i) {
            if (mixBuffer[i] != NULL) {
                free(mixBuffer[i]);
                mixBuffer[i] = NULL;
            }
        }
    }
    callBack = NULL;
    hWaveOut = NULL;
    bufCnt = 0;
}

BOOL winmm_openMixer(WinmmCallBack cb, int freq, int buflen, int bufnum) {
    int i;
    DWORD threadID;
    LONG test;
    
    for (i = 0; i < bufferNum; ++i) header[i].dwUser = 0xFFFF;
    winmm_closeMixer();
    
    assert(bufnum <= MAXBUF);
	callBack = cb;
    sampleRate = freq;
    bufferLength = buflen;
    bufferNum = bufnum;
    
    wfx.nSamplesPerSec = freq;
    wfx.wBitsPerSample = 16;
    wfx.nChannels = 2;

    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels) / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)(&waveProc), 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        goto _ERR;
    }
    
    for (i = 0; i < bufferNum; i++) {
        mixBuffer[i] = (int16_t *)(calloc(bufferLength, wfx.nBlockAlign));
        if (mixBuffer[i] == NULL) goto _ERR;
        
        memset(&header[i], 0, sizeof(WAVEHDR));
        header[i].dwBufferLength = wfx.nBlockAlign * bufferLength;
        header[i].lpData = (LPSTR) mixBuffer[i];
        waveOutPrepareHeader(hWaveOut, &header[i], sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header[i], sizeof(WAVEHDR));
    }
    
    isAudioRunning = TRUE;
    hAudioSem = CreateSemaphore(NULL, bufferNum - 1, bufferNum, NULL);
    if (!hAudioSem) goto _ERR;
    InitializeCriticalSection(&critDothings);
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(mixThread), NULL, 0, &threadID);
    if (!hThread) goto _ERR;
    
    return TRUE;
    
    _ERR:
        winmm_closeMixer();
        return FALSE;
}
