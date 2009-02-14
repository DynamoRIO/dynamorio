# **********************************************************
# Copyright (c) 2006-2008 VMware, Inc.  All rights reserved.
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

# Copyright (c) 2006-2007 Determina Corp.

##################################################
# pass build and version numbers
# custom builds: 9XYYZ
# X = developer, YY = tree, Z = diffnum
# YY defaults to 1st 2 letters of CUR_TREE, unless CASENUM is defined,
# in which case it is the last 2 letters of CASENUM (all % 10 of course)

DEVELOPERS := derek.bruening qin.zhao
ifeq ($(strip ${USER}),)
# USER is set on unix, USERNAME on windows -- in this makefile we use USER
USER := ${USERNAME}
endif

ifndef BUILD_NUMBER
# schemes like 1st letter, uid, etc. can be concocted but not easy to avoid
# collisions so we just have a list
DEVELOPER_NUMS := 1 2 3 4 5 6 7 8 9
DEVNUM := $(foreach num,${DEVELOPER_NUMS},\
    $(if $(findstring ${USER},$(word ${num},${DEVELOPERS})), ${num}))
ifeq (${strip ${DEVNUM}},)
# unknown developer gets 0
DEVNUM := 0 
endif

CUR_FULL := $(shell cd ..$(CUR_FULL_ADD); $(ECHO) $$PWD)
CUR_TREE := $(notdir ${CUR_FULL})
ifeq ($(strip ${CASENUM}),)
# convert 1st two chars in tree name to numbers where a=0, b=1, etc.
# I had it as the more familiar a=1, b=2 but then numbers get 1 added to
# them and it gets confusing.
TREENUM := $(shell x=${CUR_TREE}; for ((i=0; i<2; i++)); do \
          $(ECHO) $$((36\#$${x:$$i:1} % 10)); done) 
else
# take last 2 chars of CASENUM, converted to digits as above
TREENUM := $(shell x=${CASENUM}; for ((i=2; i>0; i--)); do \
          $(ECHO) $$((36\#$${x:$$((-$$i)):1} % 10)); done) 
# FIXME: auto-create tree name for diffs by adding CASENUM to CUR_TREE?
endif
BUILD_NUMBER_SPACES := 9${DEVNUM}${TREENUM}${DIFFNUM}
# seems to be no make function for removing spaces so we invoke shell for sed
BUILD_NUMBER := $(shell $(ECHO) ${BUILD_NUMBER_SPACES} | $(SED) 's/ //g')
RESOURCE_DEFINES := $(D)PRIVATE_BUILD $(D)PRERELEASE_BUILD $(D)PRIVATE_BUILD_DESC="\"$(USER) DIFFNUM=$(DIFFNUM) $(CUR_FULL)\""
endif # BUILD_NUMBER

ifdef SPECIAL_BUILD
  RESOURCE_DEFINES += $(D)SPECIAL_BUILD $(D)SPECIAL_BUILD_DESC="\"$(SPECIAL_BUILD)\""
endif
##################################################
