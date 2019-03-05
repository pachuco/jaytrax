#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h> // for mixer stream
#include <mmsystem.h> // for mixer stream

#include "winmm.h"

//static guarantees clean initial state
#define MAXBUF 8
static WinmmCallBack callBack;
static DWORD semAudio, critDothings;
static char isAudioRunning;
static HANDLE hThread, hAudioSem;
static HWAVEOUT hWaveOut;
static WAVEFORMATEX wfx;
static int16_t *mixBuffer[MAXBUF];
static WAVEHDR header[MAXBUF];
static int bufCnt;

static int sampleRate;
static int bufferLength;
static int bufferNum;

//might want to refine these
#define SYNC_CRIT_INIT(x) x=0
#define SYNC_CRIT_ENTER(x) while(x) {SleepEx(1,1);} x=1
#define SYNC_CRIT_LEAVE(x) x=0
#define SYNC_SEM_INIT(x, initVal) x=initVal
#define SYNC_SEM_WAIT(x, max) while(x>=max) {SleepEx(1,1);} if(x < max) x++
#define SYNC_SEM_RELEASE(x) if (x > 0) x--


static DWORD WINAPI mixThread(LPVOID lpParam) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while(isAudioRunning) {
        SYNC_CRIT_ENTER(critDothings);
        callBack(mixBuffer[bufCnt], bufferLength, sampleRate);
        SYNC_CRIT_LEAVE(critDothings);
        
        waveOutPrepareHeader(hWaveOut, &header[bufCnt], sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header[bufCnt], sizeof(WAVEHDR));
        bufCnt = (bufCnt + 1) % bufferNum;
        
        SYNC_SEM_WAIT(semAudio, bufferNum);
    }
    return (0);
}

static void CALLBACK waveProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (uMsg == WOM_DONE) SYNC_SEM_RELEASE(semAudio);
}

void winmm_closeMixer() {
    int i;
    
    isAudioRunning = FALSE;
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = NULL;
    }
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

void winmm_enterCrit() {
    SYNC_CRIT_ENTER(critDothings);
}

void winmm_leaveCrit() {
    SYNC_CRIT_LEAVE(critDothings);
}

BOOL winmm_openMixer(WinmmCallBack cb, int freq, int buflen, int bufnum) {
    int i;
    DWORD threadID;
    LONG test;
    
    for (i = 0; i < bufferNum; ++i) header[i].dwUser = 0xFFFF;
    winmm_closeMixer();
    
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
        goto onError;
    }
    
    for (i = 0; i < bufferNum; i++) {
        mixBuffer[i] = (int16_t *)(calloc(bufferLength, wfx.nBlockAlign));
        if (mixBuffer[i] == NULL) goto onError;
        
        memset(&header[i], 0, sizeof(WAVEHDR));
        header[i].dwBufferLength = wfx.nBlockAlign * bufferLength;
        header[i].lpData = (LPSTR) mixBuffer[i];
        waveOutPrepareHeader(hWaveOut, &header[i], sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header[i], sizeof(WAVEHDR));
    }
    
    isAudioRunning = TRUE;
    SYNC_SEM_INIT(semAudio, bufferNum-1);
    SYNC_CRIT_INIT(critDothings);
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(mixThread), NULL, 0, &threadID);
    if (!hThread)   goto onError;
    
    return TRUE;
    
    onError:
        winmm_closeMixer();
        return FALSE;
}
