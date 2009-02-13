# **********************************************************
# Copyright (c) 2000-2009 VMware, Inc.  All rights reserved.
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

# Copyright (c) 2003-2007 Determina Corp.
# Copyright (c) 2001-2003 Massachusetts Institute of Technology
# Copyright (c) 2001 Hewlett Packard Company

###########################################################################
###########################################################################
###
### Makefile for building DynamoRIO
###
### This Makefile's primary target is x86 and x64, either WinNT or Linux.
### It is intended to be usable either on Linux or on WinNT
### via cygwin (gnu make is required).  
###
### A separate build directory tree is used.
### This is the only Makefile -- no Makefiles in subdirectories.
### See the section "Configuration Values" on customizing the build defines.
###
### To stick a custom message into the .rsrc properties for windows
### define SPECIAL_BUILD (i.e. make SPECIAL_BUILD="experimental fix for case xxx")

ETAGS	= /usr/bin/etags
MAKE	= make

ifndef DYNAMORIO_MAKE
  DYNAMORIO_MAKE := ../make
endif
include $(DYNAMORIO_MAKE)/compiler.mk

# windows vs. linux flags, etc.
ifeq ($(MACHINE), linux)
CFLAGS  =-std=gnu99 # -std=c99 doesn't quite work
# FIXME case 191480: we used to pass -pedantic just to cpp;
# now w/ no separate cpp step we should eliminate the
# warnings and pass -pedantic to main gcc
WARN    =-Wall -Werror
OPT     =-O3
DBG     =-g3
# FIXME: perhaps we should optimize but with -fno-omit-frame-pointer?
# disable intrinsic to avoid stack overflow in set_dynamo_options(): case 7853
DBG_OPT =-fno-builtin-strcmp
OBJ_SFX =o
EXE_SFX =   # note . would be included here
ifeq ($(STATIC), 1)
SO_SFX  =.a
else # non-STATIC
SO_SFX  =.so
endif
else # win32
WARN    =/W4 /WX
OPT     =/O2
DBG     =/Zi
LINK_DBG=/debug
# do not use /O2 on windows, it messes up debug info!
DBG_OPT =
# FIXME: further debug checking for CL 13: /RTCc 
OBJ_SFX =obj
EXE_SFX =.exe
SO_SFX  =.dll
endif

# Our dependence support uses gcc on both platforms
GCC     =gcc
DEP_SFX =dep
DEPENDS =-MM -MG -MP # no system headers, generated get phony targets

# src/ or core/
CORE_DIRNAME :=core
SRC	     :=.
HTML	     :=$(DYNAMORIO_BASE)/html
HTML_BACK    :=../$(CORE_DIRNAME)

###########################################################################
##
## Target & Compiler/Linker Flags

# target ARCH
ifeq ($(ARCH), x64)
ARCH_DEFS = $(D)X86 $(D)X64
ARCH_DIRNAME := x86
IS_X86 := 1
else
ifeq ($(ARCH), x86)
ARCH_DEFS = $(D)X86
ARCH_DIRNAME := x86
IS_X86 := 1
else
$(error Unsupported arch $(ARCH))
endif
endif
ARCH_SRC := $(SRC)/$(ARCH_DIRNAME)

# target OS (properties for OS variants are below)
ifeq ($(MACHINE), linux)
OSNAME = linux
OS_DEFS = $(D)LINUX
else # win32
OSNAME = win32
OS_DEFS = $(D)WINDOWS
endif
OS_SRC = $(SRC)/$(OSNAME)

# NOTE we use LIBDIR instead of LIB to avoid shadowing the env. variable
# LIB when using make runregression on the local host
LIBDIR = lib
LIB_SRC = $(SRC)/$(LIBDIR)

ifndef PROGRAMFILES
ifdef HOMEDRIVE
PROGRAMFILES = $(HOMEDRIVE)\\PROGRAM FILES
else
PROGRAMFILES = C:\\PROGRAM FILES
endif
endif

# Include directories
# NOTE we use INCLUDE_PATHS instead of INCLUDE to avoid shadowing the env.
# variable INCLUDE when using make runregression on the local host, if buildtools
# is set then this isn't a problem so it's okay that Buildtools Setup.mk sets INCLUDE
ifeq ($(MACHINE), linux)
  INCLUDE_PATHS = $(I)$(SRC) $(I)$(ARCH_SRC) $(I)$(OS_SRC) $(I)$(LIB_SRC)
else
  INCLUDE_PATHS = $(I)$(subst /,\\,$(SRC)) $(I)$(subst /,\\,$(ARCH_SRC)) \
                  $(I)$(subst /,\\,$(OS_SRC)) $(I)$(subst /,\\,$(LIB_SRC)) \
                  $(MSINCLUDE)
endif

###########################################################################
# 
# Configuration Values
#
# The configuration to build can be specified on the cmdline by overriding the default
# values here or in a file named Makefile.mydefines which we include below.  To specify
# individual defines from the section "definitions for conditional compilation" set
# ADD_DEFINES.
#
# The common configurations are specfied with :
# [VPS|VMSAFE|VMAP]=1 INERNAL=[0,1] DEBUG=[0,1]  (default is VMSAFE=1 INTERNAL=1 DEBUG=1)
#
# INTERNAL - specifies whether this build is for internal use only or for customer's use
# DEBUG - specifies a debug build
# INTERNAL=0 DEBUG=0 is an external release build
# INTERNAL=0 DEBUG=1 is an external debug build
# INTERNAL=1 DEBUG=1 is an internal debug build
#
# We map VMware OBJDIR as well:
# release => INTERNAL=0 DEBUG=0
# beta => INTERNAL=0 DEBUG=1
# opt => INTERNAL=0 DEBUG=0
# obj => INTERNAL=1 DEBUG=1
#
# VPS, VMSAFE, and VMAP are meta-configurations that control the other configuration
# options (except INTERNAL and DEBUG) and correspond to the VPS (Determina), VMSAFE, and
# VMAP products respectively.  They are meant to be mutually exclusive.
#
# OS variants:
# VMKERNEL - ESX or ESXi
# VMKCLASSIC - ESX classic
#   FIXME PR 363075: eventually should go away and be runtime-controlled
#   Currently it's only used for INIT_TAKE_OVER
# MACOS - Mac OS X
#
# individual configuration specifiers used by VPS/VMSAFE/VMAP :
# PROGRAM_SHEPHERDING - enable execution shepherding for security
# CLIENT_INTERFACE - are we exposing our client api
# APP_EXPORTS - are we exporting the start/stop interface
# STRACE_CLIENT - should we build the strace client
# HOT_PATCHING_INTERFACE - are we using hot patching functionality (liveshields/probes)
# PROCESS_CONTROL - enable process control/lockdown features
# GBOP - enable our generic buffer overflow prevention feature (windows only)
#
# See below for additional configuration options not tied to VPS/VMSAFE/VMAP.
#

DEBUG := 1
INTERNAL := 1

ifeq ($(OBJDIR),release)
DEBUG := 0
INTERNAL := 0
endif
ifeq ($(OBJDIR),beta)
DEBUG := 1
INTERNAL := 0
endif
ifeq ($(OBJDIR),opt)
DEBUG := 0
INTERNAL := 0
endif
ifeq ($(OBJDIR),obj)
DEBUG := 1
INTERNAL := 1
endif

# default to VMAP=1, but only if neither VPS or VMSAFE is set
VPS := 0
VMAP := 0
VMSAFE := 0
ifneq ($(VMSAFE), 1)
ifneq ($(VPS), 1)
VMAP := 1
endif
endif

# default is Linux
VMKERNEL := 0
VMKCLASSIC := 0
MACOS := 0

# we default to everything off
PROGRAM_SHEPHERDING := 0
CLIENT_INTERFACE := 0
APP_EXPORTS := 0
STRACE_CLIENT := 0
HOT_PATCHING_INTERFACE := 0
PROCESS_CONTROL := 0
ifeq ($(MACHINE), win32)
GBOP := 0
endif

# set the VMSAFE/VMAP/VPS configurations

ifeq ($(VMSAFE), 1)
# client and security support
PROGRAM_SHEPHERDING := 1
CLIENT_INTERFACE := 1
APP_EXPORTS := 1
HOT_PATCHING_INTERFACE := 1
endif

ifeq ($(VMAP), 1)
# client support, no security
CLIENT_INTERFACE := 1
APP_EXPORTS := 1
endif

ifeq ($(VPS), 1)
# security support, no client
PROGRAM_SHEPHERDING := 1
HOT_PATCHING_INTERFACE := 1
PROCESS_CONTROL := 1
ifeq ($(MACHINE), win32)
GBOP := 1
endif
endif

# can be used to enable collection of DR timing info in non-internal builds
KSTATS = 0

# use to build a non-debug yet non-optimized build, for caller profiling
CALLPROF = 0

ifeq ($(MACHINE), linux)
# PR 262458: now that we're using gcc >= 3.4 we
# can use -fvisibility instead of a linker version script.
# Note that we're still using a linker script to set our preferred
# base (PR 253624) and will need to for section ordering as well
# (PR 208267) but those are separate scripts.
LINKER_VERSION_SCRIPT = 0

KEEP_SYMBOLS_FOR_LIBC_BACKTRACE := 0
ifeq ($(INTERNAL), 1)
KEEP_SYMBOLS_FOR_LIBC_BACKTRACE := 1
endif

endif

# static or shared library [no longer supported, FIXME: maybe remove]
STATIC = 0

# build an executable for a unit test, rather than a library?
STANDALONE_UNIT_TEST = 0

ifeq ($(MACHINE), win32)
# for libc independence
NOLIBC = 1
endif

# use dependencies?
ifeq ($(MACHINE), linux)
USEDEP = 0 # PR 207890: not always working on linux either
else
# FIXME PR 207890: having problems with cygwin make, leave off for now
USEDEP = 0
endif

# convenience include file for per-tree customization:
-include Makefile.mydefines

## split defines from flags so can pass to assembler & makedepend:

# general purpose CFLAGS

###################################
# definitions for conditional compilation
#
# Linux variants
#    $(D)HAVE_PROC_MAPS - set if /proc/self/maps is available
#    $(D)HAVE_TLS       - set if any form of ldt or gdt entry can be claimed
#    $(D)HAVE_TLS_AREA_MULTI - set if a usually-unused set_thread_area slot is available
#    $(D)HAVE_SIGALTSTACK - set if SYS_sigaltstack is available
#    $(D)INIT_TAKE_OVER - libdynamorio.so init() takes over so no preload needed
# not supported but still in code because may be useful later
#    $(D)STEAL_REGISTER
#    $(D)DCONTEXT_IN_EDI
#    $(D)ASSUME_NORMAL_EFLAGS - (NEVER ON) (causes errors w/ Microsoft-compiled apps)
# internal studies, not for general use
#    $(D)SHARING_STUDY
#    $(D)FRAGMENT_SIZES_STUDY
#    $(D)FOOL_CPUID
#    $(D)NATIVE_RETURN - should clean this up, as well
#	NATIVE_RETURN_CALLDEPTH, NATIVE_RETURN_RET_IN_TRACES, 
#	NATIVE_RETURN_TRY_TO_PUT_APP_RETURN_PC_ON_STACK
#    $(D)LOAD_DYNAMO_DEBUGBREAK
# profiling
#    $(D)PROFILE_LINKCOUNT $(D)LINKCOUNT_64_BITS 
#    $(D)PROFILE_RDTSC 
#    $(D)PAPI - now deprecated
#    $(D)WINDOWS_PC_SAMPLE - on for all Windows builds
#    $(D)KSTATS - on for INTERNAL, DEBUG, and PROFILE builds, use KSTATS=1 for
# release builds
#    $(D)PROGRAM_SHEPHERDING -  (always ON)
#         currently turns on code origins checks and diagnostics, eventually will also turn
#         on return-after-call and other restricted control transfer features
#    $(D)RETURN_AFTER_CALL  - (always ON) return only to instructions after seen calls
#    $(D)RCT_IND_BRANCH     - (experimental) indirect branch only to address taken entry points
#    $(D)DGC_DIAGNOSTICS
#    $(D)CHECK_RETURNS_SSE2 (experimental security feature)
#    $(D)CHECK_RETURNS_SSE2_EMIT (experimental unfinished)
#    $(D)DIRECT_CALL_CHECK  (experimental unfinished)
#    $(D)SIMULATE_ATTACK    - simulate security violations
#    $(D)GBOP - generic buffer overflow prevention via hooking APIs
# optimization of application
#    $(D)SIDELINE
#    $(D)SIDELINE_COUNT_STUDY
#    $(D)LOAD_TO_CONST - around loadtoconst.c, $(D)LTC_STATS 

