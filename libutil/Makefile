# **********************************************************
# Copyright (c) 2003-2009 VMware, Inc.  All rights reserved.
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
  DYNAMORIO_MAKE := ../make
endif

include $(DYNAMORIO_MAKE)/compiler.mk

OUTDIR := $(BUILD_LIBUTIL)

ifndef DYNAMORIO_HOME
  DYNAMORIO_HOME := ../exports
endif
WIN_DYNAMORIO_BASE := $(shell $(CYGPATH) -ma ${DYNAMORIO_BASE})

# FIXME: fix the underlying cause of these warnings and
# remove these exceptions
CFLAGS += /wd4996 /wd4047 /wd4024 /wd4133 /wd4127 /wd4100
CFLAGS += /wd4292 /wd4127 /wd4100
# avoid security_check_cookie issues when linking our library w/ others
CFLAGS += /GS-

ifndef VMAP
# We default to VMSAFE configuration
CFLAGS += /DPROGRAM_SHEPHERDING /DMF_API /DHOT_PATCHING_INTERFACE /DPROBE_API
endif

ifndef DIFFNUM
  DIFFNUM := 1
endif

# xref PR 192750 - runregression overrides this
DBG = /Zi
LINK_DBG = /debug

CORE_DIRNAME := core
include $(DYNAMORIO_MAKE)/custom_build.mk

TARGET_LIB := policy
CONFIG_LIB := drconfig
MFAPI_LIB  := mfapi

BNFLAGS  := $(D)BUILD_NUMBER=$(BUILD_NUMBER)
TYPE_EXE := VFT_APP
TYPE_DLL := VFT_DLL

RCDEFS  := $(RESOURCE_DEFINES) $(BNFLAGS)
RCINCL  := /I"$(WIN_DYNAMORIO_BASE)/$(CORE_DIRNAME)/lib" $(MSINCLUDE)
RCFLAGS := -r $(RCINCL) $(RCDEFS)

RCFILE  := $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/win32/resources.rc
WIN_RCFILE := $(shell $(CYGPATH) -ma $(RCFILE))

DEFINES := /DSHARED_CACHE /DRETURN_AFTER_CALL /DPROGRAM_SHEPHERDING \
	   /DCLIENT_INTERFACE /DPROFILE_LINKCOUNT /DSIMULATE_ATTACK /DGBOP \
	   /DWINDOWS_PC_SAMPLE /DHOT_PATCHING_INTERFACE /DNOT_DYNAMORIO_CORE \
	   /DX86 /DWINDOWS /DUNICODE /D_UNICODE /DDEBUG /DHOT_PATCHING_INTERFACE \
	   /D_CRT_SECURE_NO_WARNINGS

INCLUDES := /I"$(WIN_DYNAMORIO_BASE)/$(CORE_DIRNAME)" \
	    /I"$(WIN_DYNAMORIO_BASE)/$(CORE_DIRNAME)/lib" \
	    /I"$(WIN_DYNAMORIO_BASE)/$(CORE_DIRNAME)/win32" \
	    /I"$(OUTDIR)" \
	    $(MSINCLUDE)

CFLAGS += /W4 /WX $(DEFINES) $(INCLUDES)
DEBUG_CFLAGS := $(CFLAGS) $(DBG)
STATIC_DEBUG_CFLAGS := $(DEBUG_CFLAGS) /MTd
CFLAGS += /O2
STATIC_CFLAGS := $(CFLAGS) /MT

LDFLAGS := $(MSLIB)
LDFLAGS += /nodefaultlib:libc.lib
DEBUG_LDFLAGS := $(MSLIB) $(LINK_DBG)

GENERICLIBS := advapi32.lib

HEADERS := config.h elm.h share.h processes.h services.h utils.h $(OUTDIR)/events.h

