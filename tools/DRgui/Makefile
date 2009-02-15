# **********************************************************
# Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
# **********************************************************/

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

ifndef DYNAMORIO_MAKE
  DYNAMORIO_MAKE := ../../make
endif
ifndef DYNAMORIO_BASE
  DYNAMORIO_BASE := ../..
endif

# PR 232385: we need DRgui.exe to run on NT, which means we cannot
# build using VC71 or VC80, as their MFC versions add dependencies on
# libraries like shlwapi.dll and ole*.dll that are not present on NT.
# Unfortunately with VC60 we can't make a debug build as libutil
# uses VC71 and we can't incorporate the policy_staticd pdbs.
USE_VC60 := 1

include $(DYNAMORIO_MAKE)/compiler.mk

ifeq ($(wildcard ${BUILD_LIBUTIL}),)
  $(error Please build libutil first)
endif

# use DYNAMORIO_HOME if possible
ifndef DYNAMORIO_HOME
  DYNAMORIO_HOME := ../..
endif
WIN_DYNAMORIO_HOME := $(shell $(CYGPATH) -ma ${DYNAMORIO_HOME})

WIN_DYNAMORIO_LIBUTIL := $(shell $(CYGPATH) -ma ${BUILD_LIBUTIL})

CORE_DIRNAME := core

# VisualStudio commandline options:
# cl /Od /I "%DYNAMORIO_HOME%\exports\x86_win32_dbg" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "UNICODE" /D "NOT_DYNAMORIO_CORE" /D "_VC80_UPGRADE=0x0600" /D "_UNICODE" /Gm /EHsc /RTC1 /MTd /Yu"stdafx.h" /Fp".\Debug_Unicode/DynamoRIO.pch" /Fo".\Debug_Unicode/" /Fd".\Debug_Unicode/" /FR".\Debug_Unicode\\" /W3 /nologo /c /ZI /TP /errorReport:prompt
# link /OUT:".\Debug_Unicode/DynamoRIO.exe" /INCREMENTAL /NOLOGO /MANIFEST /MANIFESTFILE:".\Debug_Unicode\DynamoRIO.exe.intermediate.manifest" /DELAYLOAD:"OleAcc.dll" /DEBUG /PDB:".\Debug_Unicode/DynamoRIO.pdb" /SUBSYSTEM:WINDOWS /ENTRY:"wWinMainCRTStartup" /MACHINE:X86 /ERRORREPORT:PROMPT  DelayImp.lib

DEFINES := /D"WIN32" /D"WINDOWS" /D"_DEBUG" /D"_WINDOWS" /D"UNICODE" /D"_UNICODE" \
	   /D"NOT_DYNAMORIO_CORE" /D"_VC80_UPGRADE=0x0600"

DEMO := 1

ifeq ($(DEMO), 1)
  DEFINES += /D"DYNAMORIO_DEMO"
endif

ifeq ($(ARCH), x64)
  DEFINES += /D"X64"
endif

INCLUDES := /I"$(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)/lib" \
	    /I"$(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)" \
	    /I"$(WIN_DYNAMORIO_LIBUTIL)" \
	    $(MSINCLUDE)

WARN := /W4 /WX

CFLAGS += $(WARN) $(DEFINES) $(INCLUDES)

# FIXME: sort out the reasons why I need these nodefaults
ifeq ($(USE_VC60),1)
# still get warning about libcmt but I do need it
NODEF := /NODEFAULTLIB:libc.lib
else
NODEF := /NODEFAULTLIB:libcmt.lib
endif

LDFLAGS := $(MSLIB) advapi32.lib $(NODEF) \
	   /SUBSYSTEM:WINDOWS /ENTRY:"wWinMainCRTStartup" \
	   /MACHINE:X86

ifdef DEBUG
  CFLAGS += /Zi /Od
  LDFLAGS += /debug
  POLICY_LIB := $(BUILD_LIBUTIL)/policy_staticd.lib
else
  CFLAGS += /O2
  POLICY_LIB := $(BUILD_LIBUTIL)/policy_static.lib
endif

WIN_POLICY_LIB := "$(shell $(CYGPATH) -ma $(POLICY_LIB))"

ifeq ($(DEMO), 1)
  BUILDDIR := $(BUILD_TOOLS)/DRgui_dbg_demo
else
  BUILDDIR := $(BUILD_TOOLS)/DRgui_dbg_internal
endif

EXEBASE := DRgui
HEADERS := $(wildcard *.h)
SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,$(BUILDDIR)/%.obj,$(SRCS)) 
RES_SRCS := $(wildcard *.rc)
RES_OBJS := $(patsubst %.rc,$(BUILDDIR)/%.res,$(RES_SRCS)) 

.PHONY: clean

all: $(BUILDDIR) $(BUILDDIR)/$(EXEBASE).exe

$(BUILDDIR): 
	@(if ! test -d $(BUILDDIR); then $(MKDIR) -p $(BUILDDIR); fi)

$(BUILDDIR)/$(EXEBASE).exe: $(OBJS) $(RES_OBJS) $(POLICY_LIB)
	$(LD) $(LDFLAGS) $(OBJS) $(RES_OBJS) $(WIN_POLICY_LIB) /out:$@ /pdb:$(BUILDDIR)/$(EXEBASE).pdb

$(BUILDDIR)/%.obj: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

$(BUILDDIR)/%.res: %.rc
	$(RC) /r /fo $@ $(INCLUDES) $<

clean: 
	$(RM) -r $(BUILDDIR)