# optimization of dynamo
#    $(D)AVOID_EFLAGS  (uses instructions that don't modify flags) (defines ASSUME_NORMAL_EFLAGS)
#    $(D)RETURN_STACK
#    $(D)TRACE_HEAD_CACHE_INCR   (incompatible with security FIXME:?)
#    $(D)DISALLOW_CACHE_RESIZING (use as temporary hack when developing)
# transparency
#    $(D)NOLIBC = doesn't link libc on windows, currently uses ntdll.dll libc
#      functions, NOLIBC=0 causes the core to be linked against libc and kernel32.dll
# external interface
#    $(D)CLIENT_INTERFACE
#    $(D)DR_APP_EXPORTS
#    $(D)CUSTOM_EXIT_STUBS -- optional part of CLIENT_INTERFACE
#      we may want it for our own internal use too, though
#    $(D)CUSTOM_TRACES -- optional part of CLIENT_INTERFACE
#      has some sub-features that are aggressive and not supported by default:
#      $(D)CUSTOM_TRACES_RET_REMOVAL = support for removing inlined rets
#      $(D)CLIENT_SIDELINE = allows adaptive interface methods to be called 
#                            from other threads safetly, performance hit
#                            requires CLIENT_INTERFACE
#    $(D)UNSUPPORTED_API -- part of 0.9.4 MIT API but not supported in current API
#    $(D)NOT_DYNAMORIO_CORE - should be defined by non core components sharing our code

#    $(D)FANCY_COUNTDOWN - (NOT IMPLEMENTED) countdown messagebox

# debugging
#    $(D)DEBUG for debug builds
#    $(D)DEBUG_MEMORY (on for DEBUG)
#    $(D)STACK_GUARD_PAGE (on for DEBUG)
#    $(D)DEADLOCK_AVOIDANCE (on for DEBUG) - enforce total rank order on locks
#    $(D)MUTEX_CALLSTACK - enable collecting callstack info, requires DEADLOCK_AVOIDANCE
#    $(D)HEAP_ACCOUNTING (on for DEBUG)

#    $(D)INTERNAL for features that are not intended to reach customer hands
#    $(D)VERBOSE=1 for verbose debugging or in situations where normal DEBUG 

# statistics
#    $(D)HASHTABLE_STATISTICS - IBL table statistics

# target platforms
#    $(D)WINDOWS (avoid using _WIN32 used by cl)
#    $(D)LINUX 
#    note that in many cases we use the else of WINDOWS to mean LINUX and vice versa
#    we're just starting to add VMKERNEL and MACOS support
#    $(D)X86
#    $(D)X64

# support for running in x86 emulator on IA-64
#    $(D)IA32_ON_IA64

# build script provides these
#    $(D)BUILD_NUMBER (<64K == vmware's PRODUCT_BUILD_NUMBER)
#    $(D)UNIQUE_BUILD_NUMBER (== vmware's BUILD_NUMBER)
#    $(D)VERSION_NUMBER
#    $(D)VERSION_COMMA_DELIMITED

###################################

# UNIX variants
ifeq ($(MACHINE), linux)
  ifeq ($(VMKCLASSIC), 1)
    VMKERNEL := 1
  endif
  ifeq ($(VMKERNEL), 1)
    GEN_DEFINES += $(D)VMX86_SERVER
    GEN_DEFINES += $(D)USERLEVEL

    # FIXME PR 361894: ESX4.0+ has HAVE_TLS but not HAVE_TLS_AREA_MULTI:
    # so won't work w/ pthreads or other app using TLS slot:
    # not specifying for now since too limiting.
    # Remove HAVE_TLS_AREA_MULTI, then?
    # Plus, ESX3.5 does not have any TLS: so need runtime control
    # over whether use non-TLS get_thread_private_dcontext()
    # and force thread-private (and not support x64).
    ifeq ($(VMKCLASSIC), 1)
      # FIXME: setting this until get drpreload working on esx classic
      GEN_DEFINES += $(D)INIT_TAKE_OVER
    endif


    # We need these only for the OS dependent files,
    # yet there are no filename conflicts so always add to include paths
    VMKERNEL_TREE := $(VMTREE)# path to bora
    INCLUDE_PATHS += $(I)$(VMKERNEL_TREE)/public \
		     $(I)$(VMKERNEL_TREE)/lib/public \
		     $(I)$(VMKERNEL_TREE)/vmkernel/public \
		     $(I)$(VMKERNEL_TREE)/vmkernel/include/vmkapi \
		     $(I)$(VMKERNEL_TREE)/vmkernel/include/user \
		     $(I)$(VMKERNEL_TREE)/vmkernel/include/public \
		     $(I)$(VMKERNEL_TREE)/lib/vmkuser/include/public \
		     $(I)$(VMKERNEL_TREE)/vmkernel/distribute \
		     $(I)$(VMKERNEL_TREE)/vmcore/public

  else
    ifeq ($(MACOS), 1)
      # FIXME NYI
      GEN_DEFINES += $(D)MACOS
    else
      # Linux
      GEN_DEFINES += $(D)HAVE_PROC_MAPS
      GEN_DEFINES += $(D)HAVE_TLS
      GEN_DEFINES += $(D)HAVE_TLS_AREA_MULTI
      GEN_DEFINES += $(D)HAVE_SIGALTSTACK
    endif
  endif
endif

# defines in all builds
ifeq ($(MACHINE), win32)
GEN_DEFINES += $(D)WINDOWS_PC_SAMPLE
endif

# defines that depend on configuration variables
# N.B.: order is important here as some set values of later vars

ifeq ($(PROGRAM_SHEPHERDING), 1)
GEN_DEFINES += $(D)PROGRAM_SHEPHERDING
GEN_DEFINES += $(D)RETURN_AFTER_CALL
GEN_DEFINES += $(D)RCT_IND_BRANCH
endif

ifeq ($(CLIENT_INTERFACE), 1)
# standard client interface features
GEN_DEFINES += $(D)CLIENT_INTERFACE
GEN_DEFINES += $(D)DYNAMORIO_IR_EXPORTS
GEN_DEFINES += $(D)CUSTOM_TRACES
# PR 200409: not part of our current API, xref PR 215179 on -pad_jmps issues with
# CUSTOM_EXIT_STUBS
#GEN_DEFINES += $(D)CUSTOM_EXIT_STUBS
#GEN_DEFINES += $(D)UNSUPPORTED_API
endif

ifeq ($(APP_EXPORTS), 1)
GEN_DEFINES += $(D)DR_APP_EXPORTS
endif

ifeq ($(HOT_PATCHING_INTERFACE), 1)
GEN_DEFINES += $(D)HOT_PATCHING_INTERFACE
endif

ifeq ($(PROCESS_CONTROL), 1)
GEN_DEFINES += $(D)PROCESS_CONTROL
endif

ifeq ($(GBOP), 1)
# FIXME: some of the hooks depend on hotp_only HOT_PATCHING_INTERFACE
GEN_DEFINES += $(D)GBOP
endif

# we also define INTERNAL for builds for features that are not intended to reach customer hands
ifeq ($(DEBUG), 0)
# a command line override is still possible, but the default release build is NOT internal
INTERNAL = 0
else
# for bug fixing this is useful so we turn on for all debug builds
GEN_DEFINES += $(D)DEBUG_MEMORY
GEN_DEFINES += $(D)STACK_GUARD_PAGE
GEN_DEFINES += $(D)HEAP_ACCOUNTING
GEN_DEFINES += $(D)DEADLOCK_AVOIDANCE
GEN_DEFINES += $(D)MUTEX_CALLSTACK # requires DEADLOCK_AVOIDANCE
# even though only usable in all-private config useful in default builds
GEN_DEFINES += $(D)SHARING_STUDY
endif

ifeq ($(INTERNAL), 1)
GEN_DEFINES += $(D)INTERNAL
KSTATS = 1
endif

ifeq ($(EXTERNAL_RELEASE), 1)
$(error EXTERNAL_RELEASE no longer supported. Update your script.)
endif

ifeq ($(EXTERNAL_INJECTOR), 1)
GEN_DEFINES += $(D)EXTERNAL_INJECTOR
endif

ifeq ($(STANDALONE_UNIT_TEST), 1)
GEN_DEFINES += $(D)STANDALONE_UNIT_TEST
endif

NOLIBC_DLL_ENTRY = /entry:DllMain
ifeq ($(MACHINE), win32)
ifeq ($(NOLIBC), 1)
GEN_DEFINES += $(D)NOLIBC
CORE_DLL_ENTRY = $(NOLIBC_DLL_ENTRY)
else
CORE_DLL_ENTRY =
endif
endif

# Processing for custom builds
include $(DYNAMORIO_BASE)/Makefile.custom_build

ifneq ($(CUSTOM_PRODUCT_NAME),)
  GEN_DEFINES += $(D)CUSTOM_PRODUCT_NAME=$(CUSTOM_PRODUCT_NAME)
endif

ifneq ($(BUILD_NUMBER),)
  GEN_DEFINES += $(D)BUILD_NUMBER=$(BUILD_NUMBER)
endif

ifneq ($(UNIQUE_BUILD_NUMBER),)
  GEN_DEFINES += $(D)UNIQUE_BUILD_NUMBER=$(UNIQUE_BUILD_NUMBER)
else
  GEN_DEFINES += $(D)UNIQUE_BUILD_NUMBER=$(BUILD_NUMBER)
endif

ifneq ($(VERSION_NUMBER),)
  GEN_DEFINES += $(D)VERSION_NUMBER=$(VERSION_NUMBER)
endif

ifneq ($(VERSION_COMMA_DELIMITED),)
  GEN_DEFINES += $(D)VERSION_COMMA_DELIMITED=$(VERSION_COMMA_DELIMITED)
endif

STATS=$(DEBUG)
ifeq ($(STATS),1)
GEN_DEFINES += $(D)HASHTABLE_STATISTICS
KSTATS = 1
endif

# no KSTATS for caller profiling: we want to be as close to release
# build as we can, but w/o optimizations
ifeq ($(CALLPROF), 1)
KSTATS=0
GEN_DEFINES += $(D)CALL_PROFILE
endif

ifeq ($(KSTATS), 1)
GEN_DEFINES += $(D)KSTATS
endif

# choose BUILD_SUFFIX 
ifeq ($(DEBUG),1)
BUILD_SUFFIX=dbg
INSTALL_LIB = $(INSTALL_LIB_BASE)/debug
else
ifeq ($(PROFILE),1)
GEN_DEFINES += $(D)PROFILE
BUILD_SUFFIX=prof
INSTALL_LIB = $(INSTALL_LIB_BASE)/profile
else
ifeq ($(INTERNAL), 1)
BUILD_SUFFIX=intr
INSTALL_LIB = $(INSTALL_LIB_BASE)/internal
else
BUILD_SUFFIX=rel
INSTALL_LIB = $(INSTALL_LIB_BASE)/release
endif # INTERNAL
endif # PROFILE
endif # DEBUG

BUILD = $(BUILD_BASE)/$(ARCH)_$(OSNAME)_$(BUILD_SUFFIX)

# target specific CFLAGS
TGT_DEFINES = $(ARCH_DEFS) $(OS_DEFS)
ifeq ($(STATIC), 1)
TGT_DEFINES += $(D)STATIC_LIBRARY
TGT_CFLAGS =
else
ifeq ($(MACHINE), linux)

# FIXME PR 196857: pick gcc, glibc, and binutils targets in
# the vmware toolchain.
ifeq ($(ARCH), x64)
TGT_CFLAGS += -m64
ASMFLAGS += --64
else
TGT_CFLAGS += -m32
ASMFLAGS += --32
endif

# don't use -fpic, it's slow?  we want statically linked shared lib?
TGT_CFLAGS += -fpic

ifeq ($(LINKER_VERSION_SCRIPT), 0)
# only export functions so marked via attributes
# only available in gcc 3.4!
# so we use an ugly linker script via $(EXPORT_LIST) instead by default
#
# using "internal" instead of "hidden" b/c we don't need any indirect
# calls to our non-exported functions
ifeq ($(KEEP_SYMBOLS_FOR_LIBC_BACKTRACE), 0)
TGT_CFLAGS += -fvisibility=internal
endif
GEN_DEFINES += $(D)USE_VISIBILITY_ATTRIBUTES
endif

# disable strict aliasing opt in gcc 3.3.3 -- gives warnings and makes bad assumptions
TGT_CFLAGS += -fno-strict-aliasing
else
TGT_CFLAGS =
# read-only string pooling
TGT_CFLAGS +=/GF
endif
endif

GEN_CFLAGS = $(WARN)
DEFINES = $(GEN_DEFINES) $(TGT_DEFINES) $(ADD_DEFINES) $(REMOVE_DEFINES)
API_DEFINES = $(DEFINES) $(D)API_EXPORT_ONLY
CFLAGS += $(GEN_CFLAGS) $(TGT_CFLAGS)

