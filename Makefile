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
### Default build is all modules for one arch, x86 by default.
### Specify ARCH=x64 for 64-bit.
###
### The "package" target builds a release package.
###
### WARNING: do NOT include any shell-invoking values prior to
### the currently-under-cygwin test below!

.PHONY: default all package dynamorio-onearch clean dynamorio-package clean-package \
	diffdir diff diffcheck diffclean notesclean reviewclean \
        diffnotes diffprep review

default: dynamorio-onearch
all: dynamorio-onearch

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

clean: clean-package
# docs first to avoid error about no headers
	$(MAKE) -C api/docs clean
	$(MAKE) -C core clear
ifeq ($(MACHINE), win32)
	$(MAKE) -C libutil clean
	$(MAKE) -C tools clean
endif

###########################################################################
###########################################################################
#
# building a release package

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

    ifndef DYNAMORIO_MAKE
      DYNAMORIO_MAKE := make
    endif
    include $(DYNAMORIO_MAKE)/compiler.mk

dynamorio-package:
	$(DYNAMORIO_TOOLS)/makezip.sh $(MAKEZIP_ARGS)

clean-package:
	$(RM) -r $(BUILD_ROOT) $(PUBLISH_DIR)

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
	$(RM) -r $(BUILD_ROOT) $(PUBLISH_DIR)

endif
##################################################


# Processing for custom builds
# FIXME: this has shell-invoking vars: ruins under-cygwin check:
# but we may not need that check anymore since we will probably
# have to require devs to have cygwin on their machines.
include $(DYNAMORIO_MAKE)/custom_build.mk


###########################################################################
###########################################################################
# code review automation
#
# see the wiki for full discussion of our code review process.
# to summarize: reviewer commits to /reviews/<user>/<year>/i###-descr.diff
#   with a commit log that auto-opens an Issue
#
# usage: prepare a notes file with comments for the reviewer after a line
# beginning "toreview:", and point to it w/ NOTES= (deafult is ./diff.notes)
#
# example:
#   make USER=derek.bruening CUR_TREE=i4-make-review REVIEWER=qin.zhao review
# use the "diffprep" target to view prior to committing
# use "reviewclean" to abort

ifndef DYNAMORIO_REVIEWS
  DYNAMORIO_REVIEWS := ../reviews
endif
YEAR := $(shell $(DATE) +%Y)
DIFF_DIR := $(DYNAMORIO_REVIEWS)/$(USER)/$(YEAR)

# name is picked by current tree name as set above

# default for notes location
# these notes become the commit log for the diff
NOTES := diff.notes

# specify this if want to add case number or something
DIFF_PREFIX := 
DIFF_NAME := $(DIFF_PREFIX)$(CUR_TREE)
DIFF_DIFF := $(DIFF_DIR)/$(DIFF_NAME).diff
DIFF_NOTES := $(DIFF_DIR)/$(DIFF_NAME).notes
# will have machine name appended to it:
REGRESSION := $(DIFF_DIR)/$(DIFF_NAME).regr

REVIEWER := 
# only allow reviewers in the dev list
REVIEWER_CHECK := $(filter ${REVIEWER},${DEVELOPERS})

# We want context diffs with procedure names for better readability
# svn diff does show new files for us but we pass -N just in case
DIFF_OPTIONS := --diff-cmd diff -x "-c -p -N"

diffdir:
	@(cd $(DYNAMORIO_REVIEWS); \
	  if ! test -e $(USER); then $(SVN) mkdir $(USER); fi; \
	  if ! test -e $(USER)/$(YEAR); then $(SVN) mkdir $(USER)/$(YEAR); fi)

# We commit the diff to review/ using special syntax to auto-generate
# an Issue that covers performing the review
$(DIFF_DIFF): diffdir $(C_SRCS) Makefile 
	@(if ! test -e $@; then cd $(DYNAMORIO_REVIEWS); touch $@; $(SVN) add $@; fi)
	$(SVN) diff $(DIFF_OPTIONS) > $@

diffclean: 
	-cd $(DYNAMORIO_REVIEWS); $(SVN) revert $(DIFF_DIFF)

notesclean: 
	-cd $(DYNAMORIO_REVIEWS); $(SVN) revert $(DIFF_NOTES)

reviewclean: diffclean notesclean

# inline comments for a regular patch diff should be the norm for now
# FIXME: we should also produce web diffs for quick reviews
# FIXME: xref discussion on using Google Code's post-commit review options

