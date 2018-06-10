#!/bin/bash

# **********************************************************
# Copyright (c) 2018 Arm Limited    All rights reserved.
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

# Usage: ./aarch64_codec_codec_order.sh <path to core/arch/aarch64/> <path to build dir>

# Check if operand patterns are ordered by pattern.
cat $1/codec.txt | \
    perl -ne  '$x = 1 if /Instruction patterns/; print "$1\n" if !$x && /^([x\-?]+)  [a-z0-9_A-Z]+.*#/;' > $2/patterns.codec.txt
LANG=C sort $2/patterns.codec.txt > $2/patterns.codec.txt.sorted
diff $2/patterns.codec.txt $2/patterns.codec.txt.sorted
if [[ $? != 0 ]]; then
    echo "Patterns in codec.txt are not ordered as expected. See the diff " \
         "between $2/patterns.codec.txt and $2/patterns.codec.txt.sorted."
    exit 1
fi

# Check if operand patterns in codec.txt and decode_opnd_XXX order in codec.c
# matches.
cat $1/codec.txt | \
    perl -ne  '$x = 1 if /Instruction patterns/; print "$1\n" if !$x && /^[x\-?]+  ([a-z0-9_A-Z]+).*#/;' > $2/names.codec.txt
cat $1/codec.c | \
    perl -ne '$x = 1 if /each type of operand/; print "$1\n" if $x && /^decode_opnd_([^(]+)/;' > $2/names.codec.c
diff $2/names.codec.txt $2/names.codec.c
if [[ $? != 0 ]]; then
    echo "Operand order differs between codec.txt and codec.c. See the diff " \
         "between $2/names.codec.txt and $2/names.codec.c."
    exit 1
fi
