# **********************************************************
# Copyright (c) 2003-2009 VMware, Inc.  All rights reserved.
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
##
## USAGE:
##
## Just say "make" and it will run all tests and diff results with
## expected results.  
##
## The output of the programs are placed in a directory named "results"
## by default, it can be set like this "make RESDIR='myresdir`".
##
## To change DynamoRIO options: say "make OPS='whatever'"
##

.PHONY: all run clean runbase runsecurity runrunall runnogui \
        runrunall-nogui runapi runinterface runcommon runos runbasesecurity \
	runossecurity runnointerface runheader reportbuild

MAKEFLAGS := -s

ifndef DYNAMORIO_MAKE
  DYNAMORIO_MAKE := ../../make
endif
ifndef DYNAMORIO_BASE
  DYNAMORIO_BASE := ../..
endif
include $(DYNAMORIO_MAKE)/compiler.mk

# Set default values for the DYNAMORIO_ env vars
ifeq ($(MACHINE), win32)
ifndef DYNAMORIO_HOME
  DYNAMORIO_HOME := $(shell $(CYGPATH) -ma $(CURDIR)/../../exports)
endif
# export for runall/runalltest.sh
# try to check for relative path: incorrect if .. in middle
ifeq ($(findstring ..,$(BUILD_TOOLS)),)
  export BUILD_TOOLS := $(shell $(CYGPATH) -ma $(BUILD_TOOLS))
else
  export BUILD_TOOLS := $(shell $(CYGPATH) -ma $(CURDIR)/$(BUILD_TOOLS))
endif
ifeq ($(findstring ..,$(INSTALL_BIN)),)
  export INSTALL_BIN := $(shell $(CYGPATH) -ma $(INSTALL_BIN))
else
  export INSTALL_BIN := $(shell $(CYGPATH) -ma $(CURDIR)/$(INSTALL_BIN))
endif
ifndef DYNAMORIO_TOOLS
  export DYNAMORIO_TOOLS := $(shell $(CYGPATH) -ma $(CURDIR)/../../tools)
endif
ifndef DYNAMORIO_LOGDIR
  export DYNAMORIO_LOGDIR := $(shell $(CYGPATH) -ma $(CURDIR)/logs)
endif
ifndef DYNAMORIO_CACHE_ROOT
  export DYNAMORIO_CACHE_ROOT := $(shell $(CYGPATH) -ma $(CURDIR)/cache)
endif
else
ifndef DYNAMORIO_HOME
  DYNAMORIO_HOME := $(CURDIR)/../../exports
endif
# export for runall/runalltest.sh
# try to check for relative path: incorrect if .. in middle
ifeq ($(findstring ..,$(BUILD_TOOLS)),)
  export BUILD_TOOLS := $(BUILD_TOOLS)
else
  export BUILD_TOOLS := $(CURDIR)/$(BUILD_TOOLS)
endif
ifeq ($(findstring ..,$(INSTALL_BIN)),)
  export INSTALL_BIN := $(INSTALL_BIN)
else
  export INSTALL_BIN := $(CURDIR)/$(INSTALL_BIN)
endif
ifndef DYNAMORIO_TOOLS
  export DYNAMORIO_TOOLS := $(CURDIR)/../../tools
endif
ifndef DYNAMORIO_LOGDIR
  export DYNAMORIO_LOGDIR := $(CURDIR)/logs
endif
ifndef DYNAMORIO_CACHE_ROOT
  export DYNAMORIO_CACHE_ROOT := $(CURDIR)/cache
endif
endif

ifeq ($(MACHINE), linux)
# EXTRA TESTS
# Until we need file-based .runall-type functionality, for now I'm
# adding a few extra runs of other test programs solely in the Makefile:
EXTRA_OS_TESTS := linux/clone-reset linux/thread-reset linux/execve32 linux/execve64
endif

EXTRA_COMMON_TESTS += common/decode-stress

EXTRA_TESTS := $(EXTRA_COMMON_TESTS) $(EXTRA_OS_TESTS)
EXTRA_TESTS_EXE := $(patsubst %,%.$(EXE_SFX),$(EXTRA_TESTS))

# **** DISABLED TESTS ****
# Issue 20: execve => execve{32,64} and runs execve-sub
DISABLED_LINUX := | $(GREP) -vE '/vfork|/execve-sub|/execve.c'
# PR 215238 covers re-enabling threadinjection
DISABLED_WIN32 := | $(GREP) -vE '/debugger|/threadinjection'
# case 4757 covers re-enabling selfmod-threads
DISABLED_SECURITY_WIN32 := | $(GREP) -vE 'selfmod-threads'
# case 9423 covers re-enabling
DISABLED_RUNALL := | $(GREP) -vE 'earlythread'
# pthreads tests are also _completely_ disabled down below
# FIXME: PR 199115 to re-enable fragdel, get some more of the LINUX tests working
ifeq ($(MACHINE), win32)
DISABLED_INTERFACE := | $(GREP) -vE '/fragdel|/syscall-mod|/signal'
else
DISABLED_INTERFACE := | $(GREP) -vE '/fragdel|/modules|/pc-check|/syscall\.|/cbr3|/cbr4|/nudge_test'
endif
# PR 217255: removed to shorten the regression suite
DISABLED_COMMON := | $(GREP) -vE '/hello|/conflict|/ops|/recurse'

ifeq ($(MACHINE), linux)
  DISABLED_OS := $(DISABLED_LINUX)
  DISABLED_SECURITY_OS := 
else
  DISABLED_OS := $(DISABLED_WIN32)
  DISABLED_SECURITY_OS := $(DISABLED_SECURITY_WIN32)
endif

DISABLED_ALL := $(DISABLED_OS) $(DISABLED_SECURITY_OS) $(DISABLED_RUNALL) \
		$(DISABLED_INTERFACE) $(DISABLED_COMMON)

# Some things that are nice to be able to turn off from the cmd line
WARN_DEFS := yes

# default directory to put results in
RESDIR := results

# windows vs. linux flags, etc.
ifeq ($(MACHINE), linux)
WARN    =-Wall -Werror
OPT     =-O3
NOPT    =-O0
DBG     =-g3
OBJ_SFX =o
EXE_SFX =
LIB_SFX =.so
LIB_NAME =libdynamorio.so
OS_DIR  := linux
else # win32
# avoid problems w/ line endings on #! files that cygwin's sh has
DLLOUT  =/dll /out:
LIBOUT  =/implib:
WARN    =/W3 /WX
OPT     =/O2
NOPT    =/Od
DBG     =/Zi
LINK_DBG=/debug
OBJ_SFX =obj
EXE_SFX =.exe
LIB_SFX =.dll
LIB_NAME =dynamorio.dll
OS_DIR  := win32
endif

# use release build if it exists, unless DEBUG=yes
DYNAMORIO_DIR_BASE :=$(DYNAMORIO_HOME)
ifeq ($(ARCH), x64)
  LIB_SUBDIR :=$(INSTALL_LIB_X64)
  BIN_SUBDIR :=bin64
else
  LIB_SUBDIR :=$(INSTALL_LIB_X86)
  BIN_SUBDIR :=bin32
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
DYNAMORIO_BUILD_DIR = $(DYNAMORIO_HOME)/build/$(ARCH)_$(OS_DIR)_rel
else
DYNAMORIO_DIR := $(DYNAMORIO_DIR_BASE)/$(LIB_SUBDIR)/debug
DYNAMORIO_BUILD_DIR = $(DYNAMORIO_HOME)/build/$(ARCH)_$(OS_DIR)_dbg
endif

