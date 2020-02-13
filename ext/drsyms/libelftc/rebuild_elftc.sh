# **********************************************************
# Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
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

# Requires pmake and 32 bit dev libraries to run.
# On Fedora requires bmake and several patches.
# Tested on libelftc r3821 but we have to stay on r3530 until
# https://sourceforge.net/p/elftoolchain/tickets/581/ is fixed.
version="-r3530"

set -e

if [[ -z "$1" ]]
then
  echo "Usage: rebuild_elftc.sh <elf-tc-srcdir>"
  echo "If the target directory doesn't exist, it will be checked out.  If it"
  echo "does, it will be updated."
  exit 1
fi

# Fedora uses bmake.
# We assume user has already patched according to
# https://sourceforge.net/p/elftoolchain/wiki/BuildingFromSource/
if [[ -e /etc/fedora-release ]]
then
    make_cmd=bmake
else
    make_cmd=pmake
fi

elftc_dir="$1"
dr_libelftc_dir=`dirname $0`
dr_libelftc_dir=`(cd "${dr_libelftc_dir}" && pwd)`

if [[ -d "${elftc_dir}" ]]
then
  svn up "${elftc_dir}" ${version}
else
  mkdir -p `dirname "${elftc_dir}"`
  svn co ${version} svn://svn.code.sf.net/p/elftoolchain/code/trunk "${elftc_dir}"
fi
cd "${elftc_dir}"

# Apply patches
# Support re-applying by checking for $?==1
set +e
for p in "${dr_libelftc_dir}"/*.patch; do
    patch -N -p0 < $p
    # -N returns 1 when already applied
    if [[ $? != 0 && $? != 1 ]]
    then
        exit 1
    fi
done
set -e

function build_for_bits() {
  bits="$1"
  ld_mode="$2"
  # We want to directly use DR's allocator instead of relying on its private loader
  # redirecting in order to support static usage with no loader.
  # ld is not actually used, so we can't use its -wrap=malloc feature.
  # Instead we rely on the preprocessor.
  redir="-Dmalloc=__wrap_malloc -Dcalloc=__wrap_calloc -Drealloc=__wrap_realloc -Dfree=__wrap_free -Dstrdup=__wrap_strdup"
  # ${make_cmd} does not notice if you rebuild with a different configuration, so we
  # just make clean.  It builds quickly enough.
  if [[ "${make_cmd}" -eq "pmake" ]]; then
      # XXX: pmake does not seem to clean properly, but bmake is working fine.
      rm -f */*.so
      svn status | grep '?' | awk '{print $2}' | xargs rm -f
  fi
  ${make_cmd} clean
  (cd common   && ${make_cmd}  COPTS="-m${bits} -O2 -g ${redir}" LD="ld -m${ld_mode}" MKPIC=yes all)
  (cd libelf   && ${make_cmd} COPTS="-m${bits} -O2 -g ${redir}" LD="ld -m${ld_mode}" MKPIC=yes libelf_pic.a)
  (cd libdwarf && ${make_cmd} COPTS="-m${bits} -O2 -g ${redir}" LD="ld -m${ld_mode}" MKPIC=yes libdwarf_pic.a)
  (cd libelftc && ${make_cmd} COPTS="-m${bits} -O2 -g ${redir}" LD="ld -m${ld_mode}" MKPIC=yes libelftc_pic.a)
  cp libelf/libelf_pic.a     "${dr_libelftc_dir}/lib${bits}/libelf.a"
  cp libdwarf/libdwarf_pic.a "${dr_libelftc_dir}/lib${bits}/libdwarf.a"
  cp libelftc/libelftc_pic.a "${dr_libelftc_dir}/lib${bits}/libelftc.a"
}

build_for_bits 32 elf_i386
build_for_bits 64 elf_x86_64

# Get the include files.
cp \
  libdwarf/dwarf.h \
  libdwarf/libdwarf.h \
  common/elfdefinitions.h \
  libelf/libelf.h \
  libelftc/libelftc.h \
  "${dr_libelftc_dir}/include"

clang-format -i "${dr_libelftc_dir}"/include/*.h
