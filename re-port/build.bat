@echo off
set gccbase=G:\p_files\rtdk\mingw32-gcc5
set fbcbase=G:\p_files\rtdk\FBC

set opts=-std=c99 -mconsole -Os -s -Wall -Wextra
set link=-lwinmm
set outname=jaytrax

set PATH=%PATH%;%gccbase%\bin;%fbcbase%
del %outname%.exe
gcc -o %outname%.exe main.c jxs.c jaytrax.c %opts% %link% 2> err.log

%outname%.exe bumpnburn(8).jxs
pause