DYNAMORIO_INCDIR := $(DYNAMORIO_DIR_BASE)/include
DYNAMORIO_BINDIR := $(DYNAMORIO_DIR_BASE)/bin/$(BIN_SUBDIR)

CORE_DIRNAME=core

ifeq ($(wildcard $(DYNAMORIO_BUILD_DIR)/defines.h), $(DYNAMORIO_BUILD_DIR)/defines.h)
DEFS := $(subst ",,$(subst \#define DYNAMORIO_DEFINES ,,$(shell $(GREP) "DX86" $(DYNAMORIO_BUILD_DIR)/defines.h | $(TAIL) -n 1)))
else 
ifeq ($(NATIVE), 1)
DEFS := NATIVE
else
DEFS := $(shell $(STRINGS) $(DYNAMORIO_DIR)/$(LIB_NAME) | $(GREP) "DX86" | $(TAIL) -n 1)
ifeq ( , $(DEFS))
DEFS_T := $(D)INLINE_IND_BR_LOOKUP $(D)TRACE_HEAD_CACHE_INCR $(D)X86 $(D)RELEASE
ifeq (detect_mode, $(shell $(STRINGS) $(DYNAMORIO_DIR)/$(LIB_NAME) | $(GREP) "detect_mode" | $(TAIL) -n 1))
DEFS_T += $(D)PROGRAM_SHEPHERDING
endif
ifeq ($(WARN_DEFS), yes)
DEFS += $(warning Can't determine defines for release build, guessing $(DEFS_T))
endif
DEFS += $(DEFS_T)
endif
endif # NATIVE
endif # defines.h
# get emacs syntax highlighting back on track: "

# runall max wait value. It's okay to use a large number here; closewnd
# closes the app as soon as it is found. We only reach the maximum
# when something goes wrong.
RUNALL_MAX_WAIT=10

# Max wait value for client interface tests using the runall script. 
# Make this larger than RUNALL_MAX_WAIT since client libraries tend
# to slow things down.
INTERFACE_MAX_WAIT=30

# turn on no debugbox, stderr mask, and staged for tests
# NOTE - should be kept in synch with the MESSAGES_TO_STDERR_* values in runregression
ifeq ($(MACHINE), linux)
OPS = -stderr_mask 0xC -dumpcore_mask 0 $(DYNAMORIO_OPTIONS)
else
OPS = -stderr_mask 0xC -msgbox_mask 0 -dumpcore_mask 0xfd -staged $(DYNAMORIO_OPTIONS)
endif

# use separate value than that passed in on cmdline so tests can modify it
DR_OPS = $(OPS)

ifeq ($(MACHINE), linux)
DYNAMORIO_INC := $(I)$(DYNAMORIO_INCDIR) 
else
DYNAMORIO_INC := $(I)$(subst /,\\,$(shell $(CYGPATH) -m ${DYNAMORIO_INCDIR}))
DYNAMORIO_LIBDIR := $(subst /,\\,$(shell $(CYGPATH) -m ${DYNAMORIO_DIR}))
endif

CC_BASE = $(CC) $(DBG) $I. $(MSINCLUDE) tools.c

# TODO: use common flags between the liveshield creation system and this
HOTP_DLL_INC_DIR = $(shell $(CYGPATH) -m $(DYNAMORIO_HOME))/../$(CORE_DIRNAME)/lib

# DBG and LINK_DBG flags are set to empty by runregression so that regressions
# over ssh can work (PR 192750).  However, this breaks hotp tests (PR 234043)
# as they need debug info to compute .cfg files.  As ssh builds are used by some
# for short regressions, the solution was to move hotp tests to long regressions
# and hard code the debug flags for hotp tests.  Thus short regressions will
# work over ssh (hotp tests will fail if run in short regressions) and hotp
# tests will pass in long regressions (which aren't run over ssh).
HOTP_DLL_CFLAGS = $(INCR) $(OPT) /Zi /Zl /X
HOTP_DLL_LDFLAGS = /NOENTRY /INCREMENTAL:NO /debug /OPT:ICF /OPT:NOWIN98 /RELEASE /MAP

# our default is to not optimize, because we have some programs whose purpose
# is destroyed by optimizing (in particular by inlining, for example common/getretaddr.c)
# (it's hard to add hacks to prevent inlining)
ifeq ($(OPTIMIZE),yes)
CC_BASE += $(OPT)
else
CC_BASE += $(NOPT)
endif

ifeq ($(ARCH), x64)
DEFINES += $(D)X86_64 $(D)X64
else
DEFINES += $(D)X86_32
endif

# some apps need LINUX defined even if not using API
ifeq ($(MACHINE), win32)
  CC_BASE += /MTd
  DEFINES += $(D)WINDOWS
else
  DEFINES += $(D)LINUX
  ifeq ($(ARCH), x64)
    TGT_CFLAGS += -m64
  else
    TGT_CFLAGS += -m32
  endif
endif

NIGHTLY = 1 # short enough to be tolerable for short runs
PERF = 0 # longer iterations and should measure performance
STRESS = 0 # even longer iterations (or infinite unless bound by TIMER_TIME)

ifeq ($(STRESS), 1)
DEFINES += $(D)STRESS
NIGHTLY = 0
endif

ifeq ($(PERF), 1)
DEFINES += $(D)PERF
NIGHTLY = 0
endif

ifneq ($(NIGHTLY), 0)
DEFINES += $(D)NIGHTLY_REGRESSION
endif

CC_BASE += $(TGT_CFLAGS) $(DEFINES)

CC_BASE_DYNAMORIO = $(CC_BASE) $(DYNAMORIO_INC) $(D)DYNAMO $(D)X86

# We need a different command line for building client library dlls.
# tools.c uses some external libraries, which clients should avoid.
ifeq ($(MACHINE), linux)
CC_BASE_INTERFACE = $(CC) $(TGT_CFLAGS) $(DBG) $(OPT) $(WARN) $(DEFINES)
else
CC_BASE_INTERFACE = $(CC) $(TGT_CFLAGS) $(DBG) /MTd $(OPT) $(WARN) $(MSINCLUDE) $(DEFINES)
# several client-interface tests have started hitting security_cookie issues
CC_BASE_INTERFACE += /GS-
endif

ifeq ($(MACHINE), linux)
C++_BASE = $(CC_BASE:gcc=g++)
else # win32
C++_BASE = $(CC_BASE) /TP
endif

ifeq ($(MACHINE), linux)
FCOMP = g77 -g
else
FCOMP = $(error "Fortran not yet supported on windows")
endif

ifeq ($(MACHINE), linux)
LINK = -lm -ldl
LINK_DYNAMORIO = $(L)$(DYNAMORIO_DIR) -ldynamorio -lm -Xlinker -rpath -Xlinker $(DYNAMORIO_DIR)
else # win32
  # /MTd must be included when compiling, not on link line!
  # else get weird link errors
  WINLIBS =  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
             advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib \
             odbc32.lib odbccp32.lib
  LINK = /link $(MSLIB) $(WINLIBS) /subsystem:console
  ifeq ($(ARCH), x64)
    LINK += /machine:X64
  else
    LINK += /machine:I386
  endif
  LINK_DYNAMORIO = $(L)$(DYNAMORIO_LIBDIR) dynamorio.lib $(MSLIB) $(WINLIBS) /subsystem:console
endif
# no harm in putting everywhere
LINK += $(LINK_ASM)
LINK += tools_asm.$(OBJ_SFX)

###########################################################################
# PR 262902: 64-bit tests
# Since we can't have inline asm for cl, and we don't have 64-bit gcc on cygwin,
# we put asm routines outside of the main c files in local _asm.asm files.
# We share the core's platform-neutral format with preprocessing to
# enable the assembly to work on linux and windows.

# asm flags
ASM_CORE_INC := $(DYNAMORIO_HOME)/../$(CORE_DIRNAME)/x86
ifeq ($(MACHINE), linux)
  ASMPREPROC := $(I)$(ASM_CORE_INC)
  DEFINES += $(D)ASSEMBLE_WITH_GAS
  ASM_DEFINES := $(patsubst $(D)%, --defsym %=1, $(DEFINES))
  ASM_DEFINES := $(patsubst $(U)%, --defsym %=0, $(ASM_DEFINES))
  ifeq ($(ARCH), x64)
    ASMFLAGS += --64
  else
    ASMFLAGS += --32
  endif
else
  ASMPREPROC := $(I)$(subst /,\\,$(shell $(CYGPATH) -m ${ASM_CORE_INC}))
  DEFINES += $(D)ASSEMBLE_WITH_MASM
  ASM_DEFINES := $(DEFINES)
  ASMFLAGS += $(INCR)
  ifeq ($(DEBUG),1)
    ASMFLAGS += $(DBG)
  endif
  # ml can't handle the line number markers
  ASMPREPROC += $(CPP_NO_LINENUM)
endif
ASMFLAGS += $(ASM_DEFINES)

# Some files use asm code, which must be separate for x64.
# Rather than have separate rules, we use an expansion on the link line
# that evaluates to empty if there is no asm code.
# However, wildcard and shell expansions do not work on the prereq
# line; they're supposed to if using .SECONDEXPANSION, but I could not
# get that to work with a rule of the format "many-files: %.exe: %.c",
# so I'm adding the prereqs a simpler way (the hack way is to have an
# "if test -e then make" command, but that's less efficient).
ASM_SRCS := $(shell grep -l asm_defines.asm */*.c) # FIXME: use SRCS_ALL
ASM_EXES := $(subst .c,$(EXE_SFX),$(ASM_SRCS))
$(ASM_EXES): %$(EXE_SFX): %_asm.$(OBJ_SFX)

# This must be = and not := since we need $@ expanded at the use site
# Note the space after $@, to avoid substring matching
ifeq ($(MACHINE), linux)
  LINK_ASM = $(addsuffix _asm.$(OBJ_SFX), $(findstring $@ ,$(ASM_EXES)))
else
  LINK_ASM = $(subst $(EXE_SFX),_asm.$(OBJ_SFX),$(findstring $@ ,$(ASM_EXES)))
endif

# Like the core, we use platform-independent assembly code, but we
# embed it inside our .c files and pull it out with the preprocessor
# (since we'd be pre-processing an .asm file anyway).

# asm code inside .c file -> .s
# we want ".ifdef FOO" to not turn into ".ifdef 1" so we say "-DFOO=FOO":
ASM_CPP_DEFINES := $(join $(DEFINES),$(patsubst $(D)%,=%,$(DEFINES)))
%_asm.s: %.c
	$(CPP) $(D)ASM_CODE_ONLY $(PREPROC) $(ASMPREPROC) $(CPPFLAGS) $(ASM_CPP_DEFINES) $< $(CPPOUT)$@
# since we don't want to duplicate gas and ml macros we use cpp macros w/
# the added feature of insertable newlines, needed for asm code, via "@N@":
	$(SED) -i 's/\@N\@/\n/g' $@
ifeq ($(MACHINE), linux)
# must remove _ from global symbols, rest is done via macros in .asm file
	$(SED) -i 's/\([^a-zA-Z0-9_]\)_\([a-zA-Z][a-zA-Z0-9_]*[a-zA-Z0-9]\)/\1\2/' $@
endif

# .s -> .o
%.$(OBJ_SFX): %.s
# ml and ml64 ignore /Fo unless it is BEFORE the source file
	$(AS) $(ASMFLAGS) $(OUT)$@ $<

###########################################################################

SRCS_COMMON := $(shell $(LS) common/*.c ${DISABLED_COMMON})

SRCS_API := $(shell $(LS) api/*.c)
# This is for probe-api tests and test clients; they have a 1-to-1 mapping.
SRCS_PROBE_API := $(shell $(LS) probe-api/*.c | $(GREP) -E -v '\.(client|dll.*|probe.*)\.c$$')
# These may not exist for a probe-api test.
SRCS_DLL_PROBE_API := $(wildcard probe-api/*.dll.c)       # probe-api test dlls
SRCS_PROBES_PROBE_API := $(wildcard probe-api/*.probe*.c) # probe-api probe dlls

# skip the security test if we are !PROGRAM_SHEPHERDING
ifeq (,$(findstring PROGRAM_SHEPHERDING, $(DEFS)))
EXTRA_INTERFACE_DISABLE := | $(GREP) -vE '/security'
endif
SRCS_DLL_INTERFACE := $(shell $(LS) client-interface/*.dll.c ${DISABLED_INTERFACE} ${EXTRA_INTERFACE_DISABLE})
SRCS_EXE_INTERFACE := $(shell $(LS) client-interface/*.c | $(GREP) -v '\.dll\.c$$' ${DISABLED_INTERFACE} ${EXTRA_INTERFACE_DISABLE})

SRCS_SECURITY_COMMON := $(shell $(LS) security-common/*.c)

ifeq ($(MACHINE), linux)
# FIXME: re-enable signal tests when have time to fix core signal bugs
# FIXME: re-enable vfork* tests when have time to fix core bugs --
# they hang, causing ssh regression process to not return
SRCS_OS := $(shell $(LS) linux/*.c ${DISABLED_OS})
SRCS_PTHREADS := $(shell $(LS) pthreads/*.c)
SRCS_SECURITY_OS := $(shell $(LS) security-linux/*.c)
else
# .dll.c signifies a dependent dll source for the main .c test
SRCS_OS := $(shell $(LS) win32/*.c | $(GREP) -v '\.dll\.c$$' ${DISABLED_OS})
SRCS_OS_NODLL := $(foreach file,$(SRCS_OS),$(if $(wildcard $(subst .c,.dll.c,$(file))),,$(file)))
SRCS_OS_DLL := $(foreach file,$(SRCS_OS),$(if $(wildcard $(subst .c,.dll.c,$(file))),$(file),))

SRCS_PTHREADS := 

# .dll.c signifies a dependent dll source for the main .c test
SRCS_SECURITY_OS := $(shell $(LS) security-win32/*.c | $(GREP) -v '\.dll\.c$$' ${DISABLED_SECURITY_OS})
SRCS_SECURITY_OS_NODLL := $(foreach file,$(SRCS_SECURITY_OS),$(if $(wildcard $(subst .c,.dll.c,$(file))),,$(file)))
SRCS_SECURITY_OS_DLL := $(foreach file,$(SRCS_SECURITY_OS),$(if $(wildcard $(subst .c,.dll.c,$(file))),$(file),))

# .dll.c signifies a dependent dll source for the main .c test
SRCS_RUNALL := $(shell $(LS) runall/*.c | $(GREP) -v '\.dll\.c$$' ${DISABLED_RUNALL})
SRCS_RUNALL_NODLL := $(foreach file,$(SRCS_RUNALL),$(if $(wildcard $(subst .c,.dll.c,$(file))),,$(file)))
SRCS_RUNALL_DLL := $(foreach file,$(SRCS_RUNALL),$(if $(wildcard $(subst .c,.dll.c,$(file))),$(file),))
endif

# we have to have roots separate for win32 .exe suffix
PGMS_ROOT_COMMON := $(patsubst %.c,%,$(SRCS_COMMON))
PGMS_ROOT_COMMON += $(EXTRA_COMMON_TESTS)
PGMS_ROOT_API := $(patsubst %.c,%,$(SRCS_API))
PGMS_ROOT_PROBE_API := $(patsubst %.c,%,$(SRCS_PROBE_API))
PGMS_ROOT_INTERFACE := $(patsubst %.dll.c,%,$(SRCS_DLL_INTERFACE))
PGMS_ROOT_OS := $(patsubst %.c,%,$(SRCS_OS))
PGMS_ROOT_OS += $(EXTRA_OS_TESTS)
PGMS_ROOT_PTHREADS := $(patsubst %.c,%,$(SRCS_PTHREADS))
PGMS_ROOT_SECURITY_COMMON := $(patsubst %.c,%,$(SRCS_SECURITY_COMMON))
PGMS_ROOT_SECURITY_OS := $(patsubst %.c,%,$(SRCS_SECURITY_OS))
PGMS_ROOT := $(PGMS_ROOT_COMMON) $(PGMS_ROOT_API) $(PGMS_ROOT_PROBE_API) $(PGMS_ROOT_INTERFACE) $(PGMS_ROOT_OS) $(PGMS_ROOT_PTHREADS) $(PGMS_ROOT_SECURITY_COMMON) $(PGMS_ROOT_SECURITY_COMMON) $(PGMS_ROOT_SECURITY_OS) 

PGMS_EXE_COMMON := $(patsubst %.c,%$(EXE_SFX),$(SRCS_COMMON))
PGMS_EXE_API := $(patsubst %.c,%$(EXE_SFX),$(SRCS_API))
PGMS_EXE_PROBE_API := $(patsubst %.c,%$(EXE_SFX),$(SRCS_PROBE_API))
PGMS_CLIENT_PROBE_API := $(patsubst %.c,%.client.dll,$(SRCS_PROBE_API))
PGMS_DLL_PROBE_API := $(patsubst %.c,%.dll,$(SRCS_DLL_PROBE_API))
PGMS_PROBES_PROBE_API := $(patsubst %.c,%.dll,$(SRCS_PROBES_PROBE_API))
PGMS_EXE_INTERFACE := $(patsubst %.c,%$(EXE_SFX),$(SRCS_EXE_INTERFACE))
PGMS_DLL_INTERFACE := $(patsubst %.dll.c,%.dll$(LIB_SFX),$(SRCS_DLL_INTERFACE))
ifeq ($(MACHINE), linux)
PGMS_EXE_OS := $(patsubst %.c,%$(EXE_SFX),$(SRCS_OS))
PGMS_EXE_SECURITY_OS := $(patsubst %.c,%$(EXE_SFX),$(SRCS_SECURITY_OS))
else
PGMS_EXE_OS_NODLL := $(patsubst %.c,%$(EXE_SFX),$(SRCS_OS_NODLL))
PGMS_EXE_OS_DLL := $(patsubst %.c,%$(EXE_SFX),$(SRCS_OS_DLL))
PGMS_EXE_SECURITY_OS_NODLL := $(patsubst %.c,%$(EXE_SFX),$(SRCS_SECURITY_OS_NODLL))
PGMS_EXE_SECURITY_OS_DLL := $(patsubst %.c,%$(EXE_SFX),$(SRCS_SECURITY_OS_DLL))
endif
PGMS_EXE_PTHREADS := $(patsubst %.c,%$(EXE_SFX),$(SRCS_PTHREADS))
PGMS_EXE_SECURITY_COMMON := $(patsubst %.c,%$(EXE_SFX),$(SRCS_SECURITY_COMMON))
PGMS_EXE_RUNALL_NODLL := $(patsubst %.c,%$(EXE_SFX),$(SRCS_RUNALL_NODLL))
PGMS_EXE_RUNALL_DLL := $(patsubst %.c,%$(EXE_SFX),$(SRCS_RUNALL_DLL))
PGMS_EXE := $(PGMS_EXE_COMMON) $(PGMS_EXE_API) $(PGMS_EXE_PROBE_API) $(PGMS_CLIENT_PROBE_API) $(PGMS_DLL_PROBE_API) $(PGMS_PROBES_PROBE_API) $(PGMS_EXE_INTERFACE) $(PGMS_DLL_INTERFACE) $(PGMS_EXE_OS_NODLL) $(PGMS_EXE_OS_DLL) $(PGMS_EXE_OS) $(PGMS_EXE_PTHREADS) $(PGMS_EXE_SECURITY_COMMON) $(PGMS_EXE_SECURITY_OS_NODLL) $(PGMS_EXE_SECURITY_OS_DLL) $(PGMS_EXE_RUNALL_NODLL) $(PGMS_EXE_RUNALL_DLL) 

TEMPLATE_COMMON := $(shell $(LS) common/*.template security-common/*.template)
PGMS_EXP_COMMON := $(patsubst %.template, %.expect, $(TEMPLATE_COMMON))

ifeq  ($(MACHINE), linux)
TEMPLATE_OS := $(shell $(LS) security-linux/*.template linux/*.template)
else
TEMPLATE_OS := $(shell $(LS) security-win32/*.template win32/*.template runall/*.template)
endif
PGMS_EXP_OS := $(patsubst %.template, %.expect, $(TEMPLATE_OS))

TEMPLATE_INTERFACE := $(shell $(LS) client-interface/*.template)
PGMS_EXP_INTERFACE := $(patsubst %.template, %.expect, $(TEMPLATE_INTERFACE))

TEMPLATE_API := $(shell $(LS) api/*.template)
PGMS_EXP_API := $(patsubst %.template, %.expect, $(TEMPLATE_API))

TEMPLATE_PROBE_API := $(shell $(LS) probe-api/*.template)
PGMS_EXP_PROBE_API := $(patsubst %.template, %.expect, $(TEMPLATE_PROBE_API))

#for testing run all, done through a shell script
RUNALL_SCRIPTS := $(shell $(LS) runall/*.runall ${DISABLED_RUNALL})
RUNALL_ROOTS := $(patsubst %.runall,%,$(RUNALL_SCRIPTS))
# runall tests that do NOT open a window
RUNALL_NOGUI_SCRIPTS := $(shell $(GREP) -l '<nowindow>' runall/*.runall ${DISABLED_RUNALL})
RUNALL_NOGUI_ROOTS := $(patsubst %.runall,%,$(RUNALL_NOGUI_SCRIPTS))

# need absolute dir ref.
CURWD := $(CURDIR)
ifeq ($(MACHINE), win32)
  CURWD := $(shell $(CYGPATH) -ma $(CURWD))
endif

ifeq ($(MACHINE), win32) 
  DYNAMORIO_WINPREINJECT := $(CURWD)/drpreinject.dll
  DYNAMORIO_PREINJECT := $(shell $(CYGPATH) -ua $(DYNAMORIO_WINPREINJECT))
endif

ifeq ($(MACHINE), linux)
all: run
else
all: $(DYNAMORIO_PREINJECT) run
endif


# this is much like runregression
runlog: 
	$(MAKE) --no-print-directory run | $(TEE) testlog
	$(MAKE) --no-print-directory proclog

proclog:
	@$(ECHO) `$(GREP) -c PASS testlog` passed `$(GREP) -c FAIL testlog` failed
	@$(GREP) -c FAIL testlog > /dev/null && $(ECHO) Failed || $(ECHO) Success

savelog:
	$(MV) testlog testlog.`$(DATE) +"%Y-%b%d-%H:%M"`


$(PGMS_EXP_COMMON): %.expect: %.template
	$(PERL) gen_expect.pl '$(DEFS)' '$(DR_OPS)' $*.template

TOOLS := tools.h tools.c tools_asm.$(OBJ_SFX)

##################################################
ifeq ($(MACHINE), win32)
TYPE_EXE := VFT_APP
TYPE_DLL := VFT_DLL

RESOURCE_FILE := $(shell $(CYGPATH) -ua $(DYNAMORIO_HOME)/../${CORE_DIRNAME}/win32/resources.rc)
WINRESOURCE_FILE := $(shell $(CYGPATH) -ma ${RESOURCE_FILE})
DYNAMORIO_WINDIR := $(shell $(CYGPATH) -ma ${DYNAMORIO_BASE})
RCFLAGS := $(MSINCLUDE) -r

dresources.res: $(RESOURCE_FILE)
	$(RC) $(RCFLAGS) -fo $(subst /,\\,$@) /I "$(DYNAMORIO_WINDIR)\\${CORE_DIRNAME}\\lib" $(D)FILE_NAME=\"\" $(D)FILE_TYPE=$(TYPE_EXE) $(D)FILE_DESCRIPTION=\"regression_test\" $(WINRESOURCE_FILE)

mresources.res: mresources.rc
	$(RC) $(RCFLAGS) -fo $(subst /,\\,$@) mresources.rc

oresources.res: oresources.rc
	$(RC) $(RCFLAGS) -fo $(subst /,\\,$@) oresources.rc

# using %$(EXE_SFX) doesn't seem to work well on win32, so we duplicate
# we send output to a file since cl prints out crap like file name
$(PGMS_EXE_PROBE_API): LINK += /MAP     # all probe exes/dlls need map files
$(PGMS_EXE_COMMON)    \
$(PGMS_EXE_PROBE_API) \
$(PGMS_EXE_INTERFACE): %.exe: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK) > $*.clout 2>&1

# we add dr resource version information to only some of our security tests to test both
# sides of the -use_moduledb functionality, and ensure the matching works
$(PGMS_EXE_SECURITY_OS_NODLL): %.exe: %.c $(TOOLS) dresources.res
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK) dresources.res > $*.clout 2>&1

# we support a single .dll statically linked w/ a runall/ test
# we add microsoft like version information to some tests to better test -use_moduledb
$(PGMS_EXE_RUNALL_DLL): %.exe: %.c $(TOOLS) %.dll.dll mresources.res
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK) mresources.res $*.dll.lib > $*.clout 2>&1

# copy existing test
%-stress.exe: %.exe
	$(CP) -f $< $@
%-stress.expect: %.template
	$(CP) -f $< $*-stress.template
	$(PERL) gen_expect.pl '$(DEFS)' '$(DR_OPS)' $*-stress.template

$(PGMS_EXE_OS_NODLL) $(PGMS_EXE_RUNALL_NODLL): %.exe: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK) > $*.clout 2>&1

# we support a single .dll statically linked w/ a win32/ test
$(PGMS_EXE_OS_DLL) $(PGMS_EXE_SECURITY_OS_DLL): %.exe: %.c $(TOOLS) %.dll.dll
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK) $*.dll.lib > $*.clout 2>&1

%.dll: %.c $(TOOLS)
	$(CC_BASE) $< $(LINK) $(DLLOUT)$@ $(LIBOUT)$*.lib > $*.clout 2>&1

%.bind: %
	$(BIND) -u $<

# PR 230274: w/ new compiler we have to explicitly link to libc before dynamorio.lib 
# or else we get floating point callout failures from DR exporting sin, cos,
# and _fltused (from PR 219380).  However, if we link to msvcrt.lib we hit
# CRT SxS issues (PR 236914), so make sure to use libcmt.lib (static libc).
$(PGMS_EXE_API): %.exe: %.c $(TOOLS) $(DYNAMORIO_DIR)
	$(CC_BASE_DYNAMORIO) $(D)USE_DYNAMO $(EXEOUT)$@ $< /link /nodefaultlib libcmt.lib $(LINK_DYNAMORIO) tools_asm.$(OBJ_SFX) > $*.clout 2>&1

ifeq ($(ARCH), x64)
CLIENT_LINK := /base:"0x72000000" /FIXED
else
CLIENT_LINK := 
endif
$(PGMS_DLL_INTERFACE): %.dll.dll: %.dll.c
	$(CC_BASE_INTERFACE) $< $(DYNAMORIO_INC) /link /NODEFAULTLIB /NOENTRY /LIBPATH:$(DYNAMORIO_LIBDIR) dynamorio.lib /DLL /OUT:$@ $(CLIENT_LINK)

# Just like client interface tests, probe clients can't depend upon $(TOOLS).
# Note: client dlls need /MAP too because probe callback functions can be in 
# in client dlls.
$(PGMS_CLIENT_PROBE_API): %.dll: %.c
	$(CC_BASE_INTERFACE) $< $(DYNAMORIO_INC) /link /NODEFAULTLIB /NOENTRY /LIBPATH:$(DYNAMORIO_LIBDIR) dynamorio.lib /MAP /DLL /OUT:$@

$(PGMS_EXP_INTERFACE): %.expect: %.template
	$(PERL) gen_expect.pl '$(DEFS) $(RUNREGRESSION_OS_VER)' '$(DR_OPS)' $*.template

$(PGMS_EXP_API): %.expect: %.template
	$(PERL) gen_expect.pl '$(DEFS) $(RUNREGRESSION_OS_VER)' '$(DR_OPS)' $*.template

$(PGMS_EXE_PTHREADS): %.exe: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK) -lpthread > $*.clout 2>&1

# we add non-ms non-dr version information to some tests to better test -use_moduledb
$(PGMS_EXE_SECURITY_COMMON): %.exe: %.c $(TOOLS) oresources.res
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK) oresources.res > $*.clout 2>&1

$(PGMS_EXP_OS) $(PGMS_EXP_PROBE_API): %.expect: %.template
	$(PERL) gen_expect.pl '$(DEFS)' '$(DR_OPS)' $*.template

# dlls used by both hotp & probe tests (not probe or client dlls) need map files
$(PGMS_DLL_PROBE_API): %.dll: %.c $(TOOLS)
	$(CC_BASE) $(OPT) /Fm $< $(LINK) $(DLLOUT)$@ > $*.clout 2>&1

# Probes need DR API header files, and those in turn need stdio.h.
$(PGMS_PROBES_PROBE_API): HOTP_DLL_CFLAGS += $(DYNAMORIO_INC) $(MSINCLUDE)
# dlls containing probes or hot patches are built the same way. Also these dlls
# shouldn't depend upon $(TOOLS) as $(TOOLS) links agains external dlls.
$(PGMS_PROBES_PROBE_API): %.dll: %.c $(HOTP_DLL_INC_DIR)/hotpatch_interface.h
	$(CC) $(HOTP_DLL_CFLAGS) $(I)$(HOTP_DLL_INC_DIR) $(OUT)$*.obj $< > $*.clout 2>&1
	$(LD) $(HOTP_DLL_LDFLAGS) $(DLLOUT)$@ $*.obj >> $*.clout 2>&1

##################################################
else # linux

$(PGMS_EXE_COMMON): %: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK)
$(PGMS_EXE_OS): %: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK)

# extra runs that use existing apps: we copy the app and expect file
%-reset: %
	$(CP) -f $< $@
%-reset.expect: %.expect
	$(CP) -f $< $@
# copy existing test
%-stress: %
	$(CP) -f $< $@
%-stress.expect: %.template
	$(CP) -f $< $*-stress.template
	$(PERL) gen_expect.pl '$(DEFS)' '$(DR_OPS)' $*-stress.template
# execve{32,64}
%32: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK)
%32.expect: %.expect
	$(CP) -f $< $@
%64: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK)
%64.expect: %.expect
	$(CP) -f $< $@

$(PGMS_EXE_API): %: %.c $(TOOLS) $(DYNAMORIO_DIR)
	$(CC_BASE_DYNAMORIO) $(D)USE_DYNAMO $(EXEOUT)$@ $< $(LINK_DYNAMORIO) tools_asm.$(OBJ_SFX) 
$(PGMS_EXE_INTERFACE): %: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK)

  ifeq ($(ARCH), x64)
    # Currently required on 64bit that a client library have a
    # preferred base address in the lower 2GB
    PREFERRED_BASE := 0x72000000
    LD_SCRIPT := ldscript
    LD_USE_SCRIPT := -Xlinker $(LD_SCRIPT_OPTION) -Xlinker $(LD_SCRIPT)
$(LD_SCRIPT):
    # To set a preferred base address we tweak the default linker script
	ld --verbose | grep -A 1000 SECTIONS | grep -B 1000 '^}' | \
	  sed 's/0x400000/$(PREFERRED_BASE)/g; /^{/a . = $(PREFERRED_BASE);' > $@
  else
    CFLAGS := -m32 $(DBG) $(D)LINUX
    LD_USE_SCRIPT :=
  endif
$(PGMS_DLL_INTERFACE): %.dll.so: %.dll.c $(LD_SCRIPT)
	$(CC_BASE_INTERFACE) $(DYNAMORIO_INC) -fPIC -shared -nostartfiles $< -o $@ \
        $(LD_USE_SCRIPT)

$(PGMS_EXP_INTERFACE): %.expect: %.template
	$(PERL) gen_expect.pl '$(DEFS)' '$(DR_OPS)' $*.template
$(PGMS_EXP_API): %.expect: %.template
	$(PERL) gen_expect.pl '$(DEFS)' '$(DR_OPS)' $*.template
$(PGMS_EXE_PTHREADS): %: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK) -lpthread
$(PGMS_EXE_SECURITY_COMMON): %: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK)
$(PGMS_EXE_SECURITY_OS): %: %.c $(TOOLS)
	$(CC_BASE) $(EXEOUT)$@ $< $(LINK)
$(PGMS_EXP_OS): %.expect: %.template
	$(PERL) gen_expect.pl '$(DEFS)' '$(DR_OPS)' $*.template

# we do NOT want CC_BASE as it links w/ tools_asm
linux/execve-sub32: linux/execve-sub.c
	$(CC) -m32 $(EXEOUT)$@ $<
linux/execve-sub64: linux/execve-sub.c
	$(CC) -m64 $(EXEOUT)$@ $<

endif # linux

# for clean (xref PR 245246)
ALL_TEMPLATES := $(shell $(LS) */*.template)
ALL_GENERATED_EXPECT := $(patsubst %.template, %.expect, $(ALL_TEMPLATES))

##################################################

###########################################################################
# test-specific rules

TEST_ARGS :=

win32/nativeexec.runinjector: DR_OPS += -native_exec_list nativeexec.dll.dll
win32/nativeterminate.runinjector: DR_OPS += -native_exec_list nativeterminate.dll.dll

linux/clone-reset.runinjector: DR_OPS += -reset_at_fragment_count 100
linux/thread-reset.runinjector: DR_OPS += -reset_at_fragment_count 100

linux/execve32.runinjector: linux/execve-sub32
linux/execve32.runinjector: TEST_ARGS = $(CURWD)/linux/execve-sub32
linux/execve64.runinjector: linux/execve-sub64
linux/execve64.runinjector: TEST_ARGS = $(CURWD)/linux/execve-sub64

common/decode-stress.runinjector: DR_OPS += -stress_recreate_state

runall/calc-freeze.runrunall: DR_OPS += -coarse_enable_freeze
# derek: I'm putting -no_probe_api here since the check below is not
#   triggering over remote regression runs and I don't want to take the
#   time to figure it out
runall/calc-persist.runrunall: DR_OPS += -coarse_enable_freeze -use_persisted -no_probe_api
ifneq (,$(filter /DCLIENT_INERFACE, $(DEFS)))
# xref PR 226578 which tracks incompatibility between probe_api & pcaches
runall/calc-persist.runrunall: DR_OPS += -no_probe_api
endif

win32/delaybind.runnative win32/delaybind.runinjector: LINK += /delayload:delaybind.dll.dll /delay:unload
win32/delaybind.runnative win32/delaybind.runinjector: win32/delaybind.exe.bind win32/delaybind.dll.dll.bind
security-win32/sec-fixed.dll.dll: LINK += /FIXED
security-win32/secalign-fixed.dll.dll: LINK += /FIXED
# PR 229292: we want over-ssh and local builds to match the template so
# we always build w/o pdbs
security-win32/except-execution.exe: DBG=
win32/membar.exe: CC_BASE += $(OPT) # need optimizations

ifeq ($(MACHINE), linux)
# just happens to not make as many traces
client-interface/events.runinterface: DR_OPS += -trace_threshold 5
endif

client-interface/security.runinterface: DR_OPS += -security_api
client-interface/thread.runinterface: CLIENT_OPS := -paramx -paramy
client-interface/file_io.runinterface: CLIENT_OPS := $(CURWD)/client-interface/file_io_data.txt
client-interface/cbr4.runinterface: DR_OPS += -thread_private
# See PR 244090 - currently there is no way for a client to see what options DR was 
# run with.  The flush tests needs to know whether it can use the unlink
# flush routines or not which depends on having either -thread_private or -enable_full_api
# We check here and pass a client option if the unlink flush is safe to use.
ifneq (,$(filter -thread_private -enable_full_api, $(DR_OPS)))
client-interface/flush.runinterface: CLIENT_OPS := use_unlink
endif

api/dis.runapi: DR_OPS += -syntax_intel
api/dis.runapi: EXE_ARGS = $(CURWD)/api/dis-udis86-randtest.raw

###########################################################################
### for test suite

# API needs to be built with dynamo and then run natively
# All others are built as native programs and run using injector

TIMER_TIME = -s 60

ifeq ($(MACHINE), win32)
TIMER_NATIVE     = $(BUILD_TOOLS)/winstats.exe $(TIMER_TIME) -silent
DYNAMORIO_DLL := $(subst /,\\,$(shell $(CYGPATH) -m $(DYNAMORIO_DIR)/dynamorio.dll))
TIMER_INJECTOR = export DYNAMORIO_OPTIONS='$(DR_OPS)'; \
    $(DYNAMORIO_BINDIR)/drinject.exe $(TIMER_TIME) $(DYNAMORIO_DLL)
TIMER_API = export DYNAMORIO_OPTIONS='$(DR_OPS)'; \
    $(DYNAMORIO_BINDIR)/drinject.exe $(TIMER_TIME) -noinject
else
TIMER_NATIVE     = $(BUILD_TOOLS)/runstats $(TIMER_TIME) -silent
TIMER_INJECTOR   = $(BUILD_TOOLS)/runstats $(TIMER_TIME) -silent \
   -env LD_LIBRARY_PATH ':$(DYNAMORIO_DIR):$(LD_LIBRARY_PATH)' \
   -env LD_PRELOAD 'libdrpreload.so libdynamorio.so' \
   -env DYNAMORIO_OPTIONS '$(DR_OPS)'
TIMER_API   = $(BUILD_TOOLS)/runstats $(TIMER_TIME) -silent \
   -env DYNAMORIO_OPTIONS '$(DR_OPS)'
endif

# to ignore win32/unix file differences we do -b
DIFF += -b

ifeq ($(MACHINE), win32) 
#only for win32
$(DYNAMORIO_PREINJECT):
	@(if $(LS) $(DYNAMORIO_HOME)/$(LIB_SUBDIR)/drpreinject.dll > .trash 2>&1; then \
		$(CP) $(DYNAMORIO_HOME)/$(LIB_SUBDIR)/drpreinject.dll .; \
	else \
		$(MAKE) -s -C $(DYNAMORIO_HOME)/../${CORE_DIRNAME} DYNAMORIO_BASE=.. DEBUG=0 clean install; \
		$(CP) $(DYNAMORIO_HOME)/$(LIB_SUBDIR)/drpreinject.dll .; \
	fi)
endif

# we run API tests first as those are more easily broken
run: runheader runinterface runapi runnointerface

# runallbuthotp is used only as solution for PR 234043
runallbuthotp: runheader runinterface runapi runbase runsecurity runrunall

runnointerface: runbase runsecurity runrunall

runnogui: runbase runsecurity runrunall-nogui

runnorunall: runbase runsecurity

# we split these up for more parallelism w/ make -j
runbase: runcommon runos

runapi: reportbuild $(patsubst %,%.runapi,$(PGMS_ROOT_API))

runinterface: reportbuild $(patsubst %,%.runinterface,$(PGMS_ROOT_INTERFACE))

runcommon: reportbuild $(patsubst %,%.runinjector,$(PGMS_ROOT_COMMON))

runprobeapi: reportbuild $(patsubst %,%.runprobeapi,$(PGMS_ROOT_PROBE_API))

runos: reportbuild $(patsubst %,%.runinjector,$(PGMS_ROOT_OS))

# FIXME: watch these: xref PR 205306 (bug 828)
# they used to not die nicely and time out
ifeq ($(MACHINE), linux)
runos: $(patsubst %,%.runinjector,$(PGMS_ROOT_PTHREADS))
endif

# we split these up for more parallelism w/ make -j
runsecurity: runbasesecurity runossecurity
runbasesecurity: reportbuild $(patsubst %,%.runinjector,$(PGMS_ROOT_SECURITY_COMMON))

runossecurity: reportbuild $(patsubst %,%.runinjector,$(PGMS_ROOT_SECURITY_OS))

ifeq ($(MACHINE), win32)
runrunall-nogui: reportbuild $(patsubst %,%.runrunall,$(RUNALL_NOGUI_ROOTS))
else
runrunall-nogui:
endif

ifeq ($(MACHINE), win32)
runrunall: reportbuild $(patsubst %,%.runrunall,$(RUNALL_ROOTS))
else
runrunall:
endif

runheader:
	@$(ECHO) "Disabled tests are -- ${DISABLED_ALL} --"

reportbuild:
	@$(ECHO) "Using build in $(DYNAMORIO_DIR)"

# FIXME: share the parallelized runinjector cmd split
runnative: $(patsubst %,%.runnative,$(PGMS_ROOT_API)) \
           $(patsubst %,%.runnative,$(PGMS_ROOT_COMMON)) \
           $(patsubst %,%.runnative,$(PGMS_ROOT_OS)) \
           $(patsubst %,%.runnative,$(PGMS_ROOT_PTHREADS)) \
           $(patsubst %,%.runnative,$(PGMS_ROOT_SECURITY_COMMON)) \
           $(patsubst %,%.runnative,$(PGMS_ROOT_SECURITY_OS)) 

# FIXME: share the parallelized runinjector cmd split
expect: $(patsubst %,%.runexpect,$(PGMS_ROOT_API)) \
        $(patsubst %,%.runexpect,$(PGMS_ROOT_COMMON)) \
        $(patsubst %,%.runexpect,$(PGMS_ROOT_OS)) \
        $(patsubst %,%.runexpect,$(PGMS_ROOT_PTHREADS)) \
        $(patsubst %,%.runexpect,$(PGMS_ROOT_SECURITY_COMMON)) \
        $(patsubst %,%.runexpect,$(PGMS_ROOT_SECURITY_OS))

%.announce:
ifeq ($(MACHINE), linux)
	(if ! test -e $(BUILD_TOOLS)/runstats; then \
	  $(MAKE) -C $(DYNAMORIO_TOOLS); fi)
endif
	@(if ! test -e $(RESDIR); then $(MKDIR) $(RESDIR); fi)
	@(if ! test -e $(RESDIR)/$(dir $*); then $(MKDIR) -p $(RESDIR)/$(dir $*); fi)
	@(if ! test -e $$DYNAMORIO_LOGDIR; then $(MKDIR) -p $$DYNAMORIO_LOGDIR; fi)
	@(if ! test -e $$DYNAMORIO_CACHE_ROOT; then $(MKDIR) -p $$DYNAMORIO_CACHE_ROOT; fi)
	@$(ECHO)
	@$(ECHO) ----- $* ----------------------

.PHONY : %.runrunall 

%.runrunall: $(DYNAMORIO_PREINJECT) %.announce %.expect
ifeq ($(MACHINE), win32)
	@if test -e $*.c; then $(MAKE) -s $*.exe; fi
	@cd $(RESDIR); export DYNAMORIO_OPTIONS='$(DR_OPS)'; $(SHELL) $(CURWD)/runall/runalltest.sh $(CURWD)/$*.runall $(DYNAMORIO_LIBDIR)\\dynamorio.dll $(DYNAMORIO_WINPREINJECT) $(RUNALL_MAX_WAIT) $(ARCH) > $*.out 2>&1; cd $(CURWD)
	@if $(DIFF) -i $(CURWD)/$*.expect $(RESDIR)/$*.out; then $(ECHO) "PASS"; else $(ECHO) "FAIL"; fi
	@$(ECHO)
	@(if $(LS) $(CURWD)/$*.template > .trash 2>&1; then $(MV) $(CURWD)/$*.expect $(CURWD)/$*.expect.last; fi)
else
	@$(ECHO) "run all not supported on linux"
endif

ifeq ($(MACHINE), win32)
# again, we have to duplicate, b/c %$(EXE_SFX) doesn't work
%.runnative: %.announce %.exe 
else
%.runnative: %.announce % 
endif
ifeq ($(MACHINE), win32)
	@cd $(RESDIR); $(TIMER_NATIVE) $(CURWD)/$*$(EXE_SFX) > $*.out 2>&1; cd $(CURWD)
else
	@cd $(RESDIR); $(TIMER_NATIVE) $(CURWD)/$*$(EXE_SFX) > $*.out 2>&1; cd $(CURWD)
endif
	@(if $(LS) $(CURWD)/$*.template > .trash 2>&1; then $(PERL) gen_expect.pl "DNATIVE" "" $(CURWD)/$*.template; fi)	
	@(if $(DIFF) $(CURWD)/$*.expect $(RESDIR)/$*.out; then $(ECHO) "PASS"; else $(ECHO) "FAIL"; fi)
	@$(ECHO)	
	@(if $(LS) $(CURWD)/$*.template > .trash 2>&1; then $(MV) $(CURWD)/$*.expect $(CURWD)/$*.expect.last; fi)	

ifeq ($(MACHINE), win32)
# again, we have to duplicate, b/c %$(EXE_SFX) doesn't work
%.runapi: %.announce %.exe %.expect
else
%.runapi: %.announce % %.expect
endif
# set ops for API runs
# must copy dll to . so on path for win32 (must be syswide path)
# for PGMS_EXE_API
ifeq ($(MACHINE), win32)
	@cd $(RESDIR); $(CP) $(DYNAMORIO_DIR)/dynamorio.dll .; $(TIMER_API) $(CURWD)/$*$(EXE_SFX) $(EXE_ARGS) > $*.out 2>&1; cd $(CURWD)
else
	@cd $(RESDIR); $(TIMER_API) $(CURWD)/$*$(EXE_SFX) $(EXE_ARGS) > $*.out 2>&1; cd $(CURWD)
endif
	@(if $(DIFF) $(CURWD)/$*.expect $(RESDIR)/$*.out; then $(ECHO) "PASS"; else $(ECHO) "FAIL"; fi)
	@$(ECHO)	
	@(if $(LS) $(CURWD)/$*.template > .trash 2>&1; then $(MV) $(CURWD)/$*.expect $(CURWD)/$*.expect.last; fi)	

# We have two types of client interface tests: 1) a client library + a custom
# C test program we compile, and 2) a client library + a standard win32 
# executable we launch with the runall script.  The presence of a .runall
# input file vs. a .exe distinguishes the two rules below
ifeq ($(MACHINE), win32)
%.runinterface: DR_OPS += -code_api -client_lib "$(shell $(CYGPATH) -da $*.dll.dll);0;$(CLIENT_OPS)"
%.runinterface: $(DYNAMORIO_PREINJECT) %.exe %.dll.dll %.announce %.expect
else
%.runinterface: DR_OPS += -code_api -client_lib "$(CURWD)/$*.dll.so;0;$(CLIENT_OPS)"
%.runinterface: % %.dll.so %.announce %.expect
endif
	@cd $(RESDIR); $(TIMER_INJECTOR) $(CURWD)/$*$(EXE_SFX) > $*.out 2>&1; cd $(CURWD)
	@(if $(DIFF) $(CURWD)/$*.expect $(RESDIR)/$*.out; then $(ECHO) "PASS"; else $(ECHO) "FAIL"; fi)
	@$(ECHO)
	@(if $(LS) $(CURWD)/$*.template > .trash 2>&1; then $(MV) $(CURWD)/$*.expect $(CURWD)/$*.expect.last; fi)

ifeq ($(MACHINE), win32)
%.runinterface: $(DYNAMORIO_PREINJECT) %.dll.dll %.runall %.announce %.expect
	@cd $(RESDIR); export DYNAMORIO_OPTIONS='$(DR_OPS)'; $(SHELL) $(CURWD)/runall/runalltest.sh $(CURWD)/$*.runall $(DYNAMORIO_LIBDIR)\\dynamorio.dll $(DYNAMORIO_WINPREINJECT) $(INTERFACE_MAX_WAIT) $(ARCH) > $*.out 2>&1; cd $(CURWD)
	@if $(DIFF) -i $(CURWD)/$*.expect $(RESDIR)/$*.out; then $(ECHO) "PASS"; else $(ECHO) "FAIL"; fi
	@$(ECHO)
	@(if $(LS) $(CURWD)/$*.template > .trash 2>&1; then $(MV) $(CURWD)/$*.expect $(CURWD)/$*.expect.last; fi)
else
%.runinterface: %.dll.so %.runall %.announce %.expect
	@$(ECHO) "SKIP - client interface runall not implemented on linux"
endif

%.runprobeapi: DR_OPS += -code_api -probe_api -client_lib "$(shell cygpath -da $*.client.dll);0;$(CLIENT_OPS)"
# A probe-api test consists of an exe (test.exe) and a client
# (test.client.dll).  Test dlls (test.dll.dll - for now allowing just one) and
# probe dlls (test.probe*.dll) are optional.  By using SECONDEXPANSION
# the dependency on a varying number of dlls can be enforced
.SECONDEXPANSION:
%.runprobeapi: %.exe %.client.dll \
    $$(subst .c,.dll,$$(wildcard $$(@D)/$$*.probe*.c $$(@D)/$$*.dll.c)) \
    %.announce %.expect
ifeq ($(MACHINE), win32)
	@cd $(RESDIR); $(TIMER_INJECTOR) $(CURWD)/$*$(EXE_SFX) > $*.out 2>&1; cd $(PWD)
	@if $(DIFF) -i $(PWD)/$*.expect $(RESDIR)/$*.out; then echo "PASS"; else echo "FAIL"; fi
	@echo
	@(if ls $(PWD)/$*.template > .trash 2>&1; then mv -f $(PWD)/$*.expect $(PWD)/$*.expect.last; fi)
else
	@echo "SKIP - probe api NYI on linux"
endif

ifeq ($(MACHINE), win32)
# again, we have to duplicate, b/c %$(EXE_SFX) doesn't work
%.runinjector: %.announce %.exe %.expect
# injector includes timer args
	@cd $(RESDIR); $(TIMER_INJECTOR) $(CURWD)/$*$(EXE_SFX) $(TEST_ARGS) > $*.out 2>&1; cd $(CURWD)
else
%.runinjector: %.announce % %.expect
# timer includes injector args
	@cd $(RESDIR); $(TIMER_INJECTOR) $(CURWD)/$* $(TEST_ARGS) > $*.out 2>&1; cd $(CURWD)
endif
	@(if $(DIFF) $(CURWD)/$*.expect $(RESDIR)/$*.out; then $(ECHO) "PASS"; else $(ECHO) "FAIL"; fi)
	@$(ECHO)
	@(if $(LS) $(CURWD)/$*.template > .trash 2>&1; then $(MV) $(CURWD)/$*.expect $(CURWD)/$*.expect.last; fi)

# put .out in same dir as .expect
ifeq ($(MACHINE), win32)
# again, we have to duplicate, b/c %$(EXE_SFX) doesn't work
%.runexpect: %.announce %.exe %.expect
else
%.runexpect: %.announce % %.expect
endif
	$(TIMER_NATIVE) $*$(EXE_SFX) > $*.out 2>&1; cd .
	$(DIFF) $*.expect $*.out; cd .
	@$(ECHO)
	@(if $(LS) $(CURWD)/$*.template > .trash 2>&1; then $(MV) $(CURWD)/$*.expect $(CURWD)/$*.expect.last; fi)

# can't get brace expansion to work on cygwin!
clean:
	$(RM) tools_asm.$(OBJ_SFX) tools_asm.s
	$(RM) $(PGMS_EXE) $(EXTRA_TESTS_EXE) .trash
	$(RM) */*.out */*.clout */*.$(OBJ_SFX) */*.pdb */*.ilk */*.opt */*.c~ */*.expect.last */*.dll */*.dll.so */*.lib */*.exp */*.manifest */*.s
	$(RM) *.obj *.pdb *.clout *.trash drpreinject.dll *.res
	$(RM) $(ALL_GENERATED_EXPECT)
	$(RM) -r $(RESDIR)
	$(RM) $(patsubst %,%.expect,$(EXTRA_TESTS))
	$(RM) $(patsubst %,%.template,$(EXTRA_TESTS))
ifeq ($(MACHINE), linux)
	$(RM) linux/execve linux/execve-sub{32,64}
endif
