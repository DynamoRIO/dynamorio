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

ifndef DYNAMORIO_HOME
  DYNAMORIO_HOME := ..
endif
WIN_DYNAMORIO_HOME := $(shell $(CYGPATH) -ma ${DYNAMORIO_HOME})

ifndef DYNAMORIO_HOTPATCH
  DYNAMORIO_HOTPATCH := ../liveshield
endif
WIN_DYNAMORIO_HOTPATCH := $(shell $(CYGPATH) -ma $(DYNAMORIO_HOTPATCH))

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
include $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/Makefile.custom_build

TARGET_LIB := policy
CONFIG_LIB := drconfig
MFAPI_LIB  := mfapi

BNFLAGS  := $(D)BUILD_NUMBER=$(BUILD_NUMBER)
TYPE_EXE := VFT_APP
TYPE_DLL := VFT_DLL

RCDEFS  := $(RESOURCE_DEFINES) $(BNFLAGS)
RCINCL  := /I"$(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)/lib" $(MSINCLUDE)
RCFLAGS := -r $(RCINCL) $(RCDEFS)

RCFILE  := $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32/resources.rc
WIN_RCFILE := $(shell $(CYGPATH) -ma $(RCFILE))

DEFINES := /DSHARED_CACHE /DRETURN_AFTER_CALL /DPROGRAM_SHEPHERDING \
	   /DCLIENT_INTERFACE /DPROFILE_LINKCOUNT /DSIMULATE_ATTACK /DGBOP \
	   /DWINDOWS_PC_SAMPLE /DHOT_PATCHING_INTERFACE /DNOT_DYNAMORIO_CORE \
	   /DX86 /DWINDOWS /DUNICODE /D_UNICODE /DDEBUG /DHOT_PATCHING_INTERFACE \
	   /D_CRT_SECURE_NO_WARNINGS

INCLUDES := /I"$(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)" \
	    /I"$(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)/lib" \
	    /I"$(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32" \
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

HEADERS := config.h elm.h share.h processes.h services.h utils.h events.h

CORE_SRCS := options.c inject_shared.c drmarker.c module_shared.c 
CORE_OBJS := $(subst .c,.obj,$(CORE_SRCS))
CORE_OBJS_D := $(subst .c,_d.obj,$(CORE_SRCS))
CORE_OBJS_S := $(subst .c,_static.obj,$(CORE_SRCS))
CORE_OBJS_SD := $(subst .c,_staticd.obj,$(CORE_SRCS))

BASE_LIST := services config elm processes utils policy parser
ROOT_LIST := $(BASE_LIST) detach mfapi $(subst .c,,$(CORE_SRCS))

LIB_OBJS   := $(patsubst %,%_static.obj,$(ROOT_LIST))
LIB_OBJS_D := $(patsubst %,%_staticd.obj,$(ROOT_LIST))

CONFIG_LIST   := $(ROOT_LIST) dr_config
CONFIG_OBJS   := $(patsubst %,%.obj,$(CONFIG_LIST))
CONFIG_OBJS_D := $(patsubst %,%_d.obj,$(CONFIG_LIST))

MFAPI_LIST := $(ROOT_LIST) mfapi
MFAPI_OBJS := $(patsubst %,%.obj,$(MFAPI_LIST))

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

ENGINE := 30000
DR_LIB_DIR := $(DYNAMORIO_HOME)/lib32
HOTP_LIB_DIR := $(DYNAMORIO_HOME)/lib32/hotp/$(ENGINE)
HOTP_DEFS_FILE := ls-defs.cfg
HOTP_MODES_FILE := test-modes.cfg
CONFIG_DIR := $(DYNAMORIO_HOME)/conf


.PHONY: all debug clean tester

.PRECIOUS: $(TEST_EXES) $(TESTER_EXES)


all: $(TARGET_LIB)_static.lib $(CONFIG_LIB).lib dumpevts.exe

debug: $(TARGET_LIB)_staticd.lib $(CONFIG_LIB)_d.lib

