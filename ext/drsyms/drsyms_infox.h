/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

    /* INPUTS */
    /** Input: should be set by caller to sizeof(drsym_info_t) */
    size_t struct_size;
    /** Input: should be set by caller to the size of the name[] buffer, in bytes */
    size_t name_size;

    /* OUTPUTS */
    /** Output: file and line number */
    const char *file;
    uint64 line;
    /** Output: offset from address that starts at line */
    size_t line_offs;
    /** Output: offset from module base of start of symbol */
    size_t start_offs;
    /**
     * Output: offset from module base of end of symbol.
     * \note For DRSYM_PECOFF_SYMTAB (Cygwin or MinGW) symbols, the end offset
     * is not known precisely.
     * The start address of the subsequent symbol will be stored here.
     **/
    size_t end_offs;
    /** Output: type of the debug info available for this module */
    drsym_debug_kind_t debug_kind;

    /**
     * Output: size of data available for name.  Only name_size bytes will be
     * copied to name.
     */
    size_t name_available_size;
