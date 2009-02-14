# **********************************************************
# Copyright (c) 2007-2009 VMware, Inc.  All rights reserved.
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


# FIXME i#2: when proprietary we supported only a single version of
# each tool.  As open source we want to support a wide range, so we
# need a pre-make step, whether ./configure or cmake or whatever, to
# check for required tool versions.  For now we have some $(shell)
# invocations below but more performant to not do that every make.
#
# Here are some known constraints to build our API release package:
#
#   Linux:
#   - Require gcc built to target both 32-bit and 64-bit
#   - Require gas built to target both 32-bit and 64-bit
#   - Require gas >= 2.18.50 for -msyntax=intel -mmnemonic=intel
#     We provide our own version for building on older Linuxes
#   - Require gas >= 2.16 for fxsaveq
#   - Require gcc >= 4.x for -iquote: should still build ifdef HAVE_PROC_MAPS
#   - Require gcc >= 3.4 for -fvisibility: though we do have a workaround
#   - Require ld >= 2.18 for -dT: but -T should also work (see below)
#   Windows:
#   - Require cl for intrinsics
#   - Require cl >= 14.0 to target both 32-bit and 64-bit and for C99 macro varargs
#   - Require VC6 for DRgui, and VC71 for DRinstall
#   - Require cygwin perl and other cygwin tools
#   Both:
#   - Require doxygen >= 1.5.1, 1.5.3+ better, to build the docs
#   - Require fig2dev (transfig), ImageMagick, ghostscript to build the docs
#
# If typedef conflicts arise (such as on RHEL3), currently you'll have
# to manually resolve by defining one of our DR_DO_NOT_DEFINE_*
# defines (DR_DO_NOT_DEFINE_uint, DR_DO_NOT_DEFINE_ushort, etc.).
# Eventually we'll have a pre-make step that automates this.
#
# We need to be careful of subtle differences between
# compiler and header versions.  Xref issues in the past: 
#
#   * PR 212291: gcc strcmp intrinsics in gcc 4.x => stack overflow
#     (current workaround: -fno-builtin-strcmp)
#   * PR 229905: asm/ldt.h and bits/sigcontext.h changes
#     struct modify_ldt_ldt_s changed, sigcontext.h X86_FXSR_MAGIC include hurdles
#   * Changes in warnings (we're using -Werror/WX)
#   * we disabled strict aliasing opt in gcc 3.3.3
#     "gives warnings and makes bad assumptions" => -fno-strict-aliasing


# For windows builds, our makefiles assume cygwin is available
# and all necessary utilities are on the path.  For building
# on a non-cygwin windows machine, use the .bat scripts in the
# 'batch' directory.  Those take care of setting up the cygwin-
# related registry settings (i.e., mount points), specifying
# the path, and launching a cygwin bash shell with the right
# build command.  Note that we've been unable to get all the
# cygwin tools we need added to toolchain (xref PR 233837).
# Therefore, we have our own copy of cygwin utils in our local
# 'toolchain' module.  You can specify the location of that 
# module with the DYNAMORIO_TOOLCHAIN var.  If unspecified,
# we assume it's at ../toolchain.

ifndef DYNAMORIO_MAKE
  $(error DYNAMORIO_MAKE undefined. How did you include compiler.mk?)
endif

# Only Windows should have the OS env var set
# We can't use uname since we need to avoid hardcoded shell cmds
ifeq ($(OS),)
  MACHINE := linux
else
  MACHINE := win32
endif

#
# Variables for shell utilities
#
SHELL := /bin/bash

# These all come from PATH set up for either local cygwin or
# toolchain cygwin.  For Linux we use local versions.
AWK      := awk
CAT      := cat
CHMOD    := chmod
CP       := cp -f
CUT      := cut
CYGPATH  := cygpath
DATE     := date
DIFF     := diff
ECHO     := echo
GREP     := grep
HEAD     := head
LS       := ls
MKDIR    := mkdir
MV       := mv -f
PERL     := perl
RM       := rm -f
SED      := sed
STRINGS  := strings
TAIL     := tail
TEE      := tee
WC       := wc
XARGS    := xargs

# The following are for building our docs
# PR 340793: older doxygens are missing some features.
# e.g., 1.5.1 has no defines in Globals list, while 1.5.3
# seems to omit globals starting w/ 't'.  There may be other harder-to-spot
# errors, but so far they're minor enough that we ignore them.
DOXYGEN  := doxygen
MOGRIFY  := mogrify
FIG2DEV  := fig2dev

# for tools/DRinstall/
OD       := od

# for automated code reviews
SVN      := svn

# Further utilities are used but not by Makefiles we control so they
# have no defining variable:
# * gs is used by fig2dev
# * pdflatex and makeindex used by auto-generated api/docs/latex/Makefile
# * tar is used by makezip.sh
# * gzip is invoked by tar -z
# * zip is used by makezip.sh
# * make is used by makezip.sh
# * find is used by makezip.sh