diff: $(DIFF_DIFF)

diffnotes: $(DIFF_NOTES)

# anything after the line "toreview:" from $(NOTES) is added to the commit log
$(DIFF_NOTES): $(NOTES) $(DIFF_DIFF)
	@(if ! test -e $@; then cd $(DYNAMORIO_REVIEWS); touch $@; $(SVN) add $@; fi)
	@$(ECHO) "new review:" > $@
	@$(ECHO) "owner: $(REVIEWER)" >> $@
	@$(ECHO) "summary: $(USER)/$(YEAR)/$(DIFF_NAME)" >> $@
	@$(GREP) -q '^toreview' $(NOTES) || ($(ECHO) "missing toreview marker"; false)
	@$(SED) '1,/^toreview:$$/d' < $(NOTES) >> $@
	@$(ECHO) -e "\nstats:" >> $@
	@LINES=`$(WC) -l $(DIFF_DIFF)|$(CUT) -f 1 -d ' '`; $(ECHO) $$LINES diff lines >> $@
	@-diffstat -f 1 $(DIFF_DIFF) >> $@
	@$(ECHO) created $@

# FIXME - eventually depend on "runregression" and check its output for failed tests
diffcheck: ${DIFF_DIFF}
# runregression checks for make ugly failures this way, FIXME - we could use
# exit $$[1-$$?] after each of the grep rules in ugly: and just use a dependency here
# instead, however we'd have to stop using - to ignore failures in ugly: which would
# make it less usefull 
	$(MAKE) -C core -s ugly | $(GREP) -v '^$(UGLY_RULE_HEADER)'; exit $$[1-$$?]

diffprep: $(DIFF_DIFF) $(DIFF_NOTES)
	@(if [ -z $(REVIEWER) ]; then $(ECHO) "must set REVIEWER="; false; fi)
	@(if [ -z $(REVIEWER_CHECK) ]; then $(ECHO) "must set REVIEWER to one of $(DEVELOPERS)"; false; fi)
# we don't do a full diffcheck since it's ok to send incremental unfinished diffs
	-$(MAKE) -C core -s ugly
# FIXME: eventually require regression results before sending out diff?
	@(r=`$(ECHO) ${REGRESSION}*`; if test -e $${r%% *}; then $(CHMOD) g+r ${REGRESSION}*; scp -q ${REGRESSION}* $(REMOTE_DIR); else $(ECHO) "WARNING: No regression results found!"; fi)

review: diffprep
	@(cd $(DYNAMORIO_REVIEWS); $(SVN) commit --force-log -F $(DIFF_NOTES))


##################################################
##################################################
# Support for running regression suite 
#
# We update the target's tools, libutil, and suite dirs from the local copy, allowing
# testing of changes to other components than just core/.
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
RUNREG_TREE=${RUNREG_BASE}/${CUR_TREE}
RUNREG_SRC=${RUNREG_TREE}/${CORE_DIRNAME}
RUNREG_MAKE=${RUNREG_TREE}/make
RUNREG_TOOLS=${RUNREG_TREE}/tools
RUNREG_LIBUTIL=${RUNREG_TREE}/libutil
RUNREG_SUITE=${RUNREG_TREE}/suite
RUNREG_API=${RUNREG_TREE}/api
ifndef RUNREG_USER
  RUNREG_USER := ${USER}
endif
ifeq (${RUNREG_HOST}, localhost)
RUNREG_LOC=
RUNREG_CMD=$(SHELL) -c
RUNREG_SCRIPT=$(PERL) runregression
else
RUNREG_LOC=${RUNREG_USER}@${RUNREG_HOST}:
RUNREG_CMD=ssh ${RUNREG_USER}@${RUNREG_HOST}
RUNREG_SCRIPT=perl runregression
endif
# DYNAMORIO_{TOOLS,LIBUTIL,MAKE,SUITE,API} must be set
# for a regression.  We'll supply default values relative to the
# current directory if they're not explicitly provided.
ifndef DYNAMORIO_CORE
  DYNAMORIO_CORE := core
endif
ifndef DYNAMORIO_TOOLS
  DYNAMORIO_TOOLS := tools
endif
ifndef DYNAMORIO_LIBUTIL
  DYNAMORIO_LIBUTIL := libutil
endif
ifndef DYNAMORIO_SUITE
  DYNAMORIO_SUITE := suite
