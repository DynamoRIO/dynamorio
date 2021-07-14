/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

#ifndef _DR_TRACEDUMP_H_
#define _DR_TRACEDUMP_H_ 1

/**
 * @file dr_tracedump.h
 * @brief Binary trace dump format for the -tracedump_binary option.
 */

/****************************************************************************
 * BINARY TRACE DUMP FORMAT
 */

/**<pre>
 * Binary trace dump format:
 * the file starts with a tracedump_file_header_t
 * then, for each trace:
     struct _tracedump_trace_header
     if num_bbs > 0 # tracedump_origins
       foreach bb:
           app_pc tag;
           int bb_code_size;
           byte code[bb_code_size];
     endif
     foreach exit:
       struct _tracedump_stub_data
       if linkcount_size > 0 # deprecated
         linkcount_type_t count; # sizeof == linkcount_size
       endif
       if separate from body
       (i.e., exit_stub < cache_start_pc || exit_stub >= cache_start_pc+code_size):
           byte stub_code[15]; # all separate stubs are 15
       endif
     endfor
     byte code[code_size];
</pre>
 */
typedef struct _tracedump_file_header_t {
    int version;        /**< The DynamoRIO version that created the file. */
    bool x64;           /**< Whether a 64-bit DynamoRIO library created the file. */
    int linkcount_size; /**< Size of the linkcount (linkcounts are deprecated). */
} tracedump_file_header_t;

/** Header for an individual trace in a binary trace dump file. */
typedef struct _tracedump_trace_header_t {
    int frag_id;           /**< Identifier for the trace. */
    app_pc tag;            /**< Application address for start of trace. */
    app_pc cache_start_pc; /**< Code cache address of start of trace. */
    int entry_offs;        /**< Offset into trace of normal entry. */
    int num_exits;         /**< Number of exits from the trace. */
    int code_size;         /**< Length of the trace in the code cache. */
    uint num_bbs;          /**< Number of constituent basic blocks making up the trace. */
    bool x64;              /**< Whether the trace contains 64-bit code. */
} tracedump_trace_header_t;

/** Size of tag + bb_code_size fields for each bb. */
#define BB_ORIGIN_HEADER_SIZE (sizeof(app_pc) + sizeof(int))

/**< tracedump_stub_data_t.stub_size will not exceed this value. */
#define SEPARATE_STUB_MAX_SIZE IF_X64_ELSE(23, 15)

/** The format of a stub in a trace dump file. */
typedef struct _tracedump_stub_data {
    int cti_offs; /**< Offset from the start of the fragment. */
    /* stub_pc is an absolute address, since can be separate from body. */
    app_pc stub_pc; /**< Code cache address of the stub. */
    app_pc target;  /**< Target of the stub. */
    bool linked;    /**< Whether the stub is linked to its target. */
    int stub_size;  /**< Length of stub_code array */
    /****** the rest of the fields are optional and may not be present! ******/
    union {
        uint count32;   /**< 32-bit exit execution count. */
        uint64 count64; /**< 64-bit exit execution count. */
    } count;            /**< Which field is present depends on the first entry in
                         * the file, which indicates the linkcount size. */
    /** Code for exit stubs.  Only present if:
     *   stub_pc < cache_start_pc ||
     *   stub_pc >= cache_start_pc+code_size).
     * The actual size of the array varies and is indicated by the stub_size field.
     */
    byte stub_code[1 /*variable-sized*/];
} tracedump_stub_data_t;

/** The last offset into tracedump_stub_data_t of always-present fields. */
#define STUB_DATA_FIXED_SIZE (offsetof(tracedump_stub_data_t, count))

#endif /* _DR_TRACEDUMP_H_ */
