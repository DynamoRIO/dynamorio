# **********************************************************
# Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

ifndef DYNAMORIO_MAKE
  DYNAMORIO_MAKE := ../make
endif

include $(DYNAMORIO_MAKE)/compiler.mk

# 
# Linux
#
ifeq ($(MACHINE), linux)

.PHONY: all

all: runstats

runstats: runstats.c
	$(CC) $< -o $@

clean:
	$(RM) runstats

#
# Windows
#
else

# use DYNAMORIO_HOME and LIBUTIL if defined
ifndef DYNAMORIO_HOME
  DYNAMORIO_HOME := ..
endif
ifndef DYNAMORIO_LIBUTIL
  DYNAMORIO_LIBUTIL := $(DYNAMORIO_HOME)/libutil
endif

WIN_DYNAMORIO_HOME := $(shell $(CYGPATH) -ma $(DYNAMORIO_HOME))
WIN_DYNAMORIO_LIBUTIL := $(shell $(CYGPATH) -ma $(DYNAMORIO_LIBUTIL))

CORE_DIRNAME := core

# MSINCLUDE intentionally last so we don't get VisualStudio's share.h
INCLUDES := /I"$(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)" \
	    /I"$(WIN_DYNAMORIO_HOME)/$(CORE_DIRNAME)/lib" \
	    /I"$(WIN_DYNAMORIO_LIBUTIL)" \
	    $(MSINCLUDE)

# We get resource defines and the build number from
# Makefile.custom_build. (DIFFNUM is referenced there)
ifndef DIFFNUM
  DIFFNUM := 1
endif
include $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/Makefile.custom_build

BNFLAGS := /DBUILD_NUMBER=$(BUILD_NUMBER)
ifneq ($(VERSION_NUMBER),)
  BNFLAGS += /DVERSION_NUMBER=$(VERSION_NUMBER)
endif
ifneq ($(VERSION_COMMA_DELIMITED),)
  BNFLAGS += /DVERSION_COMMA_DELIMITED=$(VERSION_COMMA_DELIMITED)
endif

LDFLAGS += $(MSLIB) /release /opt:ref /opt:icf /machine:IX86 \
	   /LIBPATH:"$(WIN_DYNAMORIO_LIBUTIL)" /mapinfo:exports \
	   user32.lib advapi32.lib pdh.lib imagehlp.lib Shell32.lib

# FIXME eventually upgrade to W4 with pragma exceptions.
# W2 is default (we're using W3).  We should also replace
# all references to unsafe functions (e.g., fopen) and
# remove /wd4996
CFLAGS += /W3 /WX /wd4996 $(INCLUDES) /DNOT_DYNAMORIO_CORE /DWIN32 $(BNFLAGS)
ifeq ($(EXTERNAL_DRVIEW), 1)
CFLAGS += /DEXTERNAL_DRVIEW=1
endif

# xref PR 192750 - runregression overrides this
DBG = /Zi
LINK_DBG = /debug

ifdef DEBUG
  CFLAGS += $(DBG) /Od /MTd
  LDFLAGS += $(LINK_DBG)
  POLICY_LIB := $(DYNAMORIO_LIBUTIL)/policy_staticd.lib
else
# use $(DBG) & $(LINK_DBG) so we get a pdb file (like we do for core release)
  CFLAGS += $(DBG) /O2 /MT
  LDFLAGS += $(LINK_DBG)
  POLICY_LIB := $(DYNAMORIO_LIBUTIL)/policy_static.lib
endif

CONFIG_DLL  := drconfig.dll
CONFIG_LIB  := drconfig.lib
CONFIG_PATH := '$(shell $(CYGPATH) -ma $(DYNAMORIO_LIBUTIL))'

CFLAGS_NO_UNICODE := $(CFLAGS)
CFLAGS += /DUNICODE /D_UNICODE 