ifeq ($(MACHINE), win32)
# FIXME case 191729: we should try to enable this.
# Currently we get "unresolved external symbol ___security_cookie"
CFLAGS += /GS-
FILTER_CC =
ifneq ($(ARCH), x64)
# PR 219380: to avoid __ftol2_sse from libc
$(BUILD)/io.obj: CFLAGS += /QIfist
# There is no supported way to suppress a "command line" warning
$(BUILD)/io.obj: FILTER_CC = 2>&1 | $(GREP) -v "D9035 : option 'QIfist'";
endif
endif

ifeq ($(DEBUG),1)
# debug specific flags and defines
DEFINES += $(D)DEBUG
CFLAGS += $(DBG) $(DBG_OPT)
else
# release specific flags and defines
DEFINES += $(D)RELEASE 
ifeq ($(CALLPROF), 1)
# no opts -- we need to avoid messing up call frame walking
# FIXME: just disable frame ptr elim opt: but /Oy- ran into some issues
else
CFLAGS += $(OPT)
endif
ifeq ($(MACHINE), win32)
# using DBG here won't mess up the optimizations, debug line number info
# gets a little messed up, but is better than nothing
CFLAGS += $(DBG)
endif
endif

# asm flags
ifeq ($(IS_X86), 1)
  ifeq ($(MACHINE), linux)
    DEFINES += $(D)ASSEMBLE_WITH_GAS
    ASM_DEFINES := $(patsubst $(D)%, --defsym %=1, $(DEFINES))
    ASM_DEFINES := $(patsubst $(U)%, --defsym %=0, $(ASM_DEFINES))
    ASM_NDCP_DEFINES := --defsym NOT_DYNAMORIO_CORE_PROPER=1
    # We do NOT use --gstabs as it creates two sections, .stab and .stabstr, AFTER our
    # .lspdata and .nspdata sections.  This messes up our data segment protection scheme
    # as .data is BEFORE our protected sections and .stab* are writable (older 
    # versions of as had them read-only).  We have found no way to control the order
    # with the linker, and we don't want to bother to recognize these particular sections
    # and omit them from protection, since we don't really need them.
  else
    DEFINES += $(D)ASSEMBLE_WITH_MASM
    ASM_DEFINES := $(DEFINES)
    ASM_NDCP_DEFINES := $(D)NOT_DYNAMORIO_CORE_PROPER
    ASMFLAGS += $(INCR)
    ifeq ($(DEBUG),1)
      ASMFLAGS += $(DBG)
    endif
    # ml can't handle the line number markers
    ASMPREPROC += $(CPP_NO_LINENUM)
  endif
  ASMFLAGS += $(ASM_DEFINES)
endif

# target-specific linker flags (only used if creating .o instead of .a)
ifeq ($(MACHINE), linux)
API_UNDEFS = $(U)dynamorio_app_init $(U)dynamorio_app_exit \
	     $(U)dynamo_thread_init $(U)dynamo_thread_exit \
	     $(U)dr_app_start $(U)dr_app_stop \
	     $(U)dynamorio_app_take_over
LDFLAGS = $(API_UNDEFS) -s -x -r
# need libgcc.a for __cmpdi2 (long long conversion functions I think)
## actually I don't need to explicitly name it if link through gcc
## GCC_LIB_PATH := $(shell locate libgcc.a)
CC_LDLIBS	:= -lc -ldl -lm
# FIXME case 1891/8972: should statically link w/ everything
# but then we are initialized prior to 
# libc init and we need to get our env vars directly (case 1891/69)
#CC_LDLIBS	= $(L)/usr/lib -static-libgcc /usr/lib/libc.a /usr/lib/libdl.a /usr/lib/libm.a lib/papi/libpapi.a
LD_LDLIBS = $(CC_LDLIBS)
else # win32
OUTNAME = dynamorio
INJECTNAME = drinject
PREINJECTNAME = drpreinject
EARLY_INJECT_HELPER1_NAME = drearlyhelp1
EARLY_INJECT_HELPER2_NAME = drearlyhelp2
# PR 219380: we export ntdll routines from dynamorio.dll by forwarding them.
# However, the linker tries to be smart and thinks our _snprintf forward
# is the same as our own snprintf: it removes the latter and issues a warning.
# We re-add our own snprintf by listing it in the .def file, and we suppress
# the warning using the secret option "/ignore".
ifeq ($(ARCH), x64)
DEF_SRC := lib/dr_ntdll_x64.def
# Xref PR 215395, we currently require DynamoRIO to be loaded in a certain address
# range.  To be double sure we compile it FIXED for now.
NO_PIC64 := /FIXED
else
DEF_SRC := lib/dr_ntdll.def
NO_PIC64 :=
endif
DEF_BLD := $(BUILD)/$(DEF_SRC)
FORWARD_TO_NTDLL := /def:$(DEF_BLD) /ignore:4197
ifeq ($(DEBUG), 1)
# FIXME: we should verify these bases have no conflicts in supported apps
TGTFLAGS = ${LINK_DBG} /release /opt:ref
LDFLAGS_B = /base:"0x15000000" $(NO_PIC64)
else
TGTFLAGS = ${LINK_DBG} /release /opt:ref /opt:icf 
LDFLAGS_B = /base:"0x71000000" $(NO_PIC64)
endif
MSBUILD := $(subst /,\\,$(BUILD))
ifeq ($(ARCH), x64)
LD_MACHINE_FLAGS = /machine:X64
NTDLL_LIB := ntdll.lib
NTDLL_LIBPATH := $(NTDLL_LIBPATH_X64)
else
LDFLAGS = /machine:I386
NTDLL_LIB := ntdll.lib
NTDLL_LIBPATH := $(NTDLL_LIBPATH_X86)
endif
# make sure to export dynamo_auto_start (it's now in x86.s and so
# we can't just say __declspec(dllexport))
LDFLAGS = $(TGTFLAGS) $(LDFLAGS_B) /dll /incremental:no \
          $(CORE_DLL_ENTRY) $(LD_MACHINE_FLAGS) \
          /pdb:"$(MSBUILD)\\$(OUTNAME).pdb" \
          /out:"$(MSBUILD)\\$(OUTNAME)$(SO_SFX)" \
          /implib:"$(MSBUILD)\\$(OUTNAME).lib" \
          /map:"$(MSBUILD)\\$(OUTNAME).map" \
          /mapinfo:exports $(FORWARD_TO_NTDLL) \
          /export:dynamo_auto_start \
          /export:dynamorio_app_take_over
ifeq ($(APP_EXPORTS), 1)
LDFLAGS += /export:dr_app_start \
	   /export:dr_app_take_over
endif
# I'm adding libcmt to attempt to make sure we have multithreaded C lib:
ifeq ($(DEBUG), 1)
WIN32_C_LIB = libcmtd.lib
else
WIN32_C_LIB = libcmt.lib
endif
ifeq ($(NOLIBC), 1)
CORE_WIN32_C_LIB =
else
CORE_WIN32_C_LIB = $(WIN32_C_LIB) kernel32.lib
endif
# case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
# note that _alldvrm is in ntdll.dll on xp+; but for pre-xp uses we
# stick with the static linking of the impl in msvcrt.lib.
CC_LDLIBS = $(MSLIB) $(L)$(subst /,\\,$(NTDLL_LIBPATH)) \
            /NODEFAULTLIB $(CORE_WIN32_C_LIB) $(NTDLL_LIB) msvcrt.lib
LD_LDLIBS := $(subst /link,,$(CC_LDLIBS))
INJECTFLAGS = $(TGTFLAGS) /incremental:no $(LD_MACHINE_FLAGS) /subsystem:console \
          /pdb:"$(MSBUILD)\\$(INJECTNAME).pdb" \
          /out:"$(MSBUILD)\\$(INJECTNAME).exe" \
          /map:"$(MSBUILD)\\$(INJECTNAME).map" \
          /mapinfo:exports
INJECTLIBS_1 = $(MSLIB) $(L)$(subst /,\\,$(NTDLL_LIBPATH)) /NODEFAULTLIB \
               $(WIN32_C_LIB) $(NTDLL_LIB) kernel32.lib Advapi32.lib imagehlp.lib
INJECTLIBS = $(subst /link,,$(INJECTLIBS_1))
PRE_INJECT_B = /base:"0x14000000"
# preinjector requires no C library for its unload-self method
PRE_INJECTFLAGS = $(TGTFLAGS) $(PRE_INJECT_B) /dll /incremental:no $(LD_MACHINE_FLAGS) \
          $(NOLIBC_DLL_ENTRY) \
          /pdb:"$(MSBUILD)\\$(PREINJECTNAME).pdb" \
          /out:"$(MSBUILD)\\$(PREINJECTNAME).dll" \
          /map:"$(MSBUILD)\\$(PREINJECTNAME).map" \
          /mapinfo:exports
# case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
PRE_INJECTLIBS_1 = $(MSLIB) $(L)$(subst /,\\,$(NTDLL_LIBPATH)) \
                   /NODEFAULTLIB $(NTDLL_LIB) kernel32.lib msvcrt.lib \
                   $(L)$(INSTALL_LIB) $(OUTNAME).lib
PRE_INJECTLIBS := $(subst /link,,$(PRE_INJECTLIBS_1))
# release base - 64k 
EARLY_INJECT_HELPER1_B = /base:"0x70FF0000"
EARLY_INJECT_HELPER1_FLAGS = $(TGTFLAGS) $(EARLY_INJECT_HELPER1_B) /dll \
          /incremental:no $(LD_MACHINE_FLAGS) $(NOLIBC_DLL_ENTRY) \
          /pdb:"$(MSBUILD)\\$(EARLY_INJECT_HELPER1_NAME).pdb" \
          /out:"$(MSBUILD)\\$(EARLY_INJECT_HELPER1_NAME)$(SO_SFX)" \
          /map:"$(MSBUILD)\\$(EARLY_INJECT_HELPER1_NAME).map" \
          /mapinfo:exports
EARLY_INJECT_HELPER1_LIBS = /NODEFAULTLIB "$(MSBUILD)\\$(EARLY_INJECT_HELPER2_NAME).lib"
# helper1 base - 64k 
EARLY_INJECT_HELPER2_B = /base:"0x71110000"
EARLY_INJECT_HELPER2_FLAGS = $(TGTFLAGS) $(EARLY_INJECT_HELPER2_B) /dll \
          /incremental:no $(LD_MACHINE_FLAGS) $(NOLIBC_DLL_ENTRY) \
          /pdb:"$(MSBUILD)\\$(EARLY_INJECT_HELPER2_NAME).pdb" \
          /out:"$(MSBUILD)\\$(EARLY_INJECT_HELPER2_NAME)$(SO_SFX)" \
          /implib:"$(MSBUILD)\\$(EARLY_INJECT_HELPER2_NAME).lib" \
          /map:"$(MSBUILD)\\$(EARLY_INJECT_HELPER2_NAME).map" \
          /mapinfo:exports 
EARLY_INJECT_HELPER2_LIBS = /NODEFAULTLIB
endif

###########################################################################
##
## Files

FORTRAN_SRC = $(LIB_SRC)/fortran.c
FORTRAN_OBJ = $(LIBDIR)/fortran.$(OBJ_SFX)

STANDALONE_NAME = drtest-${UNIT_TEST}
ifeq ($(MACHINE), linux)
ifeq ($(STATIC), 1)
ARCHIVE	= libdynamorio$(SO_SFX)
else
ARCHIVE	= libdynamorio$(SO_SFX)
PRELOAD = libdrpreload$(SO_SFX)
ifeq ($(LINKER_VERSION_SCRIPT), 1)
EXPORT_LIST = $(BUILD)/exportlist
LINKER_EXPORT_FLAGS = -Xlinker --version-script -Xlinker $(EXPORT_LIST)
endif
LD_SCRIPT = $(BUILD)/ldscript
endif

ifeq ($(KEEP_SYMBOLS_FOR_LIBC_BACKTRACE), 1)
LINKER_EXPORT_FLAGS += -Xlinker --export-dynamic
endif

else # win32
ARCHIVE	= $(OUTNAME)$(SO_SFX)
INJECTOR = $(INJECTNAME)$(EXE_SFX)
PRELOAD = $(PREINJECTNAME)$(SO_SFX)
EARLY_INJECT_HELPER1 = $(EARLY_INJECT_HELPER1_NAME)$(SO_SFX)
EARLY_INJECT_HELPER2 = $(EARLY_INJECT_HELPER2_NAME)$(SO_SFX)
endif
STANDALONE_EXE = ${STANDALONE_NAME}${EXE_SFX}

