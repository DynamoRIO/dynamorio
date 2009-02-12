# **********************************************************
# Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
# **********************************************************

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# 
# * Neither the name of VMware, Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

.PHONY: all clean

ifeq ($(shell uname -s | cut -c1-5), Linux)
MACHINE = linux
else
MACHINE = win32
endif

ifndef DYNAMORIO_MAKE
  DYNAMORIO_MAKE := ../../make
endif
include $(DYNAMORIO_MAKE)/compiler.mk

# windows vs. linux flags, etc.
ifeq ($(MACHINE), linux)
U       =-u 
I       =-I
D       =-D
L       =-L
INCR    =-c
OUT     =-o 
EXEOUT  =-o 
WARN    =-Wall
OPT     =-O3
NOPT    =-O0
DBG     =-g3
LD	= /usr/bin/ld
OBJ_SFX =o
EXE_SFX =
LIB_NAME =libdrprivate.so
else # win32
# avoid problems w/ line endings on #! files that cygwin's sh has
SHELL   =bash
U       =/U
I       =/I
D       =/D
L       =/link /LIBPATH:
INCR    =/c
OUT     =/Fo
EXEOUT  =/Fe
DLLOUT  =/dll /out:
LIBOUT  =/implib:
WARN    =/W3
OPT     =/O2
NOPT    =/Od
DBG     =/Zi
LD      = link /nologo
OBJ_SFX =obj
EXE_SFX =.exe
LIB_NAME =dynamorio.dll
endif

ifndef DYNAMORIO_HOME
DYNAMORIO_HOME := ../../exports
endif

# use release build if it exists, unless DEBUG=yes
DYNAMORIO_DIR_BASE :=$(DYNAMORIO_HOME)
ifeq ($(ARCH), x64)
  LIB_SUBDIR :=lib64
  ARCH_FLAGS := $(D)X86_64
else
  LIB_SUBDIR :=lib32
  ARCH_FLAGS := $(D)X86_32
endif

# allow yes or 1 for DEBUG
ifeq ($(DEBUG),yes)
RELEASE_EXISTS:=""
else
ifeq ($(DEBUG),1)
RELEASE_EXISTS:=""
else
RELEASE_EXISTS:=$(wildcard $(DYNAMORIO_DIR_BASE)/$(LIB_SUBDIR)/release)
endif
endif

ifeq ($(RELEASE_EXISTS), $(DYNAMORIO_DIR_BASE)/$(LIB_SUBDIR)/release)
DYNAMORIO_DIR := $(DYNAMORIO_DIR_BASE)/$(LIB_SUBDIR)/release
else
DYNAMORIO_DIR := $(DYNAMORIO_DIR_BASE)/$(LIB_SUBDIR)/debug
endif

DYNAMORIO_INCDIR := $(DYNAMORIO_DIR_BASE)/include

ifeq ($(MACHINE), linux)
DYNAMORIO_INC = $(I)$(DYNAMORIO_INCDIR)
else
DYNAMORIO_INC = $(I)$(subst /,\\,$(shell cygpath -m ${DYNAMORIO_INCDIR})) $(MSINCLUDE)
DYNAMORIO_LIBDIR = $(subst /,\\,$(shell cygpath -m ${DYNAMORIO_DIR})) $(MSLIB)
endif

CC_BASE = $(CC) $(DBG) $I.

# some apps need LINUX defined even if not using API
ifeq ($(MACHINE), win32)
# don't need to define WIN32, assumed by ifndef LINUX
CC_BASE += /MT
else
CC_BASE += $(D)LINUX
endif

CC_BASE_DYNAMORIO = $(CC_BASE) $(DYNAMORIO_INC) $(D)DYNAMO $(ARCH_FLAGS)

ifeq ($(MACHINE), linux)
LINK = -lm
LINK_DYNAMORIO = $(L)$(DYNAMORIO_DIR) -ldynamorio -lm -Xlinker -rpath -Xlinker $(DYNAMORIO_DIR)
else # win32
# /MTd must be included when compiling, not on link line!
# else get weird link errors
WINLIBS =  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
           advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib \
           odbc32.lib odbccp32.lib imagehlp.lib libcmt.lib
LINK = $(WINLIBS) /link /subsystem:console
LINK_DYNAMORIO = $(L)$(DYNAMORIO_LIBDIR) $(WINLIBS) dynamorio.lib /subsystem:console
endif

ifeq ($(MACHINE), linux)
SRCS := 
else
SRCS := module_rsrc.c winsysnums.c vista_hash.c
endif

PGMS := $(patsubst %.c,%$(EXE_SFX),$(SRCS))

all: $(PGMS)

ifeq ($(MACHINE), win32)
# using %$(EXE_SFX) doesn't seem to work well on win32, so we duplicate
$(PGMS): %.exe: %.c
	$(CC_BASE_DYNAMORIO) $(D)USE_DYNAMO $(EXEOUT)$@ $< $(LINK_DYNAMORIO)
else # linux
$(PGMS): %: %.c
	$(CC_BASE_DYNAMORIO) $(D)USE_DYNAMO $(EXEOUT)$@ $< $(LINK_DYNAMORIO)
endif # linux

clean:
	-rm -f $(PGMS) *.$(OBJ_SFX) *.pdb *.ilk *.opt
