@echo off
set gccbase=G:\p_files\rtdk\mingw32-gcc5
set fbcbase=G:\p_files\rtdk\FBC

set opts=-std=c99 -mconsole -Os -s -Wall -Wextra
set link=-lwinmm
set outname=jaytrax-orig

set PATH=%PATH%;%gccbase%\bin;%fbcbase%
del %outname%.exe
gcc -o %outname%.exe main.c winmm.c jxs.c jaytrax.c mixcore.c %opts% %link% 2> err.log
IF %ERRORLEVEL% NEQ 0 pause

::%outname%.exe 090301.jxs
::pause