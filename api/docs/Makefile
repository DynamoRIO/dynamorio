# **********************************************************
# Copyright (c) 2007-2009 VMware, Inc.  All rights reserved.
# **********************************************************/

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

.PHONY: all clean mkimages

ifndef DYNAMORIO_MAKE
DYNAMORIO_MAKE := ../../make
endif

include $(DYNAMORIO_MAKE)/compiler.mk

ifndef DYNAMORIO_HOME
DYNAMORIO_HOME := ../../exports
endif

ifndef DYNAMORIO_LIBUTIL
DYNAMORIO_LIBUTIL := ../../libutil
endif

ifeq ($(MACHINE), linux)
HEADER_DIR := $(DYNAMORIO_HOME)/include
else
HEADER_DIR := $(shell $(CYGPATH) -u $(DYNAMORIO_HOME)/include)
endif
HEADERS    := $(wildcard $(HEADER_DIR)/dr_*.h)

# We need to escape HEADER_DIR so we can pass it to sed.
# Use sed to put in the escape characters.
EXPORT_ESC := $(shell $(ECHO) "$(HEADER_DIR)" | $(SED) 's/\./\\\./g')
EXPORT_ESC := $(shell $(ECHO) "$(EXPORT_ESC)" | $(SED) 's/\//\\\//g')
EXPORT_SED := "s/\(INPUT[ \t]*=\)/\1 $(EXPORT_ESC)/g"

# We also need to escape the path to libutil
LIBUTIL_DIR := $(DYNAMORIO_LIBUTIL)
ifeq ($(MACHINE), win32)
LIBUTIL_DIR := $(shell $(CYGPATH) -u $(DYNAMORIO_LIBUTIL))
endif
LIBUTIL_ESC := $(shell $(ECHO) "$(LIBUTIL_DIR)" | $(SED) 's/\./\\\./g')
LIBUTIL_ESC := $(shell $(ECHO) "$(LIBUTIL_ESC)" | $(SED) 's/\//\\\//g')
LIBUTIL_SED := "s/\(INPUT[ \t]*=\)/\1 $(LIBUTIL_ESC)/g"


all: check_headers $(HEADERS) mkimages Doxyfile
	@$(ECHO) "** Using headers in $(HEADER_DIR) **"
ifdef VMSAFE
	@$(ECHO) "Use the macro \\DynamoRIO to allow name substitution"
# we don't do -i to allow function names and macros
	$(GREP) -E -n -H "(^|[^\\])DynamoRIO" *.dox; if [ $$? == 0 ]; then false; fi
	$(GREP) -E -n -i -H "(^|[^_\\-])client($$|[^_])" *.dox; if [ $$? == 0 ]; then false; fi
endif

# run doxygen and fail on warnings
	$(DOXYGEN) 2>&1 | $(TEE) doxygen.out
	$(GREP) -i warning doxygen.out; if [ $$? == 0 ]; then false; fi
	$(RM) doxygen.out

# doxygen does not put header.html on the top-level frame so we add favicon manually
	$(SED) -i 's/<title>/<link rel="shortcut icon" type="image\/x-icon" href="favicon.ico" \/>\n<title>/' html/index.html
# add our logo to treeview pane (yes there is no closing </body></html>)
	$(ECHO) '<p>&nbsp;<br><img src="drlogo.png"></p>' >> html/tree.html
# workaround for doxygen bug
	$(CP) html/htmlstyle.css html/doxygen.css