# options.c first so that cl will put lspdata section before nspdata section!
CORE_SRCFILES = options.c dynamo.c fragment.c fcache.c link.c dispatch.c emit.c utils.c \
                stats.c heap.c instrlist.c monitor.c perfctr.c vmareas.c rct.c \
                hotpatch.c hashtable.c module_list.c moduledb.c perscache.c nudge.c \
                synch.c
ifeq ($(OSNAME), linux)
CORE_SRCFILES += io.c
else
# PR 219380: ntdll._vnsprintf doesn't support printing floats so we export ours
ifeq ($(CLIENT_INTERFACE), 1)
CORE_SRCFILES += io.c
endif
endif
CORE_SRCS := $(patsubst %,$(SRC)/%, $(CORE_SRCFILES))
CORE_OBJS := $(patsubst %.c,$(BUILD)/%.$(OBJ_SFX), $(CORE_SRCFILES)) 

########################################
## Arch files

# both win32 and linux use x86.s now
ifeq ($(IS_X86), 1)
ARCH_SRCFILES := \
  arch.c decode.c decode_table.c encode.c emit_utils.c instr.c interp.c \
  proc.c mangle.c steal_reg.c x86_code.c disassemble.c x86.s \
  decode_fast.c optimize.c sideline.c instrument.c retcheck.c
endif # x86

ARCH_SRCS  := $(patsubst %,$(ARCH_SRC)/%, $(ARCH_SRCFILES))
ARCH_OBJS1 := $(patsubst %.c,$(BUILD)/$(ARCH_DIRNAME)/%.$(OBJ_SFX), $(ARCH_SRCFILES))
ARCH_OBJS  := $(patsubst %.s,$(BUILD)/$(ARCH_DIRNAME)/%.$(OBJ_SFX), $(ARCH_OBJS1))

########################################
## OS files

ifeq ($(OSNAME), linux)
OS_SRCFILES = os.c signal.c module.c pcprofile.c stackdump.c diagnost.c
  ifeq ($(VMKERNEL), 1)
    OS_SRCFILES += vmkuw.c
  endif
else # win32
OS_SRCFILES = eventlog.c os.c syscall.c callback.c drmarker.c ntdll.c inject.c \
              inject_shared.c module.c module_shared.c diagnost.c aslr.c
endif
OS_SRCS := $(patsubst %,$(OS_SRC)/%, $(OS_SRCFILES))
OS_OBJS := $(patsubst %.c,$(BUILD)/$(OSNAME)/%.$(OBJ_SFX), $(OS_SRCFILES))

ifeq ($(OSNAME), linux)

########################################
## Preload Files

PRELOAD_SRCFILES = preload.c
PRELOAD_SRCS := $(patsubst %,$(OS_SRC)/%, $(PRELOAD_SRCFILES))
PRELOAD_OBJS := $(patsubst %.c,$(BUILD)/$(OSNAME)/%.$(OBJ_SFX), $(PRELOAD_SRCFILES)) 

endif #linux

ifeq ($(MACHINE), win32)

########################################
## Injector Files

# N.B.: *-noncore.c does not exist, this is a hack!
INJECT_SRCFILES = injector.c inject-noncore.c ntdll-noncore.c inject_shared-noncore.c \
                  x86-noncore.s
INJECT_SRC_OBJS := $(patsubst %.s,$(BUILD)/$(ARCH_DIRNAME)/%.$(OBJ_SFX),\
                     $(patsubst %.c,$(BUILD)/$(OSNAME)/%.$(OBJ_SFX), $(INJECT_SRCFILES)))
INJECT_RESOURCE_OBJS = $(BUILD)/injector.res
INJECT_OBJS := $(INJECT_SRC_OBJS) $(INJECT_RESOURCE_OBJS)

########################################
## Pre-Injector files

# N.B.: *-noncore.c does not exist, this is a hack!
PRE_INJECT_SRCFILES = pre_inject.c ntdll-noncore.c inject_shared-noncore.c \
                      drmarker-noncore.c x86-noncore.s pre_inject_asm.s \
                      module_shared-noncore.c
PRE_INJECT_SRC_OBJS := $(patsubst %.s,$(BUILD)/$(ARCH_DIRNAME)/%.$(OBJ_SFX),\
                        $(patsubst %.c,$(BUILD)/$(OSNAME)/%.$(OBJ_SFX),\
                         $(PRE_INJECT_SRCFILES)))
PRE_INJECT_RESOURCE_OBJS = $(BUILD)/preinjector.res
PRE_INJECT_OBJS := $(PRE_INJECT_SRC_OBJS) $(PRE_INJECT_RESOURCE_OBJS)

#######################################
## Early inject helper dlls files

EARLY_INJECT_HELPER1_SRCFILES = early_inject_helper1.c
EARLY_INJECT_HELPER1_SRCS = $(patsubst %,$(OS_SRC)/%, $(EARLY_INJECT_HELPER1_SRCFILES))
EARLY_INJECT_HELPER1_SRC_OBJS = $(patsubst %.c,$(BUILD)/$(OSNAME)/%.$(OBJ_SFX), $(EARLY_INJECT_HELPER1_SRCFILES))
EARLY_INJECT_HELPER1_RESOURCE_OBJS = $(BUILD)/drearlyhelp1.res
EARLY_INJECT_HELPER1_OBJS := $(EARLY_INJECT_HELPER1_SRC_OBJS) $(EARLY_INJECT_HELPER1_RESOURCE_OBJS)

EARLY_INJECT_HELPER2_SRCFILES = early_inject_helper2.c
EARLY_INJECT_HELPER2_SRCS = $(patsubst %,$(OS_SRC)/%, $(EARLY_INJECT_HELPER2_SRCFILES))
EARLY_INJECT_HELPER2_SRC_OBJS = $(patsubst %.c,$(BUILD)/$(OSNAME)/%.$(OBJ_SFX), $(EARLY_INJECT_HELPER2_SRCFILES))
EARLY_INJECT_HELPER2_RESOURCE_OBJS = $(BUILD)/drearlyhelp1.res
EARLY_INJECT_HELPER2_OBJS := $(EARLY_INJECT_HELPER2_SRC_OBJS) $(INJECT_RESOURCE_OBJS)

endif #windows

########################################
## Buildmark files
## FIXME: $(BUILD)/
DEFINES_H = $(BUILD)/defines.h
POST_DEFINES_H = $(BUILD)/post-defines.h
BUILDMARK_H =  $(DEFINES_H) $(POST_DEFINES_H)
BUILDMARK_SRCFILES = buildmark.c
BUILDMARK_SRCS := $(patsubst %,$(SRC)/%, $(BUILDMARK_SRCFILES))
BUILDMARK_OBJS := $(patsubst %.c,$(BUILD)/%.$(OBJ_SFX), $(BUILDMARK_SRCFILES)) 

########################################
## Resource files
# Event log messages
# .mc file processed and shared for fmt strings, see below
ifeq ($(MACHINE), win32)
MESSAGE_FILES=$(OSNAME)/events.mc
MESSAGE_H= $(patsubst %.mc,$(BUILD)/%.h, $(MESSAGE_FILES)) 
# note: the above goes to $(BUILD)/OS
MESSAGE_RC= $(patsubst %.mc,$(BUILD)/%.rc, $(MESSAGE_FILES)) 

# only one resource file allowed so we include by reference generated message resources
RESOURCE_OBJS=$(BUILD)/dynamorio.res
GENERATED_INCLUDES= $(I)$(subst /,\\,$(BUILD)) $(I)$(subst /,\\,$(BUILD)/$(OSNAME))
else # no resources on linux
RESOURCE_OBJS=
GENERATED_INCLUDES=$(I)$(BUILD) # for DEFINES_H
endif

########################################
## Syslog fmt string header files (shared)
MESSAGE_STRINGS_FILES=win32/events.mc
MESSAGE_STRINGS_H=$(BUILD)/event_strings.h

########################################
## Everything going into core library

SRCS   := $(CORE_SRCS) $(ARCH_SRCS) $(OS_SRCS) $(BUILDMARK_SRCS)
OTHER_SRCS := $(FORTRAN_SRC)
C_SRCS := $(patsubst %.s,, $(SRCS))
CPPS   := $(patsubst ./%.c,$(BUILD)/%.$(CPP_SFX), $(C_SRCS))
DEPS   := $(CPPS:.$(CPP_SFX)=.$(DEP_SFX))
OBJS   := $(CORE_OBJS) $(ARCH_OBJS) $(OS_OBJS) $(BUILDMARK_OBJS) $(RESOURCE_OBJS)
OTHER_OBJS := $(FORTRAN_OBJ)

ifeq ($(MACHINE), win32)
PDBS   := $(subst .$(OBJ_SFX),.pdb, $(OBJS))
endif

