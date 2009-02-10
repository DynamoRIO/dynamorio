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

# use toolchain or buildtools if available
# see http://wiki.eng.vmware.com/Build/Toolchain/Access for toolchain setup
#   view = //toolchain/main/win32/winsdk-6.1.6000/...
#   server = perforce-toolchain.eng.vmware.com:1666
# FIXME: use toolchain on linux -- PR 234917

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
# toolchain cygwin.  For Linux we swap in toolchain versions,
# if available, below.
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
# the following are for building our docs
# PR 340793: toolchain cygwin doxygen is version 1.5.1, which is
# missing some features vs 1.5.3+: no defines in Globals list, e.g.
DOXYGEN  := doxygen
MOGRIFY  := mogrify
FIG2DEV  := fig2dev
# for tools/DRinstall/
OD       := od

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
  # -Iquote adds a dir to search path for "" only and not for <>
  # which is needed when we have conflicting names like link.h!
  I := -iquote
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

  # We hardcode to use a gcc in /build/toolchain. Users need 
  # to mount /build/toolchain or provide a local copy of the 
  # neccessary toolchain modules in /build/toolchain. Note that
  # gcc is configured to look for its headers and libraries in
  # /build/toolchain, so we do not need to specify any of those
  # paths on the gcc command line.
  ifeq ($(USE_GCC_3.4.6),1)
    # Some components (spec tests) need older compiler version.
    # For this we need :
    #   /build/toolchain/lin32/gcc-3.4.6
    #   /build/toolchain/lin32/binutils-2.16.1-vt
    #   /build/toolchain/lin32/glibc-2.2.5-44  (note - same as for 4.1.2-2)
    GCCDIR := /build/toolchain/lin32/gcc-3.4.6
    BINUTILS := /build/toolchain/lin32/binutils-2.16.1-vt
  else
    # For Linux, we'll default to the same compiler as the monitor.
    # (https://wiki.eng.vmware.com/Build/Toolchain/Compilers)
    # We therefore need the following:
    #   /build/toolchain/lin32/gcc-4.1.2-2
    #   /build/toolchain/lin32/binutils-2.17.50.0.15
    #   /build/toolchain/lin32/glibc-2.2.5-44
    #   /build/toolchain/lin64/glibc-2.3.2-95.44
    GCCDIR := /build/toolchain/lin32/gcc-4.1.2-2
    BINUTILS := /build/toolchain/lin32/binutils-2.17.50.0.15
  endif

  ifeq ($(ARCH), x64)
    BINPREFIX := x86_64
  else
    BINPREFIX := i686
  endif

  CC  := $(GCCDIR)/bin/$(BINPREFIX)-linux-gcc
  CPP := $(GCCDIR)/bin/$(BINPREFIX)-linux-cpp
  LD  := $(GCCDIR)/$(BINPREFIX)-linux/bin/ld
  AR  := $(GCCDIR)/$(BINPREFIX)-linux/bin/ar
  SIZE := $(BINUTILS)/bin/$(BINPREFIX)-linux-size
  STRINGS := $(BINUTILS)/bin/$(BINPREFIX)-linux-strings
  STRIP := $(BINUTILS)/bin/$(BINPREFIX)-linux-strip
  READELF := $(BINUTILS)/bin/$(BINPREFIX)-linux-readelf

  # We require gas >= 2.18.50 for --32, --64, and the new -msyntax=intel, etc.
  BINUTILS_2_18 := /build/toolchain/lin32/binutils-2.18.50.0.9
  AS := $(BINUTILS_2_18)/bin/$(BINPREFIX)-linux-as
  ASMFLAGS := -mmnemonic=intel -msyntax=intel -mnaked-reg --noexecstack

  ###################################
  #
  # For utility apps we allow local versions for easier building by
  # remote developers.  But, we have toolchain versions of everything for
  # consistent, reproducible official builds.

  # Since our utils are all present in lin32 and we're not linking w/
  # any libraries from toolchain we don't bother to use
  # the lin64 versions on an x64 machine.

  COREUTILS := /build/toolchain/lin32/coreutils-5.97/bin
  ifneq ($(wildcard $(COREUTILS)),)
    CAT      := $(COREUTILS)/cat
    CHMOD    := $(COREUTILS)/chmod
    CP       := $(COREUTILS)/cp -f
    CUT      := $(COREUTILS)/cut
    DATE     := $(COREUTILS)/date
    ECHO     := $(COREUTILS)/echo
    HEAD     := $(COREUTILS)/head
    LS       := $(COREUTILS)/ls
    MKDIR    := $(COREUTILS)/mkdir
    MV       := $(COREUTILS)/mv -f
    RM       := $(COREUTILS)/rm -f
    TAIL     := $(COREUTILS)/tail
    TEE      := $(COREUTILS)/tee
    WC       := $(COREUTILS)/wc
    OD       := $(COREUTILS)/od
  endif

  TC_AWK     := /build/toolchain/lin32/gawk-3.1.5/bin
  ifneq ($(wildcard $(TC_AWK)),)
    AWK      := $(TC_AWK)/awk
  endif
  TC_DIFF    := /build/toolchain/lin32/diffutils-2.8/bin
  ifneq ($(wildcard $(TC_DIFF)),)
    DIFF     := $(TC_DIFF)/diff
  endif
  TC_GREP    := /build/toolchain/lin32/grep-2.5.1a/bin
  ifneq ($(wildcard $(TC_GREP)),)
    GREP     := $(TC_GREP)/grep
  endif
  TC_PERL    := /build/toolchain/lin32/perl-5.8.8/bin
  ifneq ($(wildcard $(TC_PERL)),)
    PERL     := $(TC_PERL)/perl
  endif
  TC_SED     := /build/toolchain/lin32/sed-4.1.5/bin
  ifneq ($(wildcard $(TC_SED)),)
    SED      := $(TC_SED)/sed
  endif
  TC_XARGS   := /build/toolchain/lin32/findutils-4.2.27/bin
  ifneq ($(wildcard $(TC_XARGS)),)
    XARGS    := $(TC_XARGS)/xargs
  endif
  # PR 340793: this older doxygen doesn't quite match 1.5.6:
  # seems to omit globals starting w/ 't', maybe other harder-to-spot
  # errors, but so far minor enough that we ignore them.
  TC_DOXYGEN := /build/toolchain/lin32/doxygen-1.5.3/bin
  ifneq ($(wildcard $(TC_DOXYGEN)),)
    DOXYGEN  := $(TC_DOXYGEN)/doxygen
  endif
  TC_MOGRIFY := /build/toolchain/lin32/ImageMagick-6.4.1-5/bin
  ifneq ($(wildcard $(TC_MOGRIFY)),)
    MOGRIFY  := $(TC_MOGRIFY)/mogrify
  endif
  TC_FIG2DEV := /build/toolchain/lin32/transfig-3.2.5/bin
  ifneq ($(wildcard $(TC_FIG2DEV)),)
    FIG2DEV  := $(TC_FIG2DEV)/fig2dev
  endif

  #################
  # Utilities we access by setting PATH:

  # gs is used by fig2dev
  TC_GS      := /build/toolchain/lin32/ghostscript-8.62/bin
  # pdflatex and makeindex used by auto-generated api/docs/latex/Makefile
  TC_TETEX   := /build/toolchain/lin32/tetex-3.0/bin

  # For makezip.sh util usage
  TC_TAR     := /build/toolchain/lin32/tar-1.15.1/bin
  # gzip is invoked by tar -z
  TC_GZIP    := /build/toolchain/lin32/gzip-1.2.4a/bin
  TC_ZIP     := /build/toolchain/lin32/zip-2.32/bin
  TC_MAKE    := /build/toolchain/lin32/make-3.81/bin
  TC_FIND    := /build/toolchain/lin32/findutils-4.2.27/bin

endif # win32
