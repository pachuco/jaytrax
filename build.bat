@echo off
set gccbase=G:\p_files\rtdk\mingw32-gcc5
set fbcbase=G:\p_files\rtdk\FBC

set opts=-std=c99 -mconsole -Os -s -Wall -Wextra
set link=-lwinmm
set outname=oldjaytrax_cli
set bin=bin

set ljayold=lib_oldjaytrax
set c_liboldjay=%ljayold%\jxs.c %ljayold%\jaytrax.c %ljayold%\mixcore.c
set clijayold=cliplayer_oldjaytrax
set c_clioldjay=%clijayold%\main.c %clijayold%\winmmout.c

set includes=-I%ljayold% -I%clijayold%
set compiles=%c_clioldjay% %c_liboldjay%

set PATH=%PATH%;%gccbase%\bin;%fbcbase%

del %bin%\%outname%.exe
gcc -o %bin%\%outname%.exe %includes% %compiles% %opts% %link% 2> %outname%_err.log
IF %ERRORLEVEL% NEQ 0 pause

::%outname%.exe 090301.jxs
::pause