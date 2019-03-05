#define SAMPRATE 44100
#define MIX_BUF_SAMPLES 2048
#define MIX_BUF_NUM 2

#define WIN32_LEAN_AND_MEAN // for stripping windows.h include

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "jxs.h"
#include "jaytrax.h"

static JayPlayer* jay;
static Song* song;

BOOL getKeydownVK(DWORD* out) {
    DWORD iEvNum, dummy;
    INPUT_RECORD ir;
    static HANDLE hIn = NULL;
    
    if (!hIn) hIn = GetStdHandle(STD_INPUT_HANDLE);
    
    GetNumberOfConsoleInputEvents(hIn, &iEvNum);
    while (iEvNum) {
        ReadConsoleInput(hIn, &ir, 1, &dummy);
        if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
            if (out) *out = ir.Event.KeyEvent.wVirtualKeyCode;
            return TRUE;
        }
        iEvNum--;
    }
    return FALSE;
}

void clearScreen() {
   COORD coordScreen = { 0, 0 };
   DWORD cCharsWritten;
   CONSOLE_SCREEN_BUFFER_INFO csbi; 
   DWORD dwConSize;
   static HANDLE hOut = NULL;
   
   if (!hOut) hOut = GetStdHandle(STD_OUTPUT_HANDLE);
   if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;
   dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
   if (!FillConsoleOutputCharacter(hOut, (TCHAR) ' ', dwConSize, coordScreen, &cCharsWritten)) return;
   if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;
   if (!FillConsoleOutputAttribute(hOut, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten)) return;
   SetConsoleCursorPosition(hOut, coordScreen);
}

void updateDisplay() {
    if (!jay || !song) return;
    clearScreen();
    
}

void audioCB(int16_t* buf, int32_t numSamples, int32_t sampleRate) {
    jaytrax_renderChunk(jay, buf, numSamples, sampleRate);
}


int main(int argc, char* argv[]) {
    #define FAIL(x) {printf("%s\n", (x)); return 1;}
    
    if (argc != 2) {
        printf("Usage: jaytrax.exe <module>\n");
        return (1);
    }
    
    jay = jaytrax_init();
    
    if (jxsfile_loadSong(argv[1], &song)==0) {
		if (jaytrax_loadSong(jay, song)) {
			if (winmm_openMixer(&audioCB, SAMPRATE, MIX_BUF_SAMPLES, MIX_BUF_NUM)) {
                
				for (;;) {
                    DWORD vk;
                    
                    if (getKeydownVK(&vk)) {
                        if (vk == VK_ESCAPE) {
                            break;
                        } else if (vk == 0x4D) {
                            printf("farts\n");
                        }
                    }
                    
					SleepEx(1, 1);
					// do other stuff
				}
				winmm_closeMixer();
			} else FAIL("Cannot open mixer.");
		} else FAIL("Invalid song.")
    } else FAIL("Cannot load file.");
    
    return 0;
    #undef FAIL
}