########################################
## All header files
HEADERS = $(wildcard *.h */*.h)

# script to generate API header files
GENAPI = lib/genapi.pl

###########################################################################
##
## Rules

.PHONY:: all builddir help tags clear clean exports unexport zap html ugly pretty_ugly \
	really_ugly compare-defines clean-mark buildmark buildnum injector preload \
	early_inject_helper install dynamorio usedep_defines depend clients symbols \
	runregression specupdate api_headers

###########################################################################
## Unit tests have all objs except for the current unit tested file

ifeq ($(STANDALONE_UNIT_TEST), 1)
UNIT_TEST = unitname
UNIT_OBJS := $(subst $(UNIT_TEST),unit-$(UNIT_TEST), $(OBJS))

# FIXME: should rename all other objects to -standalone.obj to make
# sure compiled with $(D)STANDALONE_UNIT_TEST without complaining when
# compiling the library itself, that way we will be able to run the
# unit tests simultaneously as regression tests too

# even though we don't use the corresponding object we'll depend on it here to capture its dependencies
$(BUILD)/unit-$(UNIT_TEST).$(OBJ_SFX) : $(BUILD)/$(UNIT_TEST).c

ifneq ($(findstring unit-$(UNIT_TEST),$(OBJS)),)
# we have older unit tests embedded in the src file itself, which
# we must copy to create the unit- version
$(BUILD)/unit-$(UNIT_TEST).$(OBJ_SFX) : $(BUILD)/$(UNIT_TEST).$(OBJ_SFX)
	$(CP) $< $@
endif

# the default target convention has to be all for the defines check to work properly
all: builddir standalone

LOGLEVEL = 0
UNIT_OPTIONS = -stderr_mask 0x15 ${UNIT_ADD_OPTIONS}
UNIT_OPTIONS += -detect_mode  # for security unit tests
ifneq (${LOGLEVEL},)
  UNIT_OPTIONS += -loglevel ${LOGLEVEL}
endif

ifeq ($(MACHINE), win32)
# we need to set any environment variables into the registry with drinject
UNIT_CMDLINE=${INSTALL_BIN}/${INJECTOR} ${INJECTOR_CMDLINE} -noinject
UNIT_CMDLINE_DEPENDS=${INSTALL_BIN}/${INJECTOR}
# FIXME: in fact it will be a good dual purpose test to run all tests under DR
# after the proper library is used - see comment about -standalone.obj
else
UNIT_CMDLINE=
endif # !win32

unitrun: all standalone ${UNIT_CMDLINE_DEPENDS}
	export DYNAMORIO_OPTIONS="${UNIT_OPTIONS}"; ${UNIT_CMDLINE} ${BUILD}/${STANDALONE_EXE}

else # !STANDALONE

ifeq ($(STATIC),1)
all: builddir dynamorio $(INSTALL_LIB)/$(FORTRAN_OBJ)
else
ifeq ($(MACHINE), win32)
all: builddir dynamorio injector early_inject_helper clients
else #linux
all: builddir dynamorio $(INSTALL_LIB)/$(FORTRAN_OBJ) \
     $(INSTALL_LIB_BASE)/$(PRELOAD) clients
endif
endif # !STATIC
endif # !STANDALONE


builddir:
	@(if ! test -e $(BUILD_BASE); then $(MKDIR) $(BUILD_BASE); fi)
	@(if ! test -e $(BUILD); then $(MKDIR) $(BUILD); fi)
	@(if ! test -e $(BUILD)/$(ARCH_DIRNAME); then $(MKDIR) $(BUILD)/$(ARCH_DIRNAME); fi)
	@(if ! test -e $(BUILD)/$(OSNAME); then $(MKDIR) $(BUILD)/$(OSNAME); fi)
	@(if ! test -e $(BUILD)/$(LIBDIR); then $(MKDIR) $(BUILD)/$(LIBDIR); fi)
	@(if ! test -e $(EXPORTS_BASE); then $(MKDIR) $(EXPORTS_BASE); fi)
	@(if ! test -e $(INSTALL_LIB_BASE); then $(MKDIR) $(INSTALL_LIB_BASE); fi)
	@(if ! test -e $(INSTALL_LIB); then $(MKDIR) $(INSTALL_LIB); fi)
	@(if ! test -e $(INSTALL_BIN_BASE); then $(MKDIR) $(INSTALL_BIN_BASE); fi)
ifeq ($(MACHINE), win32)
	@(if ! test -e $(INSTALL_BIN); then $(MKDIR) $(INSTALL_BIN); fi)
endif
	@(if ! test -e $(INSTALL_INCLUDE); then $(MKDIR) $(INSTALL_INCLUDE); fi)

# creates the TAGS directory under core/ for Emacs use
ifeq ($(MACHINE), win32)
ALL_SRCS := $(SRC)/*.[hc] $(LIB_SRC)/*.[hc] $(OS_SRC)/*.{h,c,mc,rc} $(ARCH_SRC)/*.{h,c,asm}
else
ALL_SRCS := $(SRC)/*.[hc] $(LIB_SRC)/*.[hc] $(OS_SRC)/*.{h,c} $(ARCH_SRC)/*.{h,c,asm}
endif
tags TAGS:
	$(ETAGS) -o $(SRC)/TAGS $(ALL_SRCS)
# -cscope in case it's not installed
# cscope seems to be very flaky, and many queries fail, when the index is built
# using files passed on command line: so we use cscope.files.
	$(LS) -1 $(ALL_SRCS) > cscope.files
	-cscope -b

# blows away build and exports dirs
clear:
	$(RM) -r $(BUILD_BASE)
	$(RM) -r $(EXPORTS_BASE)

# removes .a,.i and .o files and generated header files
clean:
	$(RM) $(OBJS) $(UNIT_OBJS) $(OTHER_OBJS) $(CPPS) $(DEPS) $(BUILD)/*$(SO_SFX) $(BUILD)/*.{$(OBJ_SFX),h} $(BUILD)/*/*.{$(OBJ_SFX),h} $(PRELOAD_OBJS)
	$(RM) $(BUILDMARK_H)
	$(RM) -r $(INSTALL_LIB) $(INSTALL_BIN) $(INSTALL_INCLUDE)
ifeq ($(MACHINE), win32)
	$(RM) $(PDBS) $(BUILD)/*.$(CPP_SFX).bak $(BUILD)/*/*.$(CPP_SFX).bak $(BUILD)/*.{cod,exp,lib,map,pdb,res,rc,bin,exe} $(BUILD)/*/*.{cod,exp,lib,map,pdb,res,rc,bin,exe}
else
ifeq ($(LINKER_VERSION_SCRIPT), 1)
	$(RM) $(EXPORT_LIST)
endif
endif

exports: unexport all
unexport:
	$(RM) -r $(INSTALL_LIB) $(INSTALL_BIN) $(INSTALL_INCLUDE)
	$(RM) $(BUILD)/*.{so,a,dll,lib}

# removes emacs *~ files
zap:
	$(RM) $(SRC)/*~ $(OS_SRC)/*~ $(ARCH_SRC)/*~ $(LIB_SRC)/*~

%.iso: 
	(cd $(DYNAMORIO_BASE); mkisofs -o $@ -R -J $*)
	@$(ECHO) "The source files should not be made accessible to unauthorized parties! ***"

# creates html navigation files for the entire Dynamo source tree
html:
	@(if ! test -e $(HTML); then $(MKDIR) $(HTML); fi)
	(cd $(HTML); $(DYNAMORIO_TOOLS)/ctoohtml $(HTML_BACK)/$(SRC)/*.[ch] $(HTML_BACK)/$(OS_SRC)/*.[ch] $(HTML_BACK)/$(ARCH_SRC)/*.[ch] $(HTML_BACK)/$(LIB_SRC)/*.[ch])

# Code cleanliness craze - enforcing the rules, add suggestions to Case #133
CHECK_SRC=*.[ch] */*.[ch]  # all by default, you can override CHECK_SRC=win32/os.c
# The output of make ugly rule can easily be used in emacs with C-x `
# These are as separate rules so that you can disable them easily
# Use cvs annotate to see what you're responsible for.
# Don't fix these if someone else maybe working on a file!
# Note - we use "|| true" below after the grep cmds to avoid annoying make error output.
# All ugly rules should pass, the short regression verifies this.  Rules that don't quite
# yet pass, but almost do are in pretty_ugly. Rules that have too many false positives
# or are too prevelant to really consider fixing are in really_ugly.
# All rules correspond to coding style directives and rules in docs/codestandards.html
UGLY_RULE_HEADER = Make ugly rule:
# NOTE - runregression depends on the exact value of UGLY_RULE_HEADER
ugly:
# Rules that should pass.
	@$(ECHO) "$(UGLY_RULE_HEADER) remove extraneous debugging facilities"
	-$(GREP) -n -H "TRACELOG([^l]\|NOCHECKIN" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) use SYSLOG_INTERNAL_*_ONCE instead of DO_ONCE(SYSLOG_INTERNAL"
	-$(GREP) -n -H "DO_ONCE({\?[[:space:]]*SYSLOG_INTERNAL" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) use ASSERT_CURIOSITY_ONCE instead of DO_ONCE(ASSERT"
# note that ASSERT_NOT_TESTED is already DO_ONCE so shouldn't see DO_ONCE on any ASSERT
	-$(GREP) -n -H "ONCE({\?[[:space:]]*ASSERT" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) clean TABs w/ M-x untabify ($(CAT) -T | $(GREP) "\^I" to see)"
	-$(GREP) -n -H "	" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) minor style issues"
# TODO - add perl rule for multiline if bodies (grep isn't up to the task)
	@$(ECHO) "$(UGLY_RULE_HEADER) put if body on separate line"
# since is grep, doesn't handle the if conditional taking more then one line
	-$(GREP) -n -H "[^[:alnum:]\-_]if[[:space:]]*(.*;" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) put loop body on separate line"
# since is grep, doesn't handle the while conditional taking more then one line
# check for while loop
	-$(GREP) -n -H "^[^}]*[^[:alnum:]\-_]while[[:space:]]*(.*;" $(CHECK_SRC) || true
# check that the while closing a do loop is on its own line
	-$(GREP) -n -H "^.*[^}[:space:]]\+.*[^[:alnum:]\-_]while[[:space:]]*(.*;" $(CHECK_SRC) || true
	-$(GREP) -n -H "[^[:alnum:]\-_]do[[:space:]]*{.*;" $(CHECK_SRC) || true
# since is grep, doesn't handle the for pre/check/post taking more then one line
	-$(GREP) -n -H "[^[:alnum:]\-_]for[[:space:]]*(.*;.*;.*).*;" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) (1 == var) vs. (var == 1)"
	-$(GREP) -n -H "([[:space:]]*\([[:digit:]]\+\|NULL\)[[:space:]]*==[[:space:]]*[^[:digit:]]" $(CHECK_SRC) || true
# check that empty param list is declared void
# FIXME: should we only require for decls by only running on header files?
# But we do want to catch static decls.
	-$(GREP) -n -H "^[[:alnum:]][[:alnum:]_\* ]\+();" $(CHECK_SRC) || true

pretty_ugly: ugly
# Rules that should pass (and almost do, all have only a few violations left and
# no false positives). FIXME - should be cleaned up and moved to ugly xref 8806.
	@$(ECHO) "$(UGLY_RULE_HEADER) minor style issues"
	@$(ECHO) "$(UGLY_RULE_HEADER) spacing, if( or if  ( vs if ("
	-$(GREP) -n -H "[^[:alnum:]\-_]\(do\|if\|while\|for\|else\|switch\)\([[:space:]][[:space:]]\+\)\?[({]" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) spacing, }else or }  else vs } else"
	-$(GREP) -n -H "[});]\([[:space:]][[:space:]]\+\)\?\(do\|if\|while\|for\|else\|switch\)\([^[:alnum:]\-_]\|$$\)" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) empty param list should be (void) not ()"

really_ugly: pretty_ugly
# Rules that don't pass at all, either because of excessive violations in the
# codebase or because of false positives. FIXME - for rules with few or no false
# positives but numerous violations we could use a count or something to try and
# keep things from getting worse xref case 8806.
	@$(ECHO) "$(UGLY_RULE_HEADER) long lines"
# no false positives, but numerous violations
	-$(GREP) -n -H ".\{90,\}" $(CHECK_SRC) || true
	@$(ECHO) "$UGLY_RULE_HEADER) one statement per line"
# With all the exemptions very few false positives, but still many violations
# (and prob. some false negatives). The first grep looks for lines with two ; not
# within quotes. 2nd, 3rd and 4th greps subtract out lines where the extra
# semicolon is prob. part of a comment. NOTE, for some reason [[:space:]] doesn't
# work for matching before the * in the 3rd grep. 5th grep subtracts out for (;;)
# loop usage and 6th grep subtracts out the very common 'case: foo; break;' motif
# (even though it technically breaks the rule). Final grep subtracts out certain
# common macros whose usage often breaks the rule.
	-$(GREP) -n -H -E '^(([^;"]|("([^"]|\\")*"))*;){2,}' $(CHECK_SRC) |\
	 $(GREP) -vE "^[^;]*;?[^;]*/\*" | $(GREP) -v "^[^;\*=\(]*\*[[:space:]]" |\
	 $(GREP) -vE  "^[^;]*;?[^;]*//" | $(GREP) -v "for" | $(GREP) -v "break;[^;]*$$" |\
	 $(GREP) -vE "DODEBUG|DOLOG|DOSTATS|DO_VERBOSE" || true
	@$(ECHO) "$(UGLY_RULE_HEADER) instr_t* foo vs. instr_t *foo, etc."
# only a few false positives, but numerous violations
	-$(GREP) -n -H "[[:alnum:]]\*[^[:alnum:]/\)\(]" $(CHECK_SRC) || true 
	@$(ECHO) "$(UGLY_RULE_HEADER) use C style comments"
# only a few false positives, but numerous violations (even allowing for usage
# inside /* */ comments)
	-$(GREP) -n -H "//" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) clean up commented out code"
# mostly false positives (we're too grammatical ;) due to using ; in comments
	-$(GREP) -n -H ".*//.*;\|.*/\*.*;.*\*/" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) use int/uint instead of long/ulong"
# some false positives and numerous violations
	-$(GREP) -n -H "^[^\*]*long" $(CHECK_SRC) || true
	@$(ECHO) "$(UGLY_RULE_HEADER) spacing, ( foo, bar ) vs. (foo, bar)"
# Only a few false positives and not that many violations (for  the "( " rule at
# least). FIXME - these could probably be moved to pretty_ugly at some point.
	-$(GREP) -n -H "([[:space:]]\+[^\\[:space:]]\+" $(CHECK_SRC) || true
	-$(GREP) -n -H "[[:space:]])" $(CHECK_SRC) | $(GREP) -v -e "DEBUG_DECLARE" || true
	@$(ECHO) "$(UGLY_RULE_HEADER) put spaces after commas in arg lists etc."
# no real false positives, but tons of violations (even ignoring asm listings)
	-$(GREP) -n -H ",[^[:space:]]" $(CHECK_SRC) || true

$(BUILDMARK_H): 
	@$(ECHO) -e "#define DYNAMORIO_DEFINES \""$(DEFINES)"\"\n"\
	"extern const char dynamorio_version_string[];\n"\
	"extern const char dynamorio_buildmark[];" > $@

# we ignore whitespace since from within emacs the "extern" lines have an initial space.
# note that "-b" is not sufficient since it complains about no whitespace versus some
# whitespace: it only suppresses changes in numbers >= 1.
DIFF_DEFINES := $(DIFF) --ignore-all-space
compare-defines: $(BUILDMARK_H)
ifeq ($(IGNOREDEF), y)
	@$(DIFF_DEFINES) $^ || $(ECHO) Inconsistency ignored
#still keep POST_DEFINES_H as a mark
else
	! test -e $< || $(DIFF_DEFINES) $^ || ($(ECHO) Makefile.mydefines:1:Possibly inconsistent build. Run make clean all; exit 1)
	$(RM) $(POST_DEFINES_H)
endif

clean-mark:
	$(RM) $(BUILDMARK_OBJS)

buildmark: compare-defines clean-mark $(BUILDMARK_OBJS)

# display the build number
buildnum:
	@$(ECHO) build number is ${BUILD_NUMBER}

# will always compare defines with current and then recreate buildmark.o 

# FIXME: PR 214218 - For the current cygwin version, 'make clear'
# fails to remove the build directory.  The problem stems from
# implicit rules of the form:
# 
#  $(BUILD)/%.$(OBJ_SFX): $(SRC)/%.c
#
# These rules cause make to hold an open handle to the directory
# $(BUILD).  To avoid this problem, the best solution is probably to
# use static patterns:
#
#  $(OBJS): $(BUILD)/%.$(OBJ_SFX): $(SRC)/%.c
#
# This will require extensive changes, however.  An easier solution is
# to ifdef out all rules referencing the $(BUILD) dir for 'make
# clear'.  The disadvantage of this approach is that commands like
# 'make clear all' don't work as desired.
ifneq ($(MAKECMDGOALS),clear) 

