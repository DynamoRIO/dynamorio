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

#
# Paths
#
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
# Issue 20: cross-arch execve depends on these being distinct and not
# subsets of each other (e.g., not "lib" and "lib64") and on the
# release package using these same names.
INSTALL_LIB_X64    := lib64
INSTALL_LIB_X86    := lib32
ifeq ($(ARCH), x64)
  INSTALL_LIB_BASE :=$(EXPORTS_BASE)/$(INSTALL_LIB_X64)
  INSTALL_BIN      :=$(INSTALL_BIN_BASE)/bin64
else
  INSTALL_LIB_BASE :=$(EXPORTS_BASE)/$(INSTALL_LIB_X86)
  INSTALL_BIN      :=$(INSTALL_BIN_BASE)/bin32
endif
INSTALL_INCLUDE    :=$(EXPORTS_BASE)/include
INSTALL_DOCS       :=$(EXPORTS_BASE)/docs
# src/ or core/
CORE_DIRNAME :=core


###########################################################################
###########################################################################
#
# Windows
#
ifeq ($(MACHINE), win32)
  ifndef SDKROOT
    $(error SDKROOT must point at a Windows SDK or Visual Studio installation)
  endif
  ifndef DDKROOT
    $(error DDKROOT must point at a Windows DDK or WDK installation)
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

  # FIXME: all these shell invocations slow us down: require users to
  # set paths in mixed fashion?
  # i#19 will also solve if a pre-make step records all the paths.
  SDKROOT := $(shell $(CYGPATH) -ma '$(SDKROOT)')
  DDKROOT := $(shell $(CYGPATH) -ma '$(DDKROOT)')

  # We can build dynamorio.dll from just the DDK, but we can't build
  # drinject.exe (need imagehlp.lib) or libutil's static library (need lib.exe)
  # or DRgui.  So we use the SDK/VS primarily, and use the DDK only for
  # ntdll.lib.  We need cl 14, and thus the Vista SDK (6.1.6000) or Visual Studio 2005.
  # For the DDK, either 2003 DDK (3790.1830) or Vista WDK (6000) with x64 libraries.

  ifndef SDKROOT
    # Here to show how to build core (minus injector) from just DDK.
    # We need cl 14 for C99 vararg macros which means the
    # Vista Windows Driver Kit (WDK) 6000 or later.
    # Note that we want the oldest lib but the newest headers.
    DDK_BIN_BASE := $(DDKROOT)/bin/x86
    ifeq ($(ARCH), x64)
      DDK_BIN_ARCH := $(DDKROOT)/bin/x86/amd64
      DDK_LIB := $(DDKROOT)/lib/wnet/amd64
      DDK_LIB_CRT := $(DDKROOT)/lib/crt/amd64
    else
      DDK_BIN_ARCH := $(DDKROOT)/bin/x86/x86
      DDK_LIB := $(DDKROOT)/lib/w2k/i386
      DDK_LIB_CRT := $(DDKROOT)/lib/crt/i386
    endif
    DDK_INC_API := $(DDKROOT)/inc/api
    DDK_INC_CRT := $(DDKROOT)/inc/crt
    # we are not setting INCLUDE or LIB env vars, so we set all paths here
    MSINCLUDE := $(I)'$(shell $(CYGPATH) -wa "$(DDK_INC_API)")' \
                 $(I)'$(shell $(CYGPATH) -wa "$(DDK_INC_CRT)")'
    # no imagehlp.lib => can't build drinject.exe
    MSLIB     := $(L)'$(shell $(CYGPATH) -wa "$(DDK_LIB)")' \
                 $(L)'$(shell $(CYGPATH) -wa "$(DDK_LIB_CRT)")'
    NTDLL_LIBPATH_X86 := $(DDK_LIB)
    NTDLL_LIBPATH_X64 := $(DDK_LIB)
    # we do not set PATH and rely on mspdb80.dll and mspdbsrv.exe being
    # in same dir as cl.exe.
    CC  := $(DDK_BIN_ARCH)/cl.exe /nologo
    CPP := $(CC)
    LD  := $(DDK_BIN_ARCH)/link.exe /nologo
    ifeq ($(ARCH), x64)
      AS := $(DDK_BIN_ARCH)/ml64.exe /nologo
    else
      AS := $(DDK_BIN_BASE)/ml.exe /nologo
    endif
    MC := $(DDK_BIN_BASE)/mc.exe
    # no MT but currently not used
    RC := $(DDK_BIN_BASE)/rc.exe
    ML := $(DDK_BIN_BASE)/ml.exe
    # no AR = lib.exe => can't build libutil/
    # no BIND => can't run certain suite/tests/
    # no DUMPBIN but only used for core/Makefile dll stats
  else

    ifeq ($(ARCH), x64)
      ifeq ($(wildcard $(SDKROOT)/VC/Bin/x64),$(SDKROOT)/VC/Bin/x64)
        # Vista SDK
        # Annoyingly it's missing ml64.exe and MFC.  We get ml64.exe
        # from the DDK (meaning we need the Vista WDK) but neither has
        # MFC libraries or resources so we can't build DRgui.
        # FIXME: support pointing at an older Visual Studio for DRgui.
        BINSUBDIR := /x64
        LIBSUBDIR := /x64
      else
       ifeq ($(wildcard $(SDKROOT)/VC/Bin/x86_64),$(SDKROOT)/VC/Bin/x86_x64)
        # VMware toolchain SDK + VS2005 mix
        BINSUBDIR := /x86_x64
        LIBSUBDIR := /x64
       else
        ifeq ($(wildcard $(SDKROOT)/VC/Bin/amd64),$(SDKROOT)/VC/Bin/amd64)
        # Visual Studio 2005.  bin/amd64 has mspdb* and msobj80.dll already,
        # so we use it in preference to x86_amd64.
        BINSUBDIR := /amd64
        LIBSUBDIR := /amd64
        else
          $(error Invalid SDKROOT: no x64 binaries found!)
        endif
       endif
      endif
      MFCLIBSUBDIR := /amd64
    else
      BINSUBDIR :=
      LIBSUBDIR :=
      MFCLIBSUBDIR :=
    endif

    ifeq ($(wildcard $(SDKROOT)/VC/PlatformSDK),$(SDKROOT)/VC/PlatformSDK)
      # Visual Studio 2005
      WROOT_BASE := $(SDKROOT)
      WROOT_VC := $(WROOT_BASE)/VC
      WROOT_PSDK := $(WROOT_VC)/PlatformSDK
      WROOT_SDKTOOLS := $(WROOT_BASE)/Common7/Tools/Bin
      MFCDIR := atlmfc
      ifeq ($(wildcard $(WROOT_VC)/Bin/mspdb80.dll),)
        # FIXME: too ugly to make users do?  Should we try to set PATH instead?
        $(error Please copy $(WROOT_BASE)/Common7/IDE/mspdb* and $(WROOT_BASE)/Common7/IDE/msobj80.dll to $(WROOT_VC)/Bin)
      endif
    else
      # Windows Vista SDK or VMware toolchain SDK + VS2005 mix
      WROOT_BASE := $(SDKROOT)
      WROOT_VC := $(WROOT_BASE)/VC
      WROOT_PSDK := $(WROOT_BASE)
      WROOT_SDKTOOLS := $(WROOT_PSDK)/Bin
      MFCDIR := atlmfc
    endif

    # When we build the official release version of DRgui (and DRinstall,
    # now deprecated) we use MFC and we want to run on older Windows (NT if
    # possible), which means using older Visual Studio, so we point at it
    # separately via VSROOT.  This may also be necessary when using Vista
    # SDK, which is missing MFC.
    ifeq ($(USE_VC71),1)
      ifdef VSROOT
        # Visual Studio 2003
        WROOT_BASE := $(shell $(CYGPATH) -ma '$(VSROOT)')
        WROOT_VC := $(WROOT_BASE)/VC7
        WROOT_PSDK := $(WROOT_VC)/PlatformSDK
        WROOT_SDKTOOLS := $(WROOT_BASE)/Common7/Tools/Bin
        MFCDIR := atlmfc
      else
        USE_VC71 := 0
      endif
    else
     ifeq ($(USE_VC60),1)
      ifdef VSROOT
        # Visual Studio 98
        WROOT_BASE := $(shell $(CYGPATH) -ma '$(VSROOT)')
        WROOT_VC := $(WROOT_BASE)/vc98
        WROOT_PSDK := $(WROOT_VC)
        WROOT_SDKTOOLS := $(WROOT_VC)/Bin
        MFCDIR := MFC
        ifeq ($(wildcard $(WROOT_VC)/Bin/mspdb60.dll),)
          # FIXME: too ugly to make users do?  Should we try to set PATH instead?
          $(error Please copy $(VSROOT)/Common/MSDev98/Bin/mspdb60.dll to $(WROOT_VC)/Bin)
        endif
      else
        ifeq ($(wildcard $(WROOT_VC)/$(MFCDIR)),)
          $(error To build DRgui you need to set VSROOT, or change SDKROOT to Visual Studio 2005)
        endif
        USE_VC60 := 0
      endif
     endif
    endif

    # we are not setting INCLUDE or LIB env vars, so we set all paths here
    MSINCLUDE := $(I)'$(shell $(CYGPATH) -wa "$(WROOT_PSDK)/Include")' \
                 $(I)'$(shell $(CYGPATH) -wa "$(WROOT_VC)/INCLUDE")' \
                 $(I)'$(shell $(CYGPATH) -wa "$(WROOT_VC)/$(MFCDIR)/include")'
    MSLIB     := $(L)'$(shell $(CYGPATH) -wa "$(WROOT_PSDK)/Lib$(LIBSUBDIR)")' \
                 $(L)'$(shell $(CYGPATH) -wa "$(WROOT_VC)/LIB$(LIBSUBDIR)")' \
                 $(L)'$(shell $(CYGPATH) -wa "$(WROOT_VC)/$(MFCDIR)/lib$(MFCLIBSUBDIR)")'

    # For open-source release we can't distribute ntdll.lib so point at it.
    # Note that we want the oldest ntdll.lib for maximum compatibility.
    # The 2003 DDK (3790.1830) or Vista WDK (6000) both have these paths:
    NTDLL_LIBPATH_X86 := $(DDKROOT)/lib/w2k/i386
    NTDLL_LIBPATH_X64 := $(DDKROOT)/lib/wnet/amd64

    # We do not set PATH: instead we rely on mspdb80.dll and mspdbsrv.exe being
    # in same dir as cl.exe.
    CC  := $(WROOT_VC)/Bin$(BINSUBDIR)/cl.exe /nologo
    CPP := $(CC)
    LD  := $(WROOT_VC)/Bin$(BINSUBDIR)/link.exe /nologo
    ifeq ($(ARCH), x64)
      AS := $(WROOT_VC)/Bin$(BINSUBDIR)/ml64.exe /nologo
      # Vista SDK is missing ml64.exe
      ifeq ($(wildcard $(AS)),)
        # FIXME: share w/ ifndef SDKROOT above
        AS := $(DDKROOT)/bin/x86/amd64/ml64.exe /nologo
      endif
    else
      AS := $(WROOT_VC)/Bin$(BINSUBDIR)/ml.exe /nologo
    endif
    MC := $(WROOT_SDKTOOLS)/mc.exe
    MT := $(WROOT_SDKTOOLS)/mt.exe
    ifeq ($(USE_VC60),1)
      # Missing from VMware toolchain VS98 -- though a local install would have it,
      # in Common/MSDev98/Bin
      RC := $(DDKROOT)/bin/x86/rc.exe
      # We assume ML is not needed
    else
      RC := $(WROOT_SDKTOOLS)/rc.exe
      ML := $(WROOT_VC)/Bin/ml.exe
    endif
    AR := $(WROOT_VC)/Bin$(BINSUBDIR)/lib.exe /nologo
    BIND := $(WROOT_SDKTOOLS)/bind.exe
    DUMPBIN := $(WROOT_VC)/Bin$(BINSUBDIR)/dumpbin.exe

    # Vista SDK is missing MFC.  This will supply header files so we can
    # build libutil (core/win32/resources.rc includes winres.h) but
    # we'll need VSROOT (see above) to build DRgui.
    ifeq ($(wildcard $(WROOT_VC)/$(MFCDIR)),)
      MSINCLUDE += $(I)'$(shell $(CYGPATH) -wa "$(DDKROOT)/inc/mfc42")'
      ifeq ($(ARCH), x64)
        DDKMFCLIBDIR := amd64
      else
        DDKMFCLIBDIR := i386
      endif
      MSLIB += $(L)'$(shell $(CYGPATH) -wa "$(DDKROOT)/lib/mfc/$(DDKMFCLIBDIR)")'
    endif

  endif

###########################################################################
###########################################################################
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
