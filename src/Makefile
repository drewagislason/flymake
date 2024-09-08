#  Makefile for flymake
#  Copyright (c) 2024 Drew Gislason, All Rights Reserved.

UNAME=$(shell uname -s)

ifdef WINDIR
	HOSTENV=Cygwin
	HOSTFLAGS=
	HOST_LFLAGS=-limagehlp
else
	ifeq ($(UNAME),Darwin)
		HOSTENV=Mac
		HOSTFLAGS=
		HOST_LFLAGS=
	else
		HOSTENV=Linux
		HOSTFLAGS=
		HOST_LFLAGS= 
	endif
endif

# common dependancies
DEPS=../../flylibc/inc/Fly.h ../inc/flymake.h

# compile local and lib objs the same way
VPATH = .:../../flylibc/lib

# output folder
OUT=out

# include folders
INCLUDE=-I../inc -I../../flylibc/inc

# defines
DEFINES=

CC=cc
CCFLAGS=-Wall -Werror $(HOSTFLAGS) $(INCLUDE) -o
CFLAGS=-c $(DEFINES) $(CCFLAGS)
LFLAGS=$(HOST_LFLAGS) -o

$(OUT)/%.o: %.c $(DEPS)
	$(CC) $< $(CFLAGS) $@

OBJ_FLYMAKE = \
	$(OUT)/FlyAssert.o \
	$(OUT)/FlyCli.o \
	$(OUT)/FlyFile.o \
	$(OUT)/FlyFileList.o \
	$(OUT)/FlyList.o \
	$(OUT)/FlyMem.o \
	$(OUT)/FlySemVer.o \
	$(OUT)/FlySort.o \
	$(OUT)/FlyStrSmart.o \
	$(OUT)/FlyStr.o \
	$(OUT)/FlyStrZ.o \
	$(OUT)/FlyToml.o \
	$(OUT)/FlyUtf8.o \
	$(OUT)/flymake.o \
	$(OUT)/flymakeclean.o \
	$(OUT)/flymakedep.o \
	$(OUT)/flymakelist.o \
	$(OUT)/flymakenew.o \
	$(OUT)/flymakeprint.o \
	$(OUT)/flymakestate.o \
	$(OUT)/flymaketoml.o \
	$(OUT)/flymakeuserguide.o

.PHONY: clean mkout SayAll SayDone

all: SayAll mkout flymake SayDone

SayAll:
	@echo ------------------------------
	@echo Making flymake...
	@echo ------------------------------

SayDone:
	@echo ------------------------------
	@echo Created flymake
	@echo ------------------------------

flymake: mkout $(OBJ_FLYMAKE)
	$(CC) $(LFLAGS) $@ $(OBJ_FLYMAKE)
	@echo Linked $@ ...

# clean up files that don't need to be checked in to git
# "test_*" are test case executables, "tmp_*" are temporary test case data
clean:
	rm -rf out/
	rm -f *.log
	rm -f flymake
	rm -f tmp.*

# make the out folder
mkout:
	-if test ! -d $(OUT); then mkdir $(OUT); fi

flymakeuserguide.c: ../docs/flymake_user_guide.md
	file2c ../docs/flymake_user_guide.md flymakeuserguide.c g_szFlyMakeUserGuide