###########################################################################
##
## Non-phony rules

$(MESSAGE_STRINGS_H) : $(MESSAGE_STRINGS_FILES) gen_event_strings.pl
	$(PERL) gen_event_strings.pl $(MESSAGE_STRINGS_FILES) $(MESSAGE_STRINGS_H)

ifneq ($(STATIC),1) ########################################

# injector
ifeq ($(MACHINE), win32)
injector: builddir $(INSTALL_BIN)/$(INJECTOR) $(INSTALL_LIB)/$(ARCHIVE)
$(INSTALL_BIN)/$(INJECTOR): $(BUILD)/$(INJECTOR)
	$(CP) $(BUILD)/$(INJECTNAME).{exe,map} $(INSTALL_BIN)
ifneq ($(DBG),) # sometimes can't build pdb (case 192750)
	$(CP) $(BUILD)/$(INJECTNAME).pdb $(INSTALL_BIN)
endif
$(BUILD)/$(INJECTOR): $(INJECT_OBJS)
	$(LD) $(INJECTFLAGS) $(INJECT_OBJS) $(INJECTLIBS)

early_inject_helper: builddir $(INSTALL_LIB_BASE)/$(EARLY_INJECT_HELPER1) $(INSTALL_LIB_BASE)/$(EARLY_INJECT_HELPER2)
$(INSTALL_LIB_BASE)/$(EARLY_INJECT_HELPER1): $(BUILD)/$(EARLY_INJECT_HELPER1)
	$(CP) $(BUILD)/$(EARLY_INJECT_HELPER1_NAME).{dll,map} $(INSTALL_LIB_BASE)
ifneq ($(DBG),) # sometimes can't build pdb (case 192750)
	$(CP) $(BUILD)/$(EARLY_INJECT_HELPER1_NAME).pdb $(INSTALL_LIB_BASE)
endif
$(INSTALL_LIB_BASE)/$(EARLY_INJECT_HELPER2): $(BUILD)/$(EARLY_INJECT_HELPER2)
	$(CP) $(BUILD)/$(EARLY_INJECT_HELPER2_NAME).{dll,map,lib} $(INSTALL_LIB_BASE)
ifneq ($(DBG),) # sometimes can't build pdb (case 192750)
	$(CP) $(BUILD)/$(EARLY_INJECT_HELPER2_NAME).pdb $(INSTALL_LIB_BASE)
endif
$(BUILD)/$(EARLY_INJECT_HELPER1): $(EARLY_INJECT_HELPER1_OBJS) $(BUILD)/$(EARLY_INJECT_HELPER2)
	$(LD) $(EARLY_INJECT_HELPER1_FLAGS) $(EARLY_INJECT_HELPER1_OBJS) $(EARLY_INJECT_HELPER1_LIBS)
$(BUILD)/$(EARLY_INJECT_HELPER2): $(EARLY_INJECT_HELPER2_OBJS)
	$(LD) $(EARLY_INJECT_HELPER2_FLAGS) $(EARLY_INJECT_HELPER2_OBJS) $(EARLY_INJECT_HELPER2_LIBS)


# pre-injector: separate library + copy of dynamo library in special place
preload: builddir $(INSTALL_LIB_BASE)/$(PRELOAD) $(INSTALL_LIB)/$(ARCHIVE)

$(INSTALL_LIB_BASE)/$(PRELOAD): $(PRE_INJECT_OBJS)
	$(LD) $(PRE_INJECTFLAGS) $(PRE_INJECT_OBJS) $(PRE_INJECTLIBS)
	$(CP) $(BUILD)/$(PREINJECTNAME).{dll,map} $(INSTALL_LIB_BASE)
ifneq ($(DBG),) # sometimes can't build pdb (case 192750)
	$(CP) $(BUILD)/$(PREINJECTNAME).pdb $(INSTALL_LIB_BASE)
endif

# TODO: use a RLL library instead of putting everything in one dll
$(BUILD)/%.rll : $(BUILD)/$(OSNAME)/%.res
	$(LD) -dll -noentry -out:$(subst /,\\,$@) $<
endif

ifeq ($(MACHINE), linux)
LDDYNAMO = -L$(INSTALL_LIB) -ldynamorio

preload: builddir $(INSTALL_LIB_BASE)/$(PRELOAD) $(INSTALL_LIB)/$(ARCHIVE)
$(INSTALL_LIB_BASE)/$(PRELOAD): $(BUILD)/$(PRELOAD)
# PR 212290: ensure no text relocations (they violate selinux execmod policies)
	$(READELF) -d $(BUILD)/$(PRELOAD) | $(GREP) -q TEXTREL; \
	if [ $$? == 0 ]; then \
	    $(ECHO) "$(BUILD)/$(PRELOAD) has text relocations" && false; \
	fi
# also check for execstack
	$(READELF) -l $(BUILD)/$(PRELOAD) | $(GREP) STACK | $(GREP) -q RWE; \
	if [ $$? == 0 ]; then \
	    $(ECHO) "$(BUILD)/$(PRELOAD) has executable stack" && false; \
	fi
	$(CP) -a $(BUILD)/$(PRELOAD) $(INSTALL_LIB_BASE)/$(PRELOAD)

$(BUILD)/$(PRELOAD): $(PRELOAD_OBJS)
# FIXME case 69/1891: -z initfirst = initialize first at runtime (before libc)
#	$(CC) -shared -nostartfiles -Xlinker -z -Xlinker initfirst $< $(CC_LDLIBS) $(LDDYNAMO) -o $@
	$(CC) $(CFLAGS) -shared -nostartfiles $< $(CC_LDLIBS) $(LDDYNAMO) -o $@
endif

endif # ifneq ($(STATIC),1) ########################################

ifeq ($(MACHINE), win32)
TYPE_EXE = VFT_APP
TYPE_DLL = VFT_DLL
BUILD_RESOURCE_COMMON = $(RC) -r -fo $(subst /,\\,$@) $(INCLUDE_PATHS) $(GENERATED_INCLUDES) $(DEFINES) $(RESOURCE_DEFINES)

# make resources - consumes both a resource and the associated BIN file
$(BUILD)/dynamorio.res : $(SRC)/$(OSNAME)/resources.rc $(BUILD)/$(OSNAME)/events.rc
	 $(BUILD_RESOURCE_COMMON) $(D)INCLUDE_EVENTS $(D)FILE_NAME=\"$(ARCHIVE)\" $(D)FILE_TYPE=$(TYPE_DLL) $(D)FILE_DESCRIPTION="PRODUCT_NAME \" core library\"" $<

$(BUILD)/injector.res : $(SRC)/$(OSNAME)/resources.rc
	$(BUILD_RESOURCE_COMMON) $(D)FILE_NAME=\"$(INJECTOR)\" $(D)FILE_TYPE=$(TYPE_EXE) $(D)FILE_DESCRIPTION="PRODUCT_NAME \" injection utility\"" $<

$(BUILD)/preinjector.res : $(SRC)/$(OSNAME)/resources.rc
	$(BUILD_RESOURCE_COMMON) $(D)FILE_NAME=\"$(PRELOAD)\" $(D)FILE_TYPE=$(TYPE_DLL) $(D)FILE_DESCRIPTION="PRODUCT_NAME \" systemwide injector dll\"" $<

$(BUILD)/drearlyhelp1.res : $(SRC)/$(OSNAME)/resources.rc
	$(BUILD_RESOURCE_COMMON) $(D)FILE_NAME=\"$(EARLY_INJECT_HELPER1)\" $(D)FILE_TYPE=$(TYPE_DLL) $(D)FILE_DESCRIPTION="PRODUCT_NAME \" additional win2k injector dll\"" $<

$(BUILD)/drearlyhelp2.res : $(SRC)/$(OSNAME)/resources.rc
	$(BUILD_RESOURCE_COMMON) $(D)FILE_NAME=\"$(EARLY_INJECT_HELPER2)\" $(D)FILE_TYPE=$(TYPE_DLL) $(D)FILE_DESCRIPTION="PRODUCT_NAME \" additional win2k injector dll\"" $<

# start from message files
# NOTE: a BIN file is also generated and pointed to by the .rc  $(BUILD)/$(OSNAME)/%.BIN
$(BUILD)/$(OSNAME)/%.rc $(BUILD)/$(OSNAME)/%.h: $(OSNAME)/%.mc
	$(MC) -h $(subst /,\\,$(BUILD)/$(OSNAME)) -r $(subst /,\\,$(BUILD)/$(OSNAME)) $<
endif # ifeq  ($(MACHINE), win32)

ifeq ($(STATIC),1)
install: unexport all
else
# FIXME - should install also copy early inject helper dlls to system32?
install: unexport all preload
endif
ifeq ($(MACHINE), win32)
# get the .lib file too for linking (_app interface)
	$(CP) $(BUILD)/$(subst dll,lib,$(ARCHIVE)) $(INSTALL_LIB)
endif

# lib helper: fortran
$(INSTALL_LIB)/$(FORTRAN_OBJ): $(BUILD)/$(FORTRAN_OBJ)
	$(CP) $(BUILD)/$(FORTRAN_OBJ) $(INSTALL_LIB)

ifeq ($(MACHINE), linux)
ifeq ($(LINKER_VERSION_SCRIPT), 1)
# generate linker script to limit exports
$(BUILD)/$(ARCHIVE): $(EXPORT_LIST)
$(EXPORT_LIST): $(GENAPI)
	$(PERL) $(GENAPI) -filter "$(DEFINES)" > $@
endif
endif

# dynamo library itself
$(INSTALL_LIB)/$(ARCHIVE): $(BUILD)/$(ARCHIVE) $(GENAPI)
ifeq ($(MACHINE), linux)
# PR 212290: ensure no text relocations (they violate selinux execmod policies)
	$(READELF) -d $(BUILD)/$(ARCHIVE) | $(GREP) -q TEXTREL; \
	if [ $$? == 0 ]; then \
	    $(ECHO) "$(BUILD)/$(ARCHIVE) has text relocations" && false; \
	fi
# also check for execstack
	$(READELF) -l $(BUILD)/$(ARCHIVE) | $(GREP) STACK | $(GREP) -q RWE; \
	if [ $$? == 0 ]; then \
	    $(ECHO) "$(BUILD)/$(ARCHIVE) has executable stack" && false; \
	fi
	$(CP) -a $(BUILD)/$(ARCHIVE) $(INSTALL_LIB)
	$(SIZE) $(INSTALL_LIB)/$(ARCHIVE)
else
	$(CP) $(BUILD)/$(OUTNAME).{dll,lib,map} $(INSTALL_LIB)
ifneq ($(DBG),) # sometimes can't build pdb (case 192750)
	$(CP) $(BUILD)/$(OUTNAME).pdb $(INSTALL_LIB)
endif
	$(DUMPBIN) /summary $(INSTALL_LIB)/$(ARCHIVE)
endif # WINDOWS

ifeq ($(CLIENT_INTERFACE), 1)
api_headers: $(GENAPI) $(HEADERS) builddir
# create header files to complete IR API
	$(RM) $(INSTALL_INCLUDE)/dr_*.h
	$(CP) $(LIB_SRC)/dr_api.h $(INSTALL_INCLUDE)
ifeq ($(MACHINE), linux)
	$(PERL) $(GENAPI) -header $(INSTALL_INCLUDE) "$(API_DEFINES)"
else
	$(PERL) $(GENAPI) -header $(INSTALL_INCLUDE) "$(API_DEFINES)"
endif
# kill #ifdefs from func declarations in dr_app.h,
# but not from #defines at top (WINDOWS, etc.)
# FIXME: make this simpler!  do we really have to write another script?
# there must be a better way to selectively strip defines from our header files
	export line=`$(GREP) -n '/\*\*\*\*' $(LIB_SRC)/dr_app.h | $(CUT) -d : -f 1 | $(AWK) '{print $$1 - 1}'`; \
	$(HEAD) -$$line $(LIB_SRC)/dr_app.h > $(INSTALL_INCLUDE)/dr_app.h; \
	export rest=`$(WC) $(LIB_SRC)/dr_app.h $(INSTALL_INCLUDE)/dr_app.h | $(XARGS) $(ECHO) | $(AWK) '{print $$1 - $$5}'`; \
	$(TAIL) -$$rest $(LIB_SRC)/dr_app.h | $(SED) 's/#endif \/\* _DR_APP_H_ \*\///' > ._foo; \
	$(CPP) $(CPP_KEEP_COMMENTS) $(CPP_NO_LINENUM) $(subst /,-,$(DEFINES)) ._foo >> $(INSTALL_INCLUDE)/dr_app.h; \
	$(TAIL) -2 $(LIB_SRC)/dr_app.h >> $(INSTALL_INCLUDE)/dr_app.h; \
	$(SED) -i 's/\r//' $(INSTALL_INCLUDE)/dr_app.h
	$(RM) ._foo