events.h: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32/events.mc
	$(MC) $(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32/events.mc

%.obj: %.c $(HEADERS)
	$(CC) $(CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

%_d.obj: %.c $(HEADERS)
	$(CC) $(DEBUG_CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

%_static.obj: %.c $(HEADERS)
	$(CC) $(STATIC_CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

%_staticd.obj: %.c $(HEADERS)
	$(CC) $(STATIC_DEBUG_CFLAGS) /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

%_test.obj: %.c $(HEADERS) tests.h
	$(CC) $(DEBUG_CFLAGS) $(SINGLETESTDEFS) $(FAILASSERTDEFS) /DUNIT_TEST /c $< /Fo$@ /Fd$(subst .obj,.pdb,$@)

# FIXME: if we use WIN_DYNAMORIO_HOME then some older makes parse the drive
# specification colon as part of the rule and complain; any cleaner way
# than invoking all these cygpaths?
%.obj: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/%.c
	$(CC) $(CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

%_d.obj: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/%.c
	$(CC) $(DEBUG_CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

%_static.obj: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/%.c
	$(CC) $(STATIC_CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

%_staticd.obj: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/%.c
	$(CC) $(STATIC_DEBUG_CFLAGS) $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

%.obj: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32/%.c
	$(CC) $(CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

%_d.obj: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32/%.c
	$(CC) $(DEBUG_CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

%_static.obj: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32/%.c
	$(CC) $(STATIC_CFLAGS) /c $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

%_staticd.obj: $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32/%.c
	$(CC) $(STATIC_DEBUG_CFLAGS) $(shell cygpath -ma $<) /Fo$@ /Fd$*.pdb

dumpevts.exe: dumpevts.obj elm.obj dumpevts.res
	$(LD) $(LDFLAGS) $^ $(GENERICLIBS) /out:$@

dumpevts.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) $(D)FILE_TYPE=$(TYPE_EXE) $(D)FILE_NAME=\"dumpevts.exe\" $(D)FILE_DESCRIPTION="PRODUCT_NAME \" event dumping tool\"" $(WIN_RCFILE)

$(TARGET_LIB)_static.lib: $(LIB_OBJS)
	$(AR) $(LDFLAGS) $^ $(GENERICLIBS) /out:$@

$(TARGET_LIB)_staticd.lib: $(LIB_OBJS_D)
	$(AR) $(DEBUG_LDFLAGS) /machine:I386 $^ $(GENERICLIBS) /out:$@

$(CONFIG_LIB).lib: $(CONFIG_LIB).dll

$(CONFIG_LIB).dll: $(CONFIG_OBJS)
	$(LD) $(LDFLAGS) /dll $(LINK_DBG) /opt:ref /opt:icf /def:$(CONFIG_LIB).def $^ $(GENERICLIBS) /out:$@ /pdb:$(CONFIG_LIB).pdb /mapinfo:exports

$(CONFIG_LIB)_d.lib: $(CONFIG_LIB)_d.dll

$(CONFIG_LIB)_d.dll: $(CONFIG_OBJS_D)
	$(LD) $(DEBUG_LDFLAGS) /dll /opt:ref /opt:icf /def:$(CONFIG_LIB).def $^ $(GENERICLIBS) /out:$@ /pdb:$(CONFIG_LIB)_d.pdb /mapinfo:exports

$(MFAPI_LIB).lib: $(MFAPI_LIB).dll

$(MFAPI_LIB).dll: $(MFAPI_OBJS) $(MFAPI_LIB).res
	$(LD) $(LDFLAGS) $(LINK_DBG) /opt:ref /opt:icf /pdb:$(MFAPI_LIB).pdb /DLL /def:$(MFAPI_LIB).def $^ $(GENERICLIBS) /out:$@

$(MFAPI_LIB).res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) $(D)FILE_TYPE=$(TYPE_DLL) $(D)FILE_NAME=\"$(MFAPI_LIB).dll\" $(D)FILE_DESCRIPTION="PRODUCT_NAME \" api interface dll\"" $(WIN_RCFILE)


#
# FIXME: the tests target almost certainly hasn't been tried for a while
#
tests: $(TARGET_LIB)_static.lib $(UNIT_TESTS)
	@echo "  ** All Unit Tests Passed **  "

tester: tester.exe $(HOTP_LIB_DIR) $(DR_LIB_DIR) $(CONFIG_DIR)/$(ENGINE)/$(HOTP_DEFS_FILE)
	@echo run tester.exe w/ appropriate options
	./$< -h

# hmm phony does not seem to work for this?? so use FORCE workaround.
%_test: %_test.exe $(TESTER_EXES) $(HOTP_LIB_DIR) $(DR_LIB_DIR) $(CONFIG_DIR)/$(ENGINE)/$(HOTP_DEFS_FILE) FORCE
	@echo Executing $@:
	WIN_DYNAMORIO_HOME=$(WIN_DYNAMORIO_HOME) ./$<

FORCE:

check_hotp_dir:
	@(if [ ! -d $(DYNAMORIO_HOTPATCH)/build/pkg ]; then \
	echo "DYNAMORIO_HOTPATCH must be defined (or at ..) and built!"; \
	false; fi)

check_dr_dir:
	@(if [ ! -f $(DYNAMORIO_HOME)/exports/lib32/release/dynamorio.dll ]; then \
	echo "core must be built!"; \
	false; fi)

$(DYNAMORIO_HOTPATCH)/test/hotp-test.lib: check_hotp_dir

$(DR_LIB_DIR): check_dr_dir
	mkdir -p $(DR_LIB_DIR)
	cp -f $(DYNAMORIO_HOME)/exports/lib32/release/*.dll $(DR_LIB_DIR)

$(HOTP_LIB_DIR): $(DR_LIB_DIR) check_hotp_dir
	cp -f $(DYNAMORIO_HOTPATCH)/test/hotp-test.dll .
	mkdir -p $(HOTP_LIB_DIR)
	cp -f $(DYNAMORIO_HOTPATCH)/build/pkg/liveshield-?????/lib/$(ENGINE)/* $(HOTP_LIB_DIR)

$(CONFIG_DIR):
	mkdir -p $(CONFIG_DIR)

$(CONFIG_DIR)/$(ENGINE): $(CONFIG_DIR)
	mkdir -p $@

$(CONFIG_DIR)/$(ENGINE)/$(HOTP_DEFS_FILE): check_hotp_dir $(HOTP_LIB_DIR) $(CONFIG_DIR)/$(ENGINE)
	cp $(DYNAMORIO_HOTPATCH)/build/pkg/liveshield-?????/*-defs-*.cfg $@

%_test.exe: %_test.obj $(LIB_OBJS_D)
	$(LD) $(LDFLAGS) $^ $(TEST_LDFLAGS) $(GENERICLIBS) /out:$@

tester.obj: tester.c check_hotp_dir
	$(CC) $(DEBUG_CFLAGS) /c $< /Fo$@

tester.exe: tester.obj policy_staticd.lib $(DYNAMORIO_HOTPATCH)/test/hotp-test.lib
	$(LD) /OPT:NOREF user32.lib tester.obj policy_staticd.lib $(WIN_DYNAMORIO_HOTPATCH)/test/hotp-test.lib /out:$@

tester_%.exe: tester.obj policy_staticd.lib $(DYNAMORIO_HOTPATCH)/test/hotp-test.lib
	$(LD) /OPT:NOREF user32.lib tester.obj policy_staticd.lib $(WIN_DYNAMORIO_HOTPATCH)/test/hotp-test.lib /out:$@


clean:
	$(RM) -r *.exe *.obj *.lib *.dll *~ *.exp *.pch *.idb *.pdb *.ilk test.mfp test2.mfp testcr.txt utils.tst $(DR_LIB_DIR) $(HOTP_MODES_FILE) $(CONFIG_DIR) tester.out* test-modes-2.cfg events.h