endif
ifndef DYNAMORIO_API
  DYNAMORIO_API := api
endif
ifeq ($(MACHINE), win32)
DR_CORE=$(shell $(CYGPATH) -ui ${DYNAMORIO_CORE})
DR_MAKE=$(shell $(CYGPATH) -ui ${DYNAMORIO_MAKE})
DR_TOOLS=$(shell $(CYGPATH) -ui ${DYNAMORIO_TOOLS})
DR_LIBUTIL=$(shell $(CYGPATH) -ui ${DYNAMORIO_LIBUTIL})
DR_SUITE=$(shell $(CYGPATH) -ui ${DYNAMORIO_SUITE})
DR_API=$(shell $(CYGPATH) -ui ${DYNAMORIO_API})
else
DR_CORE=${DYNAMORIO_CORE}
DR_MAKE=${DYNAMORIO_MAKE}
DR_TOOLS=${DYNAMORIO_TOOLS}
DR_LIBUTIL=${DYNAMORIO_LIBUTIL}
DR_SUITE=${DYNAMORIO_SUITE}
DR_API=${DYNAMORIO_API}
endif

SHELL_RC=~/.bashrc

# If you have problems rsync-ing (replacing an in-use file, etc.) can disable by
# running with NORSYNC=1 (will still rsync the src tree)
NORSYNC = 0

# We use -C to avoid clobbering CVS files, so remote dirs can still be used as cvs co's
# if so desired.
RSYNC = rsync -auvzC --exclude=*.map --exclude=*.pdb

RSYNC_TOOLS_FLAGS := --include=/external/whoami.exe --exclude=/external/*

runregression:
	@(if [ ! -d ${DR_CORE} ]; then \
		$(ECHO) "ERROR: ${DR_CORE} does not exist!"; false; fi)
	@(if [ ! -d ${DR_MAKE} ]; then \
		$(ECHO) "ERROR: ${DR_MAKE} does not exist!"; false; fi)
	@(if [ ! -d ${DR_TOOLS} ]; then \
		$(ECHO) "ERROR: ${DR_TOOLS} does not exist!"; false; fi)
	@(if [ ! -d ${DR_LIBUTIL} ]; then \
		$(ECHO) "ERROR: ${DR_LIBUTIL} does not exist!"; false; fi)
	@(if [ ! -d ${DR_SUITE} ]; then \
		$(ECHO) "ERROR: ${DR_SUITE} does not exist!"; false; fi)
	@(if [ ! -d ${DR_API} ]; then \
		$(ECHO) "ERROR: ${DR_API} does not exist!"; false; fi)
# Since there may be no subdirs already created, and we can't use rsync's --relative
# w/o being able to break up the local path and w/o identical path tails, we use
# mkdir -p to make the dir structure.
	${RUNREG_CMD} "mkdir -p ${RUNREG_SRC}"
	${RSYNC} ${DR_CORE}/ ${RUNREG_LOC}${RUNREG_SRC}
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
	    --exclude=results/ --exclude=stress/run/		\
	    --exclude=regression-*/ --exclude=cache/		\
	    --exclude=/release --exclude=/native		\
	    --exclude=logs/ --include=events*dummy?.so		\
	    ${DR_SUITE}/ ${RUNREG_LOC}${RUNREG_SUITE}
	${RSYNC} --include=VIPA_test.exe ${DR_API}/ ${RUNREG_LOC}${RUNREG_API}
endif
# We must source .bashrc to get the compiler path, etc. set up
# (cygwin bash non-interactive does not source it even if started by sshd).
	${RUNREG_CMD} " \
	    if [ -e ${SHELL_RC} ]; then source ${SHELL_RC}; fi; \
	    cd ${RUNREG_SUITE}; \
            export DYNAMORIO_BASE=${RUNREG_TREE}; \
            export DYNAMORIO_TOOLS=${RUNREG_TOOLS}; \
            export DYNAMORIO_LIBUTIL=${RUNREG_LIBUTIL}; \
	    export DYNAMORIO_MAKE=${RUNREG_MAKE}; \
	    export DYNAMORIO_API=${RUNREG_API}; \
            ${RUNREG_SCRIPT} ${RUNREG_OPTIONS}" 2>&1 | $(TEE) ${REGRESSION}-${RUNREG_HOST}
	@$(GREP) "all builds succesful" ${REGRESSION}-${RUNREG_HOST}