CORE_SRCS := options.c inject_shared.c drmarker.c module_shared.c 
CORE_OBJS := $(subst .c,.obj,$(CORE_SRCS))
CORE_OBJS_D := $(subst .c,_d.obj,$(CORE_SRCS))
CORE_OBJS_S := $(subst .c,_static.obj,$(CORE_SRCS))
CORE_OBJS_SD := $(subst .c,_staticd.obj,$(CORE_SRCS))

BASE_LIST := services config elm processes utils policy parser
ROOT_LIST := $(BASE_LIST) detach mfapi $(subst .c,,$(CORE_SRCS))

LIB_OBJS   := $(patsubst %,$(OUTDIR)/%_static.obj,$(ROOT_LIST))
LIB_OBJS_D := $(patsubst %,$(OUTDIR)/%_staticd.obj,$(ROOT_LIST))

CONFIG_LIST   := $(ROOT_LIST) dr_config
CONFIG_OBJS   := $(patsubst %,$(OUTDIR)/%.obj,$(CONFIG_LIST))
CONFIG_OBJS_D := $(patsubst %,$(OUTDIR)/%_d.obj,$(CONFIG_LIST))

MFAPI_LIST := $(ROOT_LIST) mfapi
MFAPI_OBJS := $(patsubst %,$(OUTDIR)/%.obj,$(MFAPI_LIST))

# FIXME: unit tests for detach?
TEST_LIST  := $(BASE_LIST) tests
TEST_EXES  := $(patsubst %,%_test.exe,$(TEST_LIST))
UNIT_TESTS := $(patsubst %,%_test,$(TEST_LIST))

# see tests.h
TESTER_EXES  := tester_1.exe tester_2.exe
TEST_LDFLAGS := $(LINK_DBG) /pdb:test.pdb

ifdef SINGLE_TEST_NAME
  SINGLETESTDEFS := /DSINGLE_TEST=$(SINGLE_TEST_NAME)
else
  SINGLETESTDEFS := 
endif

ifdef FAIL_ON_TEST_ASSERT
  FAILASSERTDEFS := /DFAIL_ON_TEST_ASSERT
else
  FAILASSERTDEFS :=
endif

.PHONY: all debug clean tester outdir

.PRECIOUS: $(TEST_EXES) $(TESTER_EXES)

all: outdir $(OUTDIR)/$(TARGET_LIB)_static.lib $(OUTDIR)/$(CONFIG_LIB).lib $(OUTDIR)/dumpevts.exe

outdir:
	@(if ! test -e $(OUTDIR); then $(MKDIR) -p $(OUTDIR); fi)
# to support DYNAMORIO_LIBUTIL still pointing at a dir with both headers and
# libraries we copy the headers to the outdir
	$(CP) *.h $(OUTDIR)

debug: $(OUTDIR)/$(TARGET_LIB)_staticd.lib $(OUTDIR)/$(CONFIG_LIB)_d.lib

$(OUTDIR)/events.h: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/win32/events.mc
	$(MC) -r $(OUTDIR) -h $(OUTDIR) $(WIN_DYNAMORIO_BASE)/$(CORE_DIRNAME)/win32/events.mc

