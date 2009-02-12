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

###########################################################################
###########################################################################
###
### Top-level Makefile for building DynamoRIO
###
### Default build is all modules for one arch.
###
### The "package" target builds a release package.
### The build directory is NOT automatically updated:
### the user should invoke as "make clean zip" (or just "make")
### in order to update an existing build directory.
###
### WARNING: do NOT include any shell-invoking values prior to
### the currently-under-cygwin test below!

.PHONY: default package dynamorio-onearch clean dynamorio-package clean-package

default: dynamorio-onearch

package: clean-package dynamorio-package

# Only Windows shoud have the OS env var set
ifeq ($(OS),)
  MACHINE := linux
else
  MACHINE := win32
endif

ifndef ARCH
# default is x86 instead of x64
ARCH = x86
endif

# Makes all but samples for one arch
# FIXME: figure out strategy for Windows w/o local cygwin: vs batch file
# usage below for package target
# FIXME: eliminate makezip.sh and move all its functionality here?
# that would address PR 371733: makezip.sh should propagate errors 
# FIXME PR 207890: re-enable USEDEP for core
# FIXME: build INTERNAL=1 instead?
# FIXME PR 209902: get core/Makefile supporting -jN and then parallelize
# this target
dynamorio-onearch:
	$(MAKE) -C core ARCH=$(ARCH) DEBUG=1 INTERNAL=0 EXTERNAL_INJECTOR=1 
	$(MAKE) -C core ARCH=$(ARCH) DEBUG=0 INTERNAL=0 EXTERNAL_INJECTOR=1 install
	$(MAKE) -C api/docs
ifeq ($(MACHINE), win32)
	$(MAKE) -C libutil
	$(MAKE) -C tools EXTERNAL_DRVIEW=1
endif

clean:
# docs first to avoid error about no headers
	$(MAKE) -C api/docs clean
	$(MAKE) -C core clear
ifeq ($(MACHINE), win32)
	$(MAKE) -C libutil clean
	$(MAKE) -C tools clean
endif

# build team provides these:
# OBJDIR               = ignored
# BUILD_ROOT           = pass to makezip.sh
# PUBLISH_DIR          = pass to makezip.sh
# BUILD_NUMBER         = -DUNIQUE_BUILD_NUMBER via makezip.sh
# PRODUCT_BUILD_NUMBER = -DBUILD_NUMBER via makezip.sh

# we no longer support viper
BUILD_TYPE := vmap

BUILD_ROOT := $(CURDIR)/zipbuild

# used if invoker doesn't specify
NO_BUILD_NUMBER := 16

# our BUILD_NUMBER goes into the windows x.y.z.w version and so must
# be <64K, which is vmware's PRODUCT_BUILD_NUMBER.  but we're supposed
# to record the potentially-larger unique BUILD_NUMBER somewhere as well,
# requiring us to pass in two build numbers to makezip.sh.
ifneq ($(BUILD_NUMBER),)
  UNIQUE_BUILD_NUMBER := $(BUILD_NUMBER)
else
  UNIQUE_BUILD_NUMBER := $(NO_BUILD_NUMBER)
endif

# part-of-version build number: must be <64K
ifneq ($(PRODUCT_BUILD_NUMBER),)
  VER_BUILD_NUMBER := $(PRODUCT_BUILD_NUMBER)
else
  VER_BUILD_NUMBER := $(NO_BUILD_NUMBER)
endif

# keep in sync with CURRENT_API_VERSION in core/x86/instrument.h
# and _USES_DR_VERSION_ in dr_api.h (PR 250952) and compatibility
# check in core/x86/instrument.c (OLDEST_COMPATIBLE_VERSION, etc.)
# and api/docs/footer.html
# and tools/DRgui/DynamoRIO.rc
VERSION_NUMBER := 1.3.1

MAKEZIP_ARGS := $(BUILD_TYPE) $(BUILD_ROOT) $(VER_BUILD_NUMBER) \
                $(UNIQUE_BUILD_NUMBER) $(VERSION_NUMBER) $(PUBLISH_DIR)

ifndef DYNAMORIO_TOOLS
  DYNAMORIO_TOOLS := tools
endif

##################################################
ifeq ($(MACHINE), win32)

  # To support building with either a local cygwin 
  # or a toolchain cygwin, we do NOT
  # invoke any commands that require sh or bash to avoid cygwin
  # conflicts before we've identified whether we're using local
  # cygwin or need to invoke our batch file to set up
  # for toolchain cygwin.
  #
  # The standard VMware Windows build uses a non-cygwin make.exe
  # from toolchain, with the path set such that it finds
  # toolchain/win32/bin/sh.exe as its shell.  We do NOT want
  # to execute that shell and want to use our toolchain cygwin
  # instead.  But, for invocations on a machine that
  # has a local cygwin, we want to use that.  Fundamentally
  # we want to know whether there is a local cygwin install
  # that we can use, not whether the make.exe is cygwin or not.
  # That's difficult to check w/o invoking utility programs.
  # Next best would be to identify whether any other running
  # processes are cygwin, since those can conflict with toolchain
  # cygwin.  That's also hard to do, so we settle for checking
  # whether make.exe was invoked from a cygwin shell.
  # Unfortunately $OSTYPE and $MACHTYPE are not exported, so
  # we instead check whether cmd's %ComSpec% is set (it is even if
  # invoking make.exe from the Run box).
  # Note that we can't check whether PATH uses ; or : since
  # toolchain make.exe converts a Unix PATH to Windows format.

  ifeq ($(ComSpec),)
    LOCAL_CYGWIN := 1
  else
    LOCAL_CYGWIN := 0
  endif

  ifeq ($(LOCAL_CYGWIN),1)
    # Currently under cygwin: keep using that cygwin.
    # We assume all utilities are on the PATH.
dynamorio-package:
	$(DYNAMORIO_TOOLS)/makezip.sh $(MAKEZIP_ARGS)

clean-package:
	$(RM) -rf $(BUILD_ROOT) $(PUBLISH_DIR)

  else

    # No local cygwin: set up for toolchain cygwin.
    SHELL   := cmd.exe
    ifndef DYNAMORIO_BATCH
      DYNAMORIO_BATCH := batch
    endif

dynamorio-package:
	$(DYNAMORIO_BATCH)\makezip.bat $(MAKEZIP_ARGS)

    # For some reason I can't get direct invocation of rd to
    # work with toolchain make: it messes up its CreateProcess
    # invocation.
clean-package:
	-$(DYNAMORIO_BATCH)\rmtree.bat $(subst /,\,$(BUILD_ROOT))
	-$(DYNAMORIO_BATCH)\rmtree.bat $(subst /,\,$(PUBLISH_DIR))

  endif

##################################################
else

  ifndef DYNAMORIO_MAKE
    DYNAMORIO_MAKE := make
  endif
  include $(DYNAMORIO_MAKE)/compiler.mk

dynamorio-package:
	$(DYNAMORIO_TOOLS)/makezip.sh $(MAKEZIP_ARGS)

clean-package:
	$(RM) -rf $(BUILD_ROOT) $(PUBLISH_DIR)

endif
##################################################