# Paths
ifndef ARCH
  # default is x86 instead of x64
  ARCH := x86
endif
ifndef DYNAMORIO_BASE
  DYNAMORIO_BASE   :=..
else
  ifeq ($(MACHINE), win32)
    DYNAMORIO_BASE   :=$(shell $(CYGPATH) -ma $(DYNAMORIO_BASE))
  endif
endif
EXPORTS_BASE       :=$(DYNAMORIO_BASE)/exports
BUILD_BASE         :=$(DYNAMORIO_BASE)/build
ifndef BUILD_LIBUTIL
  BUILD_LIBUTIL    :=$(BUILD_BASE)/libutil
endif
ifndef BUILD_TOOLS
  BUILD_TOOLS      :=$(BUILD_BASE)/tools
endif
INSTALL_BIN_BASE   :=$(EXPORTS_BASE)/bin
ifeq ($(ARCH), x64)
  INSTALL_LIB_BASE :=$(EXPORTS_BASE)/lib64
  INSTALL_BIN      :=$(INSTALL_BIN_BASE)/bin64
else
  INSTALL_LIB_BASE :=$(EXPORTS_BASE)/lib32
  INSTALL_BIN      :=$(INSTALL_BIN_BASE)/bin32
endif
INSTALL_INCLUDE    :=$(EXPORTS_BASE)/include
INSTALL_DOCS       :=$(EXPORTS_BASE)/docs
# src/ or core/
CORE_DIRNAME :=core


#
# Windows
#
ifeq ($(MACHINE), win32)
  ifdef ENABLE_TOOLCHAIN
    ifneq ($(ENABLE_TOOLCHAIN),1)
      # we don't support the old bora-winroot
      $(error !ENABLE_TOOLCHAIN is not supported)
    endif
  endif

  #
  # Build variables
  # 
  I := /I
  D := /D
  U := /U
  L := /LIBPATH:
  INCR    := /c
  OUT     := /Fo
  EXEOUT  := /Fe
  # there's no way to specify a non-default name with /P
  PREPROC := /E
  CPPOUT  := >
  CPP_SFX :=i
  CPP_KEEP_COMMENTS := /C
  CPP_NO_LINENUM := /EP

  ifdef TCROOT
    TCROOT := $(shell $(CYGPATH) -ma '$(TCROOT)')

    ifeq ($(ARCH), x64)
      BINSUBDIR := /x86_x64
      LIBSUBDIR := /x64
      # should rename this one: toolchain bug
      MFCLIBSUBDIR := /amd64
    else
      BINSUBDIR :=
      LIBSUBDIR :=
      MFCLIBSUBDIR :=
    endif
    # xref bora/make/mk/defs-compilersWin32.mk
    ifeq ($(USE_VC71),1)
      WROOT_BASE := $(TCROOT)/win32/visualstudio-2003
      WROOT_VC := $(WROOT_BASE)/VC7
      WROOT_PSDK := $(WROOT_VC)/PlatformSDK
      WROOT_SDKTOOLS := $(WROOT_BASE)/Common7/Tools/Bin
      MFCDIR := atlmfc
    else
     ifeq ($(USE_VC60),1)
       WROOT_BASE := $(TCROOT)/win32/visual-studio-98
       WROOT_VC := $(WROOT_BASE)/vc98
       # no SDK, so missing Common/MSDev98/Bin for rc.exe, ml.exe!
       # Below we use the vc7 versions
       WROOT_PSDK := $(WROOT_VC)
       WROOT_SDKTOOLS := $(WROOT_VC)/Bin
       MFCDIR := MFC
     # windows vista sdk compiler is the default
     else
       WROOT_BASE := $(TCROOT)/win32/winsdk-6.1.6000
       WROOT_VC := $(WROOT_BASE)/VC
       WROOT_PSDK := $(WROOT_BASE)
       WROOT_SDKTOOLS := $(WROOT_PSDK)/Bin
       MFCDIR := atlmfc
     endif
    endif
    # we are not setting INCLUDE or LIB env vars, so we set all paths here
    MSINCLUDE := $(I)'$(shell $(CYGPATH) -wa "$(WROOT_PSDK)/Include")' \
                 $(I)'$(shell $(CYGPATH) -wa "$(WROOT_VC)/INCLUDE")' \
                 $(I)'$(shell $(CYGPATH) -wa "$(WROOT_VC)/$(MFCDIR)/include")'
    MSLIB     := $(L)'$(shell $(CYGPATH) -wa "$(WROOT_PSDK)/Lib$(LIBSUBDIR)")' \
                 $(L)'$(shell $(CYGPATH) -wa "$(WROOT_VC)/LIB$(LIBSUBDIR)")' \
                 $(L)'$(shell $(CYGPATH) -wa "$(WROOT_VC)/$(MFCDIR)/lib$(MFCLIBSUBDIR)")'
    # for open-source release we can't distribute ntdll.lib so point at it
    # note that we want the oldest ntdll.lib for maximum compatibility
    DDK_BASE := $(TCROOT)/win32/winddk-6000.16386
    NTDLL_LIBPATH_X86 := $(DDK_BASE)/lib/w2k/i386
    NTDLL_LIBPATH_X64 := $(DDK_BASE)/lib/wnet/amd64
    # we do not set PATH and rely on mspdb80.dll and mspdbsrv.exe being
    # in same dir as cl.exe.
    CC  := $(WROOT_VC)/Bin$(BINSUBDIR)/cl.exe /nologo
    CPP := $(CC)
    LD  := $(WROOT_VC)/Bin$(BINSUBDIR)/link.exe /nologo
    ifeq ($(ARCH), x64)
      AS := $(WROOT_VC)/Bin$(BINSUBDIR)/ml64.exe /nologo
    else
      AS := $(WROOT_VC)/Bin$(BINSUBDIR)/ml.exe /nologo
    endif
    MC := $(WROOT_SDKTOOLS)/mc.exe
    MT := $(WROOT_SDKTOOLS)/mt.exe
    ifeq ($(USE_VC60),1)
      # FIXME: ask toolchain people to add these; for now we use vc7 versions
      RC := $(TCROOT)/win32/visualstudio-2003/Common7/Tools/Bin/rc.exe
      ML := $(TCROOT)/win32/visualstudio-2003/VC/Bin/ml.exe
    else
      RC := $(WROOT_SDKTOOLS)/rc.exe
      ML := $(WROOT_VC)/Bin/ml.exe
    endif
    AR := $(WROOT_VC)/Bin$(BINSUBDIR)/lib.exe /nologo
    BIND := $(WROOT_SDKTOOLS)/bind.exe
    DUMPBIN := $(WROOT_VC)/Bin$(BINSUBDIR)/dumpbin.exe
  else
    ifndef BUILDTOOLS
      $(error compiler.mk requires TCROOT or BUILDTOOLS)
    endif
    ifeq ($(USE_VC60),1)
      # note that to build with vc6.0sp5 you'll need to re-instate the separate
      # use of cpp to handle vararg macros
      COMPILER := VC/6.0sp5
    else
      ifeq ($(USE_VC71),1)
        COMPILER := VC/7.1
      else
        # default to 8.0
        COMPILER := VC/8.0
      endif
    endif
    include $(BUILDTOOLS)/$(COMPILER)/Setup.mk
    MSINCLUDE := 
    MSLIB := 
    LD += /nologo
    AR := lib.exe /nologo
    CPP := $(CC)
  endif # TCROOT