# FIXME - would be better if this was just not generated at all when !HOT_PATCING_INTERFACE.
# Need to add appropriate ifdefs for the core, for now this is the easiest path.
ifneq ($(HOT_PATCHING_INTERFACE), 1)
	$(RM) $(INSTALL_INCLUDE)/dr_probe.h
endif
else
ifeq ($(APP_EXPORTS), 1)
api_headers: $(LIB_SRC)/dr_app.h builddir
	$(RM) $(INSTALL_INCLUDE)/dr_*.h
	$(CP) $(LIB_SRC)/dr_app.h $(INSTALL_INCLUDE)
else
api_headers:
endif
endif

dynamorio: $(DEFINES_H) compare-defines usedep_defines $(INSTALL_LIB)/$(ARCHIVE) api_headers

standalone: $(DEFINES_H) compare-defines usedep_defines $(BUILD)/$(STANDALONE_EXE)

usedep_defines:
# HACK: 1st clean, while doing deps, decides DEFINES_H does not need to be remade,
# then does the clean, and then if another target is specified, DEFINES_H is
# already on prune list and we end up w/ a compilation error since it's missing...
	(if ! test -e $(DEFINES_H); then $(MAKE) builddir $(DEFINES_H); fi)

ifeq ($(ARCH), x64) # see notes below on PR 253624
SET_PREFERRED_BASE := 1
PREFERRED_BASE := 0x71000000
LINKER_DEFAULT_BASE:=0x400000
else
  ifeq ($(VMKERNEL), 1)
    # we end up with the default executable base (0x08*) so go back to 0 base
    # (else we fail to load on esx)
    SET_PREFERRED_BASE := 1
    PREFERRED_BASE := 0x00000000
  else
    # off by default, but can be specified
    SET_PREFERRED_BASE := 0
  endif
LINKER_DEFAULT_BASE:=0x08048000
endif

# we always use a script for our own library bounds (PR 361594)
LD_USE_SCRIPT := -Xlinker $(LD_SCRIPT_OPTION) -Xlinker $(LD_SCRIPT)

$(BUILD)/$(ARCHIVE): $(MESSAGE_STRINGS_H) $(MESSAGE_H) $(OBJS)
ifeq ($(MACHINE), linux)
ifeq ($(STATIC), 1)
	ar r $@ $(OBJS) 
else
#	$(LD) -shared -Bsymbolic $(OBJS) $(LD_LDLIBS) -o $@ /usr/lib/gcc-lib/i386-redhat-linux/egcs-2.91.66/libgcc.a
#	$(CC) -shared -nostartfiles -Xlinker -Bsymbolic -Xlinker -z -Xlinker now $(OBJS) $(CC_LDLIBS) -o $@
#	$(CC) -nostdlib -shared -nostartfiles -Xlinker -Bsymbolic -Xlinker -rpath -Xlinker $(DYNAMORIO_HOME)/${CORE_DIRNAME}-signals/lib $(OBJS) $(CC_LDLIBS)  -L$(LIB_SRC) -lourlibc -o $@
# -z now = do non-lazy runtime binding
	$(MAKE) buildmark
# PR 361954: set up vars pointing at our own library bounds using the
# symbols __executable_start and end defined in default script
	$(LD) --verbose | $(SED) '/SECTIONS/,/^}/!d' | \
	  $(SED) 's/^}/PROVIDE(dynamorio_so_start = __executable_start); PROVIDE(dynamorio_so_end = end);\n}/;' \
	  > $(LD_SCRIPT)
ifeq ($(SET_PREFERRED_BASE), 1)
# PR 253624: right now our library has to be next to our heap, and our
# heap needs to be in lower 4GB for our fallback TLS (PR 285410), so we
# set a preferred base address via a linker script.
# In order to just tweak the default linker script we start with exactly that.
	$(SED) -i 's/$(LINKER_DEFAULT_BASE)/$(PREFERRED_BASE)/g; s/^{/{\n . = 0x71000000;/;' $(LD_SCRIPT)
endif
	$(CC) $(CFLAGS) -shared -nostartfiles -Xlinker -z -Xlinker now -Xlinker -Bsymbolic $(LINKER_EXPORT_FLAGS) $(OBJS) $(CC_LDLIBS) -o $@ $(LD_USE_SCRIPT)
endif
else # win32
	$(CP) $(DEF_SRC) $(DEF_BLD)
ifeq ($(CLIENT_INTERFACE), 0)
# we're not exporting snprintf() so remove from def file */
	$(SED) -i '/ snprintf/d' $(DEF_BLD)
endif
	$(LD) $(LDFLAGS) $(OBJS) $(LD_LDLIBS)
endif

$(BUILD)/$(STANDALONE_EXE): $(MESSAGE_STRINGS_H) $(MESSAGE_H) $(UNIT_OBJS)
ifeq ($(MACHINE), linux)
	$(MAKE) buildmark
	$(CC) $(UNIT_OBJS) $(CC_LDLIBS) -o $@
else # win32
	$(LD) $(TGTFLAGS) $(UNIT_OBJS) $(LD_LDLIBS) /out:$@ \
		/pdb:"$(MSBUILD)\\$(STANDALONE_NAME).pdb"
endif
# FIXME: copy to exports or anything?

# .c -> .o
$(BUILD)/%.$(OBJ_SFX): $(SRC)/%.c
ifeq ($(MACHINE), win32)
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $(GENERATED_INCLUDES) $(DEFINES) $(INCR) /TC $(subst /,\\,$<) $(OUT)$(subst /,\\,$@) /Fd$(subst /,\\,$(subst .$(OBJ_SFX),.pdb,$@)) $(FILTER_CC)
else # linux
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $(GENERATED_INCLUDES) $(DEFINES) $(INCR) $< $(OUT)$@
endif

# non-core build of a core file
# FIXME: this is a hack -- ugly duplication of above rule -- but
# no simpler alternatives (could create ntdll-noncore.c that defines
# NOT_DYNAMORIO_CORE_PROPER, or a separate make:
# make NONCORE=1 BUILD=noncore ADD_DEFINES=NOT_DYNAMORIO_CORE_PROPER)
#
# -noncore.c -> -noncore.o
# FIXME: This rule should use gcc on both Windows and Linux!
$(BUILD)/%-noncore.$(OBJ_SFX): $(SRC)/%.c
ifeq ($(MACHINE), win32)
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $(GENERATED_INCLUDES) $(DEFINES) $(D)NOT_DYNAMORIO_CORE_PROPER $(INCR) /TC $(subst /,\\,$<) $(OUT)$(subst /,\\,$@) /Fd$(subst /,\\,$(subst .$(OBJ_SFX),.pdb,$@))
else # linux
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) $(GENERATED_INCLUDES) $(DEFINES) $(D)NOT_DYNAMORIO_CORE_PROPER $(INCR) $< $(OUT)$@
endif

# .c -> .cod (and implicitly .$(OBJ_SFX))
$(BUILD)/%.cod: $(BUILD)/%.c
ifeq ($(MACHINE), win32)
	$(CC) $(CFLAGS) $(INCR) /TC $(subst /,\\,$<) $(OUT)$(subst /,\\,$(subst .cod,.$(OBJ_SFX),$@)) /Fd$(subst /,\\,$(subst .cod,.pdb,$@)) /FAcs /Fa$(subst /,\\,$@)
endif

## We use .asm as a platform-independent assembly file (for cygwin and linux),
## and generate a .s file in the build directory from the .asm file:

# .asm -> .s
# we want ".ifdef FOO" to not turn into ".ifdef 1" so we say "-DFOO=FOO":
ASM_CPP_DEFINES := $(join $(DEFINES),$(patsubst $(D)%,=%,$(DEFINES)))
$(BUILD)/%.s: $(SRC)/%.asm
	$(CPP) $(PREPROC) $(ASMPREPROC) $(CPPFLAGS) $(ASM_CPP_DEFINES) $< $(CPPOUT)$@
# since we don't want to duplicate gas and ml macros we use cpp macros w/
# the added feature of insertable newlines, needed for asm code, via "@N@":
	$(SED) -i 's/\@N\@/\n/g' $@

$(BUILD)/%-noncore.s: $(SRC)/%.asm
	$(CPP) $(PREPROC) $(ASMPREPROC) $(CPPFLAGS) $(ASM_CPP_DEFINES) $(ASM_NDCP_DEFINES) $< $(CPPOUT)$@
# we can't put in newlines with cpp so we have sed do it
	$(SED) -i 's/\@N\@/\n/g' $@

# .s -> .o
$(BUILD)/%.$(OBJ_SFX): $(BUILD)/%.s
# ml and ml64 ignore /Fo unless it is BEFORE the source file
	$(AS) $(ASMFLAGS) $(OUT)$@ $<

# .c -> .d
# see info make Prerequisites
# cannot depend on builddir, else make will keep rebuilding .dep files, since
# every time it reads Makefile it will think they need to be remade b/c of builddir
# always being out-of-date, so instead we test for presence inside the command
$(BUILD)/%.$(DEP_SFX): $(SRC)/%.c
	@(if test -e $(BUILD); then $(GCC) $(DEPENDS) $(CPPFLAGS) $(INCLUDE_PATHS) $(GENERATED_INCLUDES) \
                $(DEFINES) -MQ $(patsubst %.$(DEP_SFX),%.$(OBJ_SFX), $@) -MQ $@ $(SRC)/$*.c > $@ ; fi)

depend: builddir $(DEPS)
	@$(ECHO) Generated dependency files for make USEDEP=1
	@$(ECHO) If new files are added use make clean depend.

endif # ifneq ($(MAKECMDGOALS),clear)

# include dependencies
ifeq ($(USEDEP), 1)
-include $(DEPS)
endif

# External modules go here, and will be build if present
ifeq ($(STRACE_CLIENT), 1)
CLIENTS += ../clients/stracedr
endif

.PHONY:: $(CLIENTS)

clients: $(CLIENTS)

$(CLIENTS):
	@(if test -e $(CLIENTS); then $(MAKE) -C $@ all; fi)

###########################################################################
# non build related targets follow

###########################################################################
# this target should be able to run all unit tests
# currently requires make clean unitall if the core was previously built
allunit: 
	-$(MAKE) UNIT_TEST=rct NOLIBC=0 STANDALONE_UNIT_TEST=1 ADD_DEFINES=$(D)RCT_IND_BRANCH UNIT_ADD_OPTIONS='-rct_ind_call 11 -rct_ind_jump 11 -no_diagnostics' unitrun
	$(MAKE) clean
	-$(MAKE) UNIT_TEST=options NOLIBC=0 STANDALONE_UNIT_TEST=1 ADD_DEFINES=$(D)OPTIONS_UNIT_TEST unitrun
	$(MAKE) clean
	-$(MAKE) UNIT_TEST=vmareas NOLIBC=0 STANDALONE_UNIT_TEST=1 ADD_DEFINES=$(D)VMAREAS_UNIT_TEST unitrun

###########################################################################
# run a target program with the current library
ifeq ($(MACHINE), win32)
# .exe added automatically
TESTPROG=notepad
progrun: all
	${INSTALL_BIN}/${INJECTOR} ${INSTALL_LIB}/${ARCHIVE} ${TESTPROG}${EXE_SFX} ${TESTARGS}
else
TESTPROG=ls
progrun: all
	export LD_LIBRARY_PATH=${CUR_FULL}/${CORE_DIRNAME}/${INSTALL_LIB}/:$$LD_LIBRARY_PATH \
	LD_PRELOAD="$(PRELOAD) $(ARCHIVE)"; ${TESTPROG}${EXE_SFX} ${TESTARGS}
endif

## prefast
# used only when building dynamorio.sys
PREFAST_STACKHOG_THRESHOLD=512
PREFAST_LOG=${BUILD}/prefast.log
prefast: builddir
	-prefast /reset /StackHogThreshold=${PREFAST_STACKHOG_THRESHOLD} /log=${PREFAST_LOG} $(MAKE) -k 

prefastlist: prefast
	prefast /log=${PREFAST_LOG} list | $(TEE) ${BUILD}/prefast.err

prefastview: # shows last results
	prefast /log=${PREFAST_LOG} view


###########################################################################
# Support for adding symbols to a symbol server
# you can do this only on Windows
ifeq ($(MACHINE), win32)

