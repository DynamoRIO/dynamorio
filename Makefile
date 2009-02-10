# **********************************************************
# Copyright 2008 VMware, Inc.  All rights reserved. -- VMware Confidential
# **********************************************************

###########################################################################
###########################################################################
###
### Top-level Makefile for building DynamoRIO
###
### Default build is a GoVirtual release package.
###
### The build directory is NOT automatically updated:
### the user should invoke as "make clean zip" (or just "make")
### in order to update an existing build directory.
###
### WARNING: do NOT include any shell-invoking values prior to
### the currently-under-cygwin test below!

.PHONY: default zip clean dynamorio-all

default: clean dynamorio-all

# Only Windows shoud have the OS env var set
ifeq ($(OS),)
  MACHINE := linux
else
  MACHINE := win32
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
zip:
	$(DYNAMORIO_TOOLS)/makezip.sh $(MAKEZIP_ARGS)

clean:
	$(RM) -rf $(BUILD_ROOT) $(PUBLISH_DIR)

  else

    # No local cygwin: set up for toolchain cygwin.
    SHELL   := cmd.exe
    ifndef DYNAMORIO_BATCH
      DYNAMORIO_BATCH := batch
    endif

zip:
	$(DYNAMORIO_BATCH)\makezip.bat $(MAKEZIP_ARGS)

    # For some reason I can't get direct invocation of rd to
    # work with toolchain make: it messes up its CreateProcess
    # invocation.
clean:
	-$(DYNAMORIO_BATCH)\rmtree.bat $(subst /,\,$(BUILD_ROOT))
	-$(DYNAMORIO_BATCH)\rmtree.bat $(subst /,\,$(PUBLISH_DIR))

  endif

##################################################
else

  ifndef DYNAMORIO_MAKE
    DYNAMORIO_MAKE := make
  endif
  # we need the x64 version of strip in order to operate on x64 libs
  ARCH := x64
  include $(DYNAMORIO_MAKE)/compiler.mk

  # Set up for scripts like makezip.sh and for auto-generated
  # api/docs/latex/Makefile to use toolchain utils.
  # For scripts we could set env vars whenever make invokes a script
  #   TAR=$(TAR) PERL=$(PERL) ... <script>
  # and then have the script explicitly invoke $TAR, but it's less work
  # to set up the path (esp. for auto-gen latex/Makefile).
  #
  # PR 340793: this older doxygen doesn't quite match newer local
  # doxygen, but it's close enough
  #
  PATH := $(BINUTILS)/$(BINPREFIX)-linux/bin:$(COREUTILS):$(TC_AWK):$(TC_DIFF):$(TC_GREP):$(TC_PERL):$(TC_SED):$(TC_XARGS):$(TC_DOXYGEN):$(TC_MOGRIFY):$(TC_FIG2DEV):$(TC_GS):$(TC_TETEX):$(TC_TAR):$(TC_GZIP):$(TC_ZIP):$(TC_MAKE):$(TC_FIND)

zip:
	$(DYNAMORIO_TOOLS)/makezip.sh $(MAKEZIP_ARGS)

clean:
	$(RM) -rf $(BUILD_ROOT) $(PUBLISH_DIR)

endif
##################################################
dynamorio-all: zip