#
# Linux
#
else
  # 
  # Build variables
  #
  # -iquote adds a dir to search path for "" only and not for <>
  # which is needed when we have conflicting names like link.h!
  ifeq ($(shell $(CC) -v --help 2>&1 | grep iquote),)
    # Fall back to -I.  This will only work ifdef HAVE_PROC_MAPS.
    I := -I
  else
    I := -iquote
  endif
  D := -D
  U := -U
  L := -L
  INCR    := -c
  OUT     := -o 
  EXEOUT  := -o 
  PREPROC := -E
  CPPOUT  := -o 
  CPP_SFX :=i
  CPP_KEEP_COMMENTS := -C
  CPP_NO_LINENUM := -P

  CC   	  := gcc
  CPP  	  := cpp
  LD   	  := ld
  AR   	  := ar
  SIZE 	  := size
  STRIP   := strip
  READELF := readelf

  # Better to use -dT when passing linker options through gcc, but ld
  # prior to 2.18 only supports -T.  Yet another thing we want ./configure
  # or cmake to check for.
  # FIXME: this is duplicated in api/samples/Makefile b/c it needs to be
  # a standalone Makefile
  ifeq ($(shell $(LD) -help | grep dT),)
    LD_SCRIPT_OPTION := -T
  else
    LD_SCRIPT_OPTION := -dT
  endif

  # We require gas >= 2.18.50 for --32, --64, and the new -msyntax=intel, etc.
  # Since this is pretty recent we include a copy (built vs as old a glibc
  # as was convenient)
  AS := $(DYNAMORIO_MAKE)/as-2.18.50
  ASMFLAGS := -mmnemonic=intel -msyntax=intel -mnaked-reg --noexecstack

  # Features
  ifeq ($(shell $(CC) -v --help 2>&1 | grep fvisibility=),)
    HAVE_FVISIBILITY := 0
  else
    HAVE_FVISIBILITY := 1
  endif

endif # win32
