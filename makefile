# Compiler and linker specific configuration macros and flags.
# ============================================================

CC = gcc

CFLAG = -std=c99 -mconsole -Os -s -Wall -Wextra
LDFLAG = -lwinmm

BUILD_PATH = target-build_binary

INCLUDE_PATH = -I../lib_oldjaytrax
MODULE_PATH = ../lib_oldjaytrax

INCLUDE = ioutil.h mixcore.h jaytrax.h jxs.h
MODULES = jaytrax.c mixcore.c jxs.c
OBJS = jaytrax.o mixcore.o jxs.o

SRC = $(MODULES:%.c=$(MODULE_PATH)/%.c)

# Macros for clitools generation.
# =================================================
EXE_NAME = jaytrax_bin32.exe

EXECUTABLE_INCLUDE_PATH = -I../clitools
EXECUTABLE_MODULE_PATH = ../clitools

EXECUTABLE_INCLUDE_INCLUDE_PATH = winmmout.h
EXECUTABLE_MODULES = winmmout.c main.c
EXECUTABLE_OBJECTS = winmmout.o main.o

EXE_SRC = $(EXECUTABLE_MODULES:%.c=$(EXECUTABLE_MODULE_PATH)/%.c)

# Commands and dependencies in declarative clauses.	
# =================================================

all: build

build: compile link

compile: 
	cd $(BUILD_PATH) && $(CC) $(INCLUDE_PATH) $(EXECUTABLE_INCLUDE_PATH) $(CFLAG) -c $(SRC) $(EXE_SRC)

link:
	cd $(BUILD_PATH) && $(CC) $(OBJS) $(EXECUTABLE_OBJECTS) $(LDFLAG) -o $(EXE_NAME)
	
install:
	echo Not yet implemented.
     
clean:
	 rm $(BUILD_PATH) *.exe
	 rm $(BUILD_PATH) *.o
	 
# Need to handle change detection per object file. TODO.
	 