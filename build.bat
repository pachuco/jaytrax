@echo off

set gccbase=G:\p_files\rtdk\i686-8.1.0-win32-dwarf-rt_v6-rev0\mingw32
set PATH=%PATH%;%gccbase%\bin

set opts=-std=c99 -mconsole -Os -s -Wall -Wextra
set link=-lwinmm
set bin=bin

set ljayold=lib_oldjaytrax
set c_liboldjay=%ljayold%\jxs.c %ljayold%\jaytrax.c %ljayold%\mixcore.c
set cli=clitools
set c_cli=%cli%\main.c %cli%\winmmout.c

set includes=-I%ljayold% -I%cli%
set compiles=%c_cli% %c_liboldjay%


set outname=oldjaytrax_cli
del %bin%\%outname%.exe
gcc -o %bin%\%outname%.exe %includes% %compiles% %opts% %link% 2> %outname%_err.log
IF %ERRORLEVEL% NEQ 0 (
    echo oops %outname%!
    pause
)