CFLAGS_DRDEPLOY := $(CFLAGS)
ifndef VMAP
# We default to VMSAFE configuration
CFLAGS_DRDEPLOY += /DPROGRAM_SHEPHERDING /DMF_API /DHOT_PATCHING_INTERFACE /DPROBE_API
endif

WIN_POLICY_LIB := "$(shell $(CYGPATH) -ma $(POLICY_LIB))"

RCDEFS  := $(RESOURCE_DEFINES) /DFILE_TYPE=VFT_APP
RCFLAGS := -r $(INCLUDES) $(RCDEFS) $(BNFLAGS)

RCFILE := $(DYNAMORIO_HOME)/$(CORE_DIRNAME)/win32/resources.rc
WIN_RCFILE := "$(shell $(CYGPATH) -ma $(RCFILE))"

TGTS := DRview.exe DRcontrol.exe DRkill.exe closewnd.exe \
	winstats.exe svccntrl.exe DRload.exe \
	balloon.exe dummy.exe ldmp.exe \
	drdeploy.exe

.PHONY: DRgui policy_lib_check

all: $(TGTS) DRgui

%.exe: %.obj %.res $(POLICY_LIB)
	$(LD) $*.obj $*.res $(LDFLAGS) $(WIN_POLICY_LIB)

%.obj: %.c 
	$(CC) $(CFLAGS) /c $<

$(POLICY_LIB):
	@(if [ ! -e $(POLICY_LIB) ]; then \
	$(ECHO) "$(POLICY_LIB) does not exist; you may need to build it."; \
	false; fi)

# drdeploy.exe links to libutil/drconfig.dll.  When we build drdeploy,
# we'll copy the DLL to the tools directory so users won't need to
# update their path.
drdeploy.exe: $(CONFIG_DLL) drdeploy.res drdeploy.c
	$(CC) $(CFLAGS_DRDEPLOY) /c drdeploy.c
	$(LD) drdeploy.obj drdeploy.res $(LDFLAGS) $(CONFIG_LIB) $(WIN_POLICY_LIB)
# We could add a manifest so that we can request elevation on Vista (xref PR 233520).
# Works fine for double clicking the app, but running from a cmd shell just gives access
# denied if the cmd shell isn't runas admin which isn't quite as nice.  Disable for now. 
# $(MT) -manifest drdeploy.exe.manifest -outputresource:drdeploy.exe

$(CONFIG_DLL): $(DYNAMORIO_LIBUTIL)/$(CONFIG_DLL)
	$(CP) $< ./

$(DYNAMORIO_LIBUTIL)/$(CONFIG_DLL):
	@(if [ ! -e $@ ]; then \
	$(ECHO) "$@ does not exist; you may need to build it."; \
	false; fi)

closewnd.obj: closewnd.c
	$(CC) $(CFLAGS_NO_UNICODE) /c $< 

winstats.obj: winstats.c
	$(CC) $(CFLAGS_NO_UNICODE) /c $< 

# need ntdll.lib
ifeq ($(ARCH), x64)
NTDLL_LIBPATH := $(NTDLL_LIBPATH_X64)
else
NTDLL_LIBPATH := $(NTDLL_LIBPATH_X86)
endif
ldmp.exe: ldmp.c ldmp.res
	$(CC) $(CFLAGS) $< /link ldmp.res $(LDFLAGS) /LIBPATH:"$(subst /,\\,$(NTDLL_LIBPATH))" ntdll.lib

DRview.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"DRview.exe\" /DFILE_DESCRIPTION="PRODUCT_NAME \" cmd line status viewer\"" $(WIN_RCFILE)

DRcontrol.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"DRcontrol.exe\" /DFILE_DESCRIPTION="PRODUCT_NAME \" cmd line configuration tool\"" $(WIN_RCFILE)

DRkill.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"DRkill.exe\" /DFILE_DESCRIPTION="PRODUCT_NAME \" process terminating utility\"" $(WIN_RCFILE)

drdeploy.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"drdeploy.exe\" /DFILE_DESCRIPTION="PRODUCT_NAME \" DynamoRIO deployment command-line utility\"" $(WIN_RCFILE)

