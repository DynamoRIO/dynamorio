# **********************************************************
# Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
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

###########################################################################
###########################################################################
###
### Makefile for DynamoRIO samples
### Requires GNU make for conditionals.
### On Windows requires cygwin cygpath and standard VC command-line setup
### (cl.exe on path, LIB and INCLUDE environment vars set).
### Use ARCH=x64 to build for 64bit.
### Use DEBUG=1 to link with the debug build.
###

.PHONY: clean all

GCC := gcc
GXX := g++
CL := cl

ifndef ARCH
  # default is x86 instead of x64
  ARCH := x86
endif
ifndef DEBUG
  # default is release build
  DEBUG := 0
endif

ifndef DYNAMORIO_HOME
  # assume we're in docs/html/samples/
  DYNAMORIO_HOME := ../../..
endif

ifeq ($(shell uname -s | cut -c1-5), Linux)
  MACHINE := linux
  WD := $(shell pwd)
else
  MACHINE := win32
  WD := $(shell cygpath -w $(shell pwd))
  # rely on the env vars used for cmd-line building with cl.exe
  SYS_INCLUDE := /I$(subst ;, /I, $(INCLUDE))
  SYS_LIB :=  /libpath:$(subst ;, /libpath:,$(LIB))
  DYNAMORIO_HOME := $(shell cygpath -ma $(DYNAMORIO_HOME))
endif

ifeq ($(DEBUG), 0)
LIBDIR := release
else
LIBDIR := debug
endif

ifeq ($(ARCH), x64)
  DR_LIBPATH := $(DYNAMORIO_HOME)/lib64/$(LIBDIR)
else
  DR_LIBPATH := $(DYNAMORIO_HOME)/lib32/$(LIBDIR)
endif
DR_LIB := dynamorio.lib
DR_INCLUDE := $(DYNAMORIO_HOME)/include

ifeq ($(MACHINE), linux)
  D := -D
  DBG := -g3
  SRC_EXCLUDE := stats.c
  ifeq ($(ARCH), x64)
    CFLAGS := -m64 $(DBG) $(D)LINUX
    # Note - it's currently required on 64bit that a client library have a
    # preferred base address in the lower 2GB, to be reachable by our
    # library and code cache.  This is due to limitations in the current
    # experimental 64bit release and will be adressed in later releases.
    # Naturally for multiple clients different addresses should be used.
    PREFERRED_BASE := 0x72000000
    LD_SCRIPT := ldscript
    # -dT is better when going through gcc but it requires ld 2.18+
    ifeq ($(shell $(LD) -help | grep dT),)
      LD_SCRIPT_OPTION := -T
    else
      LD_SCRIPT_OPTION := -dT
    endif
    LD_USE_SCRIPT := -Xlinker $(LD_SCRIPT_OPTION) -Xlinker $(LD_SCRIPT)
  else
    CFLAGS := -m32 $(DBG) $(D)LINUX
    LD_USE_SCRIPT :=
  endif
  SLIB_SUFFIX := .so
  SEXE_SUFFIX :=
  CXXFLAGS += `$(GXX) -print-file-name=libstdc++.a`
  CXXFLAGS += `$(GXX) -print-file-name=libgcc.a`
  CXXFLAGS += `$(GXX) -print-file-name=libgcc_eh.a`
  CXXFLAGS += -Xlinker -wrap=malloc -Xlinker -wrap=realloc -Xlinker -wrap=free
else
  D := /D
  DBG := /Zi
  CFLAGS := /nologo /GS- $(DBG) $(D)WINDOWS
  SRC_EXCLUDE := stl_test.cpp
  ifeq ($(ARCH), x64)
    # Note - it's currently required on 64bit that /base is used to specify a
    # preferred base address such that the resulting instrumentation dll is
    # loaded in the range 0x72000000-0x75000000. To ensure that we also add
    # /FIXED.  This is due to limitations in the current experimental 64bit release
    # and will be adressed in later releases.
    # Naturally for multiple clients different addresses should be used.
    LFLAGS := /machine:X64 /base:"0x72000000" /FIXED
    SRC_EXCLUDE += stats.c
  else
    LFLAGS :=
  endif
  SLIB_SUFFIX := .dll
  SEXE_SUFFIX := .exe
endif

# standalone client isn't easy to run in short regressions
SRCS_STANDALONE := tracedump.c
SEXES := $(subst .c,$(SEXE_SUFFIX),$(SRCS_STANDALONE))