# FIXME: if have never connected this will fail
# how specify username of guest?
# NOTE: could add a "logon to symbol server" target which executes:
#  cmd /c net use \\\\deter-build.determina.com\\symbols "" /user:guest
SYMBOL_SERVER=\\\\10.1.5.15\\symbols


# vlk: I also like my VMs to have host only network so then I do
#   subst y: z:\shared\symbols   # which would be host shared with /mnt/fileserver/symbols if that were writable
#   SYMBOL_SERVER=y:\\

SYMBOL_STORE=${SYMBOL_SERVER}\\product
SYMBOL_ADMIN=$(SYMBOL_STORE)\\000Admin

ifndef BUILD_NUMBER
  BUILD_NUMBER=custom
endif
PRODUCT_NAME="DynamoRIO"
DATE='$(shell $(DATE) +"Built at %Y-%m-%d %T")'
COMMENT="Build ${BUILD_NUMBER} by $(USER)@$(COMPUTERNAME)"

symbols:
	@$(ECHO) "Connecting to $(SYMBOL_SERVER)"; \
	export sa=$(SYMBOL_ADMIN) ss=$(SYMBOL_STORE); \
	$(DYNAMORIO_TOOLS)/external/symstore add /r /f ${EXPORTS_BASE} \
	/s ${SYMBOL_STORE} /t ${PRODUCT_NAME} /v ${COMMENT} /c ${DATE} | $(PERL) -e '\
	$$sa=$$ENV{"sa"}; $$ss=$$ENV{"ss"}; \
	while (<>) { if (/^Finding ID...\s+(\d+)/) {$$id=$$1;} print $$_; } \
	open (SA, "< $$sa\\$$id"); print "\nFiles added:\n"; \
	while (<SA>) { print $$_; if (/(dynamorio.dll.[0-9A-H]+)/) {$$file=$$1;} } close(SA); \
	if (defined($$file)) { print "\nDR dll: $$ss\\$$file\\dynamorio.dll\n"; } \
	else { print "\nWarning: no dynamorio.dll seems to have been added!\n"; }'

endif # win32

##################################################
# Support for running regression suite 
#
# We update the target's tools, libutil, and suite dirs from the local copy, allowing
# testing of changes to other components than just core/.
# We do assume that benchmarks/ is already set up and pointed at by a
# DYNAMORIO_BENCHMARKS that is set in the shell rc file.
#
# To avoid the problem of a later checkout being newer than an older modified
# file, we can either clobber every time or use rsync w/ separate targets --
# we choose the latter, splitting up src targets, but assuming people rarely
# send over multiple tools/libutil/suite targets.
# By default we copy to a dedicated regression dir and don't clobber in-use
# checkouts, etc. on the remote machine.
#
# WARNING: if you have serious clock skew rsync may copy the same files multiple times
# in a row, thinking they're still new, or may not copy files that really are new.

ifdef DYNAMORIO_REGRESSION
  RUNREG_BASE := ${DYNAMORIO_REGRESSION}
else
  RUNREG_BASE := ~/dr/regression
endif
RUNREG_HOST=localhost
RUNREG_TREE=${RUNREG_BASE}/trees/${CUR_TREE}
RUNREG_SRC=${RUNREG_TREE}/${CORE_DIRNAME}
RUNREG_MAKE=${RUNREG_BASE}/make
RUNREG_TOOLS=${RUNREG_BASE}/tools
RUNREG_LIBUTIL=${RUNREG_BASE}/libutil
RUNREG_SUITE=${RUNREG_BASE}/suite
RUNREG_BENCHMARKS=${RUNREG_BASE}/benchmarks
RUNREG_API=${RUNREG_BASE}/api
ifeq (${RUNREG_HOST}, localhost)
RUNREG_LOC=
RUNREG_CMD=$(SHELL) -c
RUNREG_SCRIPT=$(PERL) runregression
else
RUNREG_LOC=${USER}@${RUNREG_HOST}:
RUNREG_CMD=ssh ${USER}@${RUNREG_HOST}
RUNREG_SCRIPT=perl runregression
endif
# DYNAMORIO_{TOOLS,LIBUTIL,MAKE,SUITE,BENCHMARKS,API} must be set
# for a regression.  We'll supply default values relative to the
# current directory if they're not explicitly provided.
ifndef DYNAMORIO_TOOLS
  DYNAMORIO_TOOLS := ../tools
endif
ifndef DYNAMORIO_LIBUTIL
  DYNAMORIO_LIBUTIL := ../libutil
endif
ifndef DYNAMORIO_SUITE
  DYNAMORIO_SUITE := ../suite
endif
ifndef DYNAMORIO_BENCHMARKS
  DYNAMORIO_BENCHMARKS := ../benchmarks
endif
ifndef DYNAMORIO_API
  DYNAMORIO_API := ../api
endif
ifeq ($(MACHINE), win32)
DR_MAKE=$(shell $(CYGPATH) -ui ${DYNAMORIO_MAKE})
DR_TOOLS=$(shell $(CYGPATH) -ui ${DYNAMORIO_TOOLS})
DR_LIBUTIL=$(shell $(CYGPATH) -ui ${DYNAMORIO_LIBUTIL})
DR_SUITE=$(shell $(CYGPATH) -ui ${DYNAMORIO_SUITE})
DR_BENCHMARKS=$(shell $(CYGPATH) -ui ${DYNAMORIO_BENCHMARKS})
DR_API=$(shell $(CYGPATH) -ui ${DYNAMORIO_API})
else
DR_MAKE=${DYNAMORIO_MAKE}
DR_TOOLS=${DYNAMORIO_TOOLS}
DR_LIBUTIL=${DYNAMORIO_LIBUTIL}
DR_SUITE=${DYNAMORIO_SUITE}
DR_BENCHMARKS=${DYNAMORIO_BENCHMARKS}
DR_API=${DYNAMORIO_API}
endif

# To save time when rsync'ing the benchmarks directory, only include
# the bare minimum needed for the regression:
BENCHMARK_SRCS:=RunSuite			\
		spec2000/Makefile*		\
		spec2000/tools			\
		spec2000/specperl		\
		spec2000/int/crafty/RunSpec	\
		spec2000/int/crafty/Makefile	\
		spec2000/int/crafty/src		\
		spec2000/int/crafty/data/test 	\
		spec2000/int/gcc/RunSpec	\
		spec2000/int/gcc/Makefile	\
		spec2000/int/gcc/src		\
		spec2000/int/gcc/data/test	\
		spec2000/int/mcf/RunSpec	\
		spec2000/int/mcf/Makefile	\
		spec2000/int/mcf/src		\
		spec2000/int/mcf/data/test	

# Note below that we use rsync -R when transferting portions of the
# benchmarks module so we can maintain the directory structure.  We
# need the './' syntax to get it right.  See the description of -R in
# the rsync man page.
BENCHMARK_SRCS := $(patsubst %,${DR_BENCHMARKS}/./%,${BENCHMARK_SRCS})

SHELL_RC=~/.bashrc

# If you have problems rsync-ing (replacing an in-use file, etc.) can disable by
# running with NORSYNC=1 (will still rsync the src tree)
NORSYNC = 0

# We use -C to avoid clobbering CVS files, so remote dirs can still be used as cvs co's
# if so desired.
RSYNC = rsync -auvzC --exclude=*.map --exclude=*.pdb

RSYNC_TOOLS_FLAGS := --include=/external/whoami.exe --exclude=/external/*

runregression:
	@(if [ ! -d ${DR_MAKE} ]; then \
		$(ECHO) "ERROR: ${DR_MAKE} does not exist!"; false; fi)
	@(if [ ! -d ${DR_TOOLS} ]; then \
		$(ECHO) "ERROR: ${DR_TOOLS} does not exist!"; false; fi)
	@(if [ ! -d ${DR_LIBUTIL} ]; then \
		$(ECHO) "ERROR: ${DR_LIBUTIL} does not exist!"; false; fi)
	@(if [ ! -d ${DR_SUITE} ]; then \
		$(ECHO) "ERROR: ${DR_SUITE} does not exist!"; false; fi)
	@(if [ ! -d ${DR_BENCHMARKS} ]; then \
		$(ECHO) "ERROR: ${DR_BENCHMARKS} does not exist!"; false; fi)
	@(if [ ! -d ${DR_API} ]; then \
		$(ECHO) "ERROR: ${DR_API} does not exist!"; false; fi)
# Since there may be no subdirs already created, and we can't use rsync's --relative
# w/o being able to break up the local path and w/o identical path tails, we use
# mkdir -p to make the dir structure.
	${RUNREG_CMD} "mkdir -p ${RUNREG_SRC}"
	${RSYNC} . ${RUNREG_LOC}${RUNREG_SRC}
ifeq (${NORSYNC}, 0)
	${RSYNC} ${DR_MAKE}/ ${RUNREG_LOC}${RUNREG_MAKE}
	${RSYNC} $(RSYNC_TOOLS_FLAGS) ${DR_TOOLS}/ ${RUNREG_LOC}${RUNREG_TOOLS}
	${RSYNC} ${DR_LIBUTIL}/ ${RUNREG_LOC}${RUNREG_LIBUTIL}
# FIXME: We no longer have the CVS/Entries file to easily get
# a list of directories to rsync.  We may end up sending more
# than necessary; for now just provide the long list of
# exclusions to try to avoid sending generated files.
	${RSYNC} \
	    --exclude=*.exe --exclude=*.obj --exclude=*.pdb	\
	    --exclude=*.ilk --exclude=*.clout --exclude=*.dll	\
	    --exclude=*.last --exclude=*.lib --exclude=*.exp	\
	    --exclude=results/ --exclude=TMPPREINJDIR/		\
	    --exclude=TMPPREINJDIR/ --exclude=stress/run/	\
	    --exclude=regression-*/ --exclude=cache/		\
	    --exclude=logs/ --include=events*dummy?.so		\
	    ${DR_SUITE}/ ${RUNREG_LOC}${RUNREG_SUITE}
# -C auto-ignores *.exe so we explicitly include specperl.exe here
	${RSYNC} -R --include=specperl.exe \
	    ${BENCHMARK_SRCS} ${RUNREG_LOC}${RUNREG_BENCHMARKS}
	${RSYNC} --exclude=/docs/html/ --exclude=/docs/latex/   \
                 --include=VIPA_test.exe                        \
	    ${DR_API}/ ${RUNREG_LOC}${RUNREG_API}
endif
# We must source .bashrc to get the compiler path, etc. set up
# (cygwin bash non-interactive does not source it even if started by sshd).
	${RUNREG_CMD} " \
	    if [ -e ${SHELL_RC} ]; then source ${SHELL_RC}; fi; \
	    cd ${RUNREG_SUITE}; \
            export DYNAMORIO_HOME=${RUNREG_TREE}; \
            export DYNAMORIO_TOOLS=${RUNREG_TOOLS}; \
            export DYNAMORIO_LIBUTIL=${RUNREG_LIBUTIL}; \
	    export DYNAMORIO_MAKE=${RUNREG_MAKE}; \
	    export DYNAMORIO_BENCHMARKS=${RUNREG_BENCHMARKS}; \
	    export DYNAMORIO_API=${RUNREG_API}; \
            ${RUNREG_SCRIPT} ${RUNREG_OPTIONS}" 2>&1 | $(TEE) ${REGRESSION}-${RUNREG_HOST}
	@$(GREP) "all builds succesful" ${REGRESSION}-${RUNREG_HOST}

specupdate:
	@if ! test ${DYNAMORIO_BENCHMARKS}; then $(ECHO) "ERROR: DYNAMORIO_BENCHMARKS not set"; exit -1; fi
# Send just config files for spec2000
# (need the dirs explicit so * doesn't squash them)
# Make sure your symlinks are local, if possible -- FIXME: runstats.c is not
	RUNREG_BENCHMARKS=`${RUNREG_CMD} "source ${SHELL_RC} > /dev/null; \
            cygpath -u \\$${DYNAMORIO_BENCHMARKS} 2> /dev/null || $(ECHO) \\$${DYNAMORIO_BENCHMARKS}"`; \
            ${RSYNC} --include=/spec2000/ --include=/spec2000/*/ \
            --include=/spec2000/*/*/ --include=/RunSuite --include=/spec2000**/Makefile* \
            --include=/spec2000**/RunSpec --include=/spec2000**/tools/* \
            --exclude=* ${DR_BENCHMARKS}/ ${RUNREG_LOC}$${RUNREG_BENCHMARKS}

up update: # (needed only since we don't have good make depends)
# FIXME: not very sophisticated version for starters with P4, 
# check if up to date, and update if not
#	(cvs status | grep Status | grep -vE 'Up-to-date|Locally') && (cvs update; make clear) || $(ECHO) Up-to-date
	$(P4) diff -sa  # new files not needed here
	$(P4) sync
	make clear