closewnd.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"closewnd.exe\" /DFILE_DESCRIPTION="\"closes window app\"" /DPRERELEASE_BUILD $(WIN_RCFILE)

winstats.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"winstats.exe\" /DFILE_DESCRIPTION="\"gathers memory stats\"" /DPRERELEASE_BUILD $(WIN_RCFILE)

svccntrl.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"svccntrl.exe\" /DFILE_DESCRIPTION="PRODUCT_NAME \" cmd line services configuration utility\"" $(WIN_RCFILE)

DRload.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"DRload.exe\" /DFILE_DESCRIPTION="\"loads a dll for analysis\"" /DPRERELEASE_BUILD $(WIN_RCFILE)

balloon.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"balloon.exe\" /DFILE_DESCRIPTION="\"out of memory testing tool\"" /DPRERELEASE_BUILD $(WIN_RCFILE)

runjob.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"runjob.exe\" /DFILE_DESCRIPTION="\"job limit testing tool\"" /DPRERELEASE_BUILD $(WIN_RCFILE)

readproc.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"readproc.exe\" /DFILE_DESCRIPTION="\"dump process memory\"" /DPRERELEASE_BUILD $(WIN_RCFILE)

drdel.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"drdel.exe\" /DFILE_DESCRIPTION="\"delete from file cache\"" /DPRERELEASE_BUILD $(WIN_RCFILE)

dummy.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"dummy.exe\" /DFILE_DESCRIPTION="\"dummy process for ldmp creation\"" /DPRERELEASE_BUILD $(WIN_RCFILE)

ldmp.res: $(RCFILE)
	$(RC) /Fo$@ $(RCFLAGS) /DFILE_NAME=\"ldmp.exe\" /DFILE_DESCRIPTION="\".ldmp reading tool\"" /DPRERELEASE_BUILD $(WIN_RCFILE)


DRgui:
# case 192750: propagate whether to build pdb files
	$(MAKE) -C DRgui DBG=$(DBG) LINK_DBG=$(LINK_DBG)

clean:
	$(RM) *.obj *.ilk *.pdb *.map *.exp *.res *.dll
	$(RM) $(TGTS)
	$(MAKE) -C DRgui clean


# Support for adding symbols to a symbol server
# you can do this only on Windows

# You need of course double backslashes if you want to pass a name, 
# so \\\\fileserver\\symbols\\product

#FIXME: set the IP properly for fileserver, right now it is going to evlk-w2kas 
SYMBOL_SERVER=\\\\fileserver\\symbols

# I also like my VMs to have host only network so then I do
#   subst y: z:\shared\symbols
# which would be host shared with /mnt/fileserver/symbols if that were writable
#SYMBOL_SERVER=y:\\

SYMBOL_STORE=${SYMBOL_SERVER}\\product

# Note: you'd need EXPORTS=..\\..\\exports if you don't have DYNAMORIO_HOME setup
EXPORTS=${WIN_DYNAMORIO_HOME}\\exports
GUI_EXPORTS=${WIN_DYNAMORIO_HOME}\\mmcgui\\Release
PRODUCT_NAME="SecureCore"
GUI_NAME="SecureCore GUI"
DATE='$(shell $(DATE) +"Built at %Y-%m-%d %T")'
COMMENT="Build ${BUILD_NUMBER} by ${USER}@${HOSTNAME}"

symbols:
	cd external; ./symstore add /r /f ${EXPORTS} /s ${SYMBOL_STORE} /t ${PRODUCT_NAME} /v ${COMMENT} /c ${DATE}

guisymbols:
	cd external; ./symstore add /r /f ${GUI_EXPORTS} /s ${SYMBOL_STORE} /t ${GUI_NAME} /v ${COMMENT} /c ${DATE}
# FIXME: should tail ${SYMBOL_STORE}/000Admin/history to check if the file was really added

endif
