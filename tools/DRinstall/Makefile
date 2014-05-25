# **********************************************************
# Copyright (c) 2008 VMware, Inc.  All rights reserved.
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

# We would stick w/ VC60 for compatibility w/ NT, to match DRstats.exe (PR 232385),
# but it runs out of memory processing zipdump.h, and /Zm1000 doesn't even
# get halfway (/Zm1500 and higher don't seem to work).
# We go w/ 7.1 instead of 8 to avoid fixing the warnings that 8 has.
# Using static libs (/MT, and no _AFXDLL define), this does run on 2K,
# so only NT is out of luck.
USE_VC71 := 1

include $(DYNAMORIO_MAKE)/compiler.mk

# use DYNAMORIO_HOME if possible
ifndef DYNAMORIO_HOME
  DYNAMORIO_HOME := ../..
endif
WIN_DYNAMORIO_HOME := $(shell $(CYGPATH) -ma ${DYNAMORIO_HOME})

ifndef DYNAMORIO_LIBUTIL
  DYNAMORIO_LIBUTIL := ../../libutil
endif
WIN_DYNAMORIO_LIBUTIL := $(shell $(CYGPATH) -ma ${DYNAMORIO_LIBUTIL})

DEFINES := /D"WIN32" /D"WINDOWS" /D"_WINDOWS"

ifeq ($(ARCH), x64)
  DEFINES += /D"X64"
endif

LDFLAGS := $(MSLIB) advapi32.lib /SUBSYSTEM:WINDOWS /MACHINE:X86

ifdef USE_VC60
  DEFINES += /D"USE_VC60"
endif

ifdef DEBUG
  CFLAGS += /Zi /Od /MTd
  DEFINES += /D"_DEBUG"
  LDFLAGS += /debug
  BUILDDIR := $(BUILD_TOOLS)/DRinstall_dbg
else
  CFLAGS += /O2 /MT
  BUILDDIR := $(BUILD_TOOLS)/DRinstall_rel
endif

INCLUDES := $(MSINCLUDE) /I$(BUILDDIR)

WARN := /W4 /WX

CFLAGS += $(WARN) $(DEFINES) $(INCLUDES)

EXEBASE := DRinstall
HEADERS := $(wildcard *.h)
SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,$(BUILDDIR)/%.obj,$(SRCS))
RES_SRCS := $(wildcard *.rc)
RES_OBJS := $(patsubst %.rc,$(BUILDDIR)/%.res,$(RES_SRCS))

# To have the files not accessible until the license is viewed, and to
# avoid distributing a zip utility or building a self-extracting zip
# file that has password support, we simply hide the zip file inside
# our executable data section.
ZIPFILE := dynamorio.zip
ZIPDUMP := $(BUILDDIR)/zipdump.h

.PHONY: clean all check

all: $(BUILDDIR) $(ZIPDUMP) $(BUILDDIR)/$(EXEBASE).exe

$(ZIPDUMP): $(ZIPFILE)
	$(OD) -v -t x1 $(ZIPFILE) | $(PERL) -p -e 's|^\w+\s+(.*)$$|\1 |; s|(\w+) +|0x\1,|g;' > $@

$(ZIPFILE):
	@(if ! test -e $(ZIPFILE); then \
	  $(ECHO) "ERROR: must point at zip file with ZIPFILE="; false; fi)

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
