@echo off
set gccbase=G:\p_files\rtdk\mingw32-gcc5
set fbcbase=G:\p_files\rtdk\FBC

set opts=-std=c99 -mconsole -Os -s -Wall -Wextra
set link=

set PATH=%PATH%;%gccbase%\bin;%fbcbase%
del test.exe
gcc -o test.exe main.c jxs.c ioutil.c %opts% %link% 2> err.log

test.exe
pause