# get our samples in place
	$(MKDIR) -p html/samples
	$(CP) -a ../samples/*.c html/samples/
# not exporting custom stats for VMsafe, security api for VMAP
# FIXME: these exclusions are duplicated in tools/makezip.sh: combine somehow?
ifdef VMSAFE
# -f because of perforce file protections being carried over
	$(RM) html/samples/stats.c html/samples/tracedump.c
	$(CP) -a ../samples/MF_moduledb-sample.config html/samples/
	$(CP) -a ../samples/VIPA_test.exe html/samples/
else
	$(RM) html/samples/MF_moduledb*
ifeq ($(MACHINE), linux)
# no stats sample on linux
	$(RM) html/samples/stats.c
endif
endif
	$(CP) -a ../samples/Makefile html/samples/
# IE for some reason won't initially load an .ico in a web page from a server,
# though will show it via file: or after pointing straight at image from server.
# doxygen fails to copy image referenced only in footer.html
	$(CP) images/favicon.png html/

check_headers:
	@(if ! test -e $(HEADER_DIR)/dr_api.h; then \
	  echo ERROR: API header files do not exist; false; fi)

pdf: all
	-$(CP) images/vmware-logo-crop.png latex/vmware-logo.png
	./fix-latex.pl
	$(MAKE) -C latex

# note that we can have "a" vs "an" issues
# thus, going w/ a consonant-onset acronym:
# VIPER = VMsafe In-ProcEss Runtime
VMSAFE_NAME=VIPER
# Viper In-Process Agent: again, starting w/ a consonant
VMSAFE_CLIENT=VIPA

Doxyfile: API.doxy Makefile
	$(SED) $(EXPORT_SED) $< > $@
	$(SED) -i $(LIBUTIL_SED) $@
ifdef VMSAFE
	$(SED) -i 's/\(PROJECT_NAME[ \t]*=\).*/\1 "VMsafe In-Process API"/' $@
	$(SED) -i 's/\(ENABLED_SECTIONS[ \t]*=\)/\1 vmsafe/' $@
	$(SED) -i 's/DynamoRIO=DynamoRIO/DynamoRIO=${VMSAFE_NAME}/' $@
	$(SED) -i 's/client=client/client=${VMSAFE_CLIENT}/' $@
	$(SED) -i 's/clients=clients/clients=${VMSAFE_CLIENT}s/' $@
	$(SED) -i 's/Client=Client/Client=${VMSAFE_CLIENT}/' $@
	$(SED) -i 's/Clients=Clients/Clients=${VMSAFE_CLIENT}s/' $@
else
	$(SED) -i 's/\(ENABLED_SECTIONS[ \t]*=\)/\1 linux/' $@
endif
# "doxygen -u" seems to preserve our non-default settings, while eliminating
# warnings about deprecated options due to differing versions of doxygen
	$(DOXYGEN) -u

ifndef HEADER_DIR
update_headers:
	@$(ECHO) "must set HEADER_DIR variable"; false
else
update_headers: check_headers
# even w/ no extension given, my cygwin perl makes .bak files!
	cd $(HEADER_DIR); \
	  $(PERL) -pi -e 's|\bDynamoRIO\b|${VMSAFE_NAME}|g' *.h; \
	  $(PERL) -pi -e 's|\bDR\b|${VMSAFE_NAME}|g' *.h;        \
	  $(PERL) -pi -e 's|\bclient(s?)\b|${VMSAFE_CLIENT}\1|gi' *.h; \
	  $(RM) *.bak
endif

####################
# Figures from xfig
#
# Requires fig2dev from the transfig package and mogrify from the ImageMagick
# package to be installed.
ifdef VMSAFE
FIGXL8 := $(SED) 's|DynamoRIO|${VMSAFE_NAME}|g'
else
FIGXL8 := $(CAT)
endif
GENIMGDIR := genimages
FIGS := $(wildcard *.fig)
FIG_IMAGES := $(patsubst %.fig,$(GENIMGDIR)/%.eps,$(FIGS)) \
              $(patsubst %.fig,$(GENIMGDIR)/%.png,$(FIGS))
mkimages: $(GENIMGDIR) $(FIG_IMAGES)
$(GENIMGDIR):
	if ! test -d $(GENIMGDIR); then $(MKDIR) -p $(GENIMGDIR); fi
# fig2dev w/ pipe or redirect crashes sometimes on cygwin so
# we use ugly temp files
$(GENIMGDIR)/%.post.fig: %.fig
	${FIGXL8} $< > $@
$(GENIMGDIR)/%.eps: $(GENIMGDIR)/%.post.fig
	$(FIG2DEV) -L eps $< $@
# Because fig2dev does not perform anti-aliasing, converting directly
# to an image format can result in very poor quality lines and text.
# This problem is compounded if that image is subsequently resized
# without any anti-aliasing, such as by your web browser.  The
# solution is to magnify the vector data to at least 4x and then
# generate a lossless bitmap format such as TIFF; then have a real
# image manipulator resize it to the final size (cannot make
# browser-resizable vector graphics).
# See also http://www.burningcutlery.com/derek/bargraph/
%.png: %.tiff
	$(MOGRIFY) -resize 33% -format png $<
# ImageMagick 6.0.4 produces foo.png.0 while 6.3.0 creates foo-0.png
	@(if test -e $*-1.png; then $(RM) $*-1.png; $(MV) $*-0.png $@; else \
	  $(RM) $*.png.1; $(MV) $*.png.0 $@; fi)
$(GENIMGDIR)/%.tiff: $(GENIMGDIR)/%.post.fig
	$(FIG2DEV) -L tiff -m 4 $< $@
####################

clean:
	$(RM) -r html latex rtf Doxyfile $(GENIMGDIR)

