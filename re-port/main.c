#define SAMPRATE 44100
#define MIX_BUF_SAMPLES 4096
#define MIX_BUF_NUM 2

#define WIN32_LEAN_AND_MEAN // for stripping windows.h include

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#include <windows.h> // for mixer stream
#include <mmsystem.h> // for mixer stream

#include "jxs.h"
#include "jaytrax.h"

static volatile char isAudioRunning;
static HANDLE hThread;
static HWAVEOUT hWaveOut; // Device handle
static WAVEFORMATEX wfx;
static int16_t *mixBuffer[MIX_BUF_NUM];
static WAVEHDR header[MIX_BUF_NUM];
static int currBuf;

static JayPlayer* jay;

DWORD WINAPI mixThread(LPVOID lpParam) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while(isAudioRunning) {
        while (header[currBuf].dwFlags & WHDR_DONE) {
			//move this into main thread somehow
            jaytrax_renderChunk(jay, mixBuffer[currBuf], MIX_BUF_SAMPLES, SAMPRATE);
            
            waveOutPrepareHeader(hWaveOut, &header[currBuf], sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header[currBuf], sizeof(WAVEHDR));
            currBuf = (currBuf + 1) % MIX_BUF_NUM;
        }
        SleepEx(1, 1);
    }
    return (0);
}

void closeMixer(void) {
    int i;
    
    isAudioRunning = FALSE;
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = NULL;
    }
    if (hWaveOut) {
        waveOutReset(hWaveOut);
        for (i = 0; i < MIX_BUF_NUM; ++i) {
            if (header[i].dwUser != 0xFFFF) waveOutUnprepareHeader(hWaveOut, &header[i], sizeof (WAVEHDR));
        }
        waveOutClose(hWaveOut);
        for (i = 0; i < MIX_BUF_NUM; ++i) {
            if (mixBuffer[i] != NULL) {
                free(mixBuffer[i]);
                mixBuffer[i] = NULL;
            }
        }
    }
    hWaveOut = NULL;
    currBuf = 0;
}

BOOL openMixer(uint32_t outputFreq) {
    int i;
    DWORD threadID;

    for (i = 0; i < MIX_BUF_NUM; ++i) header[i].dwUser = 0xFFFF;
    closeMixer();

    wfx.nSamplesPerSec = outputFreq;
    wfx.wBitsPerSample = 16;
    wfx.nChannels = 2;

    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels) / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return FALSE;
    }

    for (i = 0; i < MIX_BUF_NUM; i++) {
        mixBuffer[i] = (int16_t *)(calloc(MIX_BUF_SAMPLES, wfx.nBlockAlign));
        if (mixBuffer[i] == NULL) goto onError;
        
        memset(&header[i], 0, sizeof(WAVEHDR));
        header[i].dwBufferLength = wfx.nBlockAlign * MIX_BUF_SAMPLES;
        header[i].lpData = (LPSTR) mixBuffer[i];
        waveOutPrepareHeader(hWaveOut, &header[i], sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header[i], sizeof(WAVEHDR));
    }
    isAudioRunning = TRUE;
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(mixThread), NULL, 0, &threadID);
    if (!hThread) return FALSE;
    
    return TRUE;
    
    onError:
        closeMixer();
        return FALSE;
}

int main(int argc, char* argv[]) {
    #define FAIL(x) {printf("%s\n", (x)); return 1;}
    Song* song = NULL;
    
    if (argc != 2) {
        printf("Usage: jaytrax.exe <module>\n");
        return (1);
    }
    
    jay = jaytrax_init();
    
    if (jxsfile_loadSong(argv[1], &song)==0) {
		if (jaytrax_loadSong(jay, song)) {
			if (openMixer(SAMPRATE)) {
				printf("Press any key to stop.\n");
				for (;;) {
					if (kbhit()) break;
					SleepEx(1, 1);
					// do other stuff
				}
				closeMixer();
			} else FAIL("Cannot open mixer.");
		} else FAIL("Invalid song.")
    } else FAIL("Cannot load file.");
    
    return 0;
    #undef FAIL
}