SRCS_LIB := $(filter-out $(SRC_EXCLUDE) $(EXCLUDE) $(SRCS_STANDALONE),$(wildcard *.c *.cpp))
SLIBS := $(subst .c,$(SLIB_SUFFIX),$(subst .cpp,$(SLIB_SUFFIX),$(SRCS_LIB)))

SHOW_RESULTS := 1
ifeq ($(SHOW_RESULTS), 1)
  CFLAGS += $(D)SHOW_RESULTS
endif
ifeq ($(ARCH), x64)
  CFLAGS += $(D)X86_64
else
  CFLAGS += $(D)X86_32
endif

all: $(SLIBS) $(SEXES)

listsrcs:
	@echo $(SRCS_LIB)

# shared library clients
ifeq ($(MACHINE), linux)
  ifeq ($(ARCH), x64)
$(LD_SCRIPT):
    # To set a preferred base address we tweak the default linker script
	ld --verbose | sed '/SECTIONS/,/^}/!d' | \
	  sed 's/0x400000/$(PREFERRED_BASE)/g; /^{/a . = $(PREFERRED_BASE);' > $@
  endif

%.so: %.c $(LD_SCRIPT)
    # avoid SElinux text relocation security violations by explicitly requesting PIC
	$(GCC) $(CFLAGS) -fPIC -shared -nostartfiles -I$(DR_INCLUDE) $(SYS_INCLUDE) \
          $< -o $@ $(LD_USE_SCRIPT)
    # client libraries need to be executable by all, otherwise the
    # client library may not be loaded if run as a different user
	chmod ugo+rx "$(WD)/$@"
	@echo "---------------------------------------------------------------------------"
	@echo "Usage: pass to drdeploy :"
	@echo "  -client $(WD)/$@ 0 \"\""

%.so: %.cpp $(LD_SCRIPT)
    # avoid SElinux text relocation security violations by explicitly requesting PIC
    # Note that if we list the source file before the .a files (in CXXFLAGS) we
    # get ld warnings on x64 so be sure to list source file last.
	$(GXX) $(CFLAGS) $(CXXFLAGS) -fPIC -shared -nostartfiles -I$(DR_INCLUDE) \
          $(SYS_INCLUDE) $< -o $@ $(LD_USE_SCRIPT)
else
%.dll: %.c
	$(CL) $(CFLAGS) $< /I$(DR_INCLUDE) $(SYS_INCLUDE) /link $(LFLAGS) $(SYS_LIB) \
          /libpath:$(DR_LIBPATH) $(DR_LIB) /dll /out:$@ /nodefaultlib /noentry
endif
    # client libraries need to be executable by all, otherwise the
    # client library may not be loaded if run as a different user
	chmod ugo+rx "$(WD)/$@"
	@echo "---------------------------------------------------------------------------"
	@echo "Usage: pass to drdeploy :"
ifeq ($(MACHINE), linux)
	@echo "  -client $(WD)/$@ 0 \"\""
else
	@echo "  -client '$(WD)\\$@' 0 \"\""
endif

# standalone programs
ifeq ($(MACHINE), linux)
%: %.c
	$(GCC) $(CFLAGS) -I$(DR_INCLUDE) $(SYS_INCLUDE) -L$(DR_LIBPATH) -ldynamorio \
          -Xlinker -rpath -Xlinker $(DR_LIBPATH) $< -o $@
	@echo "rpath is set, but if libdynamorio.so is moved, LD_LIBRARY_PATH will need to be set"
else
%.exe: %.c
	$(CL) $(CFLAGS) $< /I$(DR_INCLUDE) $(SYS_INCLUDE) /link /nodefaultlib $(SYS_LIB) \
          /libpath:$(DR_LIBPATH) libcmt.lib $(DR_LIB) kernel32.lib user32.lib /out:$@
	@echo "---------------------------------------------------------------------------"
	@echo "Make sure the loader finds dynamorio.dll (e.g., copy to same dir)"
endif

clean:
ifeq ($(MACHINE), linux)
	-rm -f *.so
	-rm -f *.log
  ifeq ($(ARCH), x64)
	-rm -f $(LD_SCRIPT)
  endif
else
	-rm -f *.{dll,exp,lib,obj,pdb,ilk,log}
endif
	-rm -f $(SEXES)