$(OUTDIR)/%.obj: %.c $(HEADERS)
	$(CC) $(CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

$(OUTDIR)/%_d.obj: %.c $(HEADERS)
	$(CC) $(DEBUG_CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

$(OUTDIR)/%_static.obj: %.c $(HEADERS)
	$(CC) $(STATIC_CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

$(OUTDIR)/%_staticd.obj: %.c $(HEADERS)
	$(CC) $(STATIC_DEBUG_CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

$(OUTDIR)/%_test.obj: %.c $(HEADERS) tests.h
	$(CC) $(DEBUG_CFLAGS) $(SINGLETESTDEFS) $(FAILASSERTDEFS) /DUNIT_TEST /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

# FIXME: if we use WIN_DYNAMORIO_BASE then some older makes parse the drive
# specification colon as part of the rule and complain; any cleaner way
# than invoking all these cygpaths?
$(OUTDIR)/%.obj: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/%.c
	$(CC) $(CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

$(OUTDIR)/%_d.obj: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/%.c
	$(CC) $(DEBUG_CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

$(OUTDIR)/%_static.obj: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/%.c
	$(CC) $(STATIC_CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

$(OUTDIR)/%_staticd.obj: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/%.c
	$(CC) $(STATIC_DEBUG_CFLAGS) $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

$(OUTDIR)/%.obj: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/win32/%.c
	$(CC) $(CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

$(OUTDIR)/%_d.obj: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/win32/%.c
	$(CC) $(DEBUG_CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

$(OUTDIR)/%_static.obj: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/win32/%.c
	$(CC) $(STATIC_CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

$(OUTDIR)/%_staticd.obj: $(DYNAMORIO_BASE)/$(CORE_DIRNAME)/win32/%.c
	$(CC) $(STATIC_DEBUG_CFLAGS) $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

$(OUTDIR)/dumpevts.exe: $(OUTDIR)/dumpevts.obj $(OUTDIR)/elm.obj $(OUTDIR)/dumpevts.res
	$(LD) $(LDFLAGS) $^ $(GENERICLIBS) /out:$@

$(OUTDIR)/dumpevts.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) $(D)FILE_TYPE=$(TYPE_EXE) $(D)FILE_NAME=\"dumpevts.exe\" $(D)FILE_DESCRIPTION="PRODUCT_NAME \" event dumping tool\"" $(WIN_RCFILE)

$(OUTDIR)/$(TARGET_LIB)_static.lib: $(LIB_OBJS)
	$(AR) $(LDFLAGS) $^ $(GENERICLIBS) /out:$@

$(OUTDIR)/$(TARGET_LIB)_staticd.lib: $(LIB_OBJS_D)
	$(AR) $(DEBUG_LDFLAGS) /machine:I386 $^ $(GENERICLIBS) /out:$@

$(OUTDIR)/$(CONFIG_LIB).lib: $(OUTDIR)/$(CONFIG_LIB).dll

$(OUTDIR)/$(CONFIG_LIB).dll: $(CONFIG_OBJS)
	$(LD) $(LDFLAGS) /dll $(LINK_DBG) /opt:ref /opt:icf /def:$(CONFIG_LIB).def $^ $(GENERICLIBS) /out:$@ /pdb:$(OUTDIR)/$(CONFIG_LIB).pdb /mapinfo:exports

$(OUTDIR)/$(CONFIG_LIB)_d.lib: $(OUTDIR)/$(CONFIG_LIB)_d.dll

$(OUTDIR)/$(CONFIG_LIB)_d.dll: $(CONFIG_OBJS_D)
	$(LD) $(DEBUG_LDFLAGS) /dll /opt:ref /opt:icf /def:$(CONFIG_LIB).def $^ $(GENERICLIBS) /out:$@ /pdb:$(OUTDIR)/$(CONFIG_LIB)_d.pdb /mapinfo:exports

$(OUTDIR)/$(MFAPI_LIB).lib: $(OUTDIR)/$(MFAPI_LIB).dll

$(OUTDIR)/$(MFAPI_LIB).dll: $(MFAPI_OBJS) $(OUTDIR)/$(MFAPI_LIB).res
	$(LD) $(LDFLAGS) $(LINK_DBG) /opt:ref /opt:icf /pdb:$(OUTDIR)/$(MFAPI_LIB).pdb /DLL /def:$(MFAPI_LIB).def $^ $(GENERICLIBS) /out:$@

$(OUTDIR)/$(MFAPI_LIB).res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) $(D)FILE_TYPE=$(TYPE_DLL) $(D)FILE_NAME=\"$(OUTDIR)/$(MFAPI_LIB).dll\" $(D)FILE_DESCRIPTION="PRODUCT_NAME \" api interface dll\"" $(WIN_RCFILE)

clean:
	$(RM) -rf $(OUTDIR)
