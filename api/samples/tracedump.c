/* **********************************************************
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * tracedump.c
 *
 * Disassembles a trace dump in binary format produced by the
 * -tracedump_binary option.  Also illustrates the standalone API.
 */

#include "dr_api.h"
#include <stdlib.h> /* for realloc */
#include <assert.h>
#include <stddef.h> /* for offsetof */
#include <string.h> /* for memcpy */

#define OUTPUT 0
#define VERBOSE 0
#define VERBOSE_VERBOSE 0

/* binary trace dump format is in dr_tools.h */

#define BUF_SIZE 4096

static app_pc
dis(void *drcontext, app_pc pc, app_pc display_pc)
{
    return disassemble_from_copy(drcontext, pc, display_pc, STDOUT,
                                 true /*show display_pc*/, true /*bytes*/);
}

/* We use an array of SEPARATE_STUB_MAX_SIZE */
#define STUB_STRUCT_MAX_SIZE (sizeof(tracedump_stub_data_t) + SEPARATE_STUB_MAX_SIZE)
#define STUBIDX(stubs, i) \
    ((tracedump_stub_data_t *)(((byte *)stubs) + (i)*STUB_STRUCT_MAX_SIZE))

static void
read_data(file_t f, void *drcontext)
{
    byte sbuf[BUF_SIZE];
    byte *dbuf = NULL;
    ssize_t read;
    int dlen = 0, i;
    byte *p = sbuf, *pc, *next_pc;
    tracedump_trace_header_t hdrs;
    tracedump_file_header_t fhdr;
    tracedump_stub_data_t *stubs;
    int next_stub_offs;
    int cur_stub;

    read = dr_read_file(f, &fhdr, sizeof(fhdr));
    assert(read == sizeof(fhdr));
    if (fhdr.version != _USES_DR_VERSION_) {
        dr_fprintf(STDERR, "Error: file version %d does not match tool version %d\n",
                   fhdr.version, _USES_DR_VERSION_);
        return;
    }
    if (IF_X64_ELSE(!fhdr.x64, fhdr.x64)) {
        dr_fprintf(STDERR, "Error: file architecture %s does not match tool's %s\n",
                   fhdr.x64 ? "x64" : "x86", IF_X64_ELSE("x64", "x86"));
        return;
    }
    if (fhdr.linkcount_size != 0 && fhdr.linkcount_size != 4 &&
        fhdr.linkcount_size != 8) {
        dr_fprintf(STDERR, "Error: file is not a trace dump\n");
        return;
    }

    while (1) {
        read = dr_read_file(f, &hdrs, sizeof(tracedump_trace_header_t));
        if (read != sizeof(tracedump_trace_header_t))
            break; /* assume end of file */
#ifdef X86_64
        set_x86_mode(drcontext, !hdrs.x64);
#endif
        dr_printf("\nTRACE # %d\n", hdrs.frag_id);
        dr_printf("Tag = " PFX "\n", hdrs.tag);
        if (hdrs.num_bbs > 0) {
            uint j;
            app_pc tag;
            dr_printf("\nORIGINAL CODE\n");
            for (j = 0; j < hdrs.num_bbs; j++) {
                read = dr_read_file(f, sbuf, BB_ORIGIN_HEADER_SIZE);
                assert(read == BB_ORIGIN_HEADER_SIZE);
                p = sbuf;
                tag = *((app_pc *)p);
                p += sizeof(tag);
                dr_printf("Basic block %d: tag " PFX "\n", j, tag);
                i = *((int *)p);
                p += sizeof(i);
                dr_printf("Size: %d bytes\n", i);
                if (i >= BUF_SIZE) {
                    if (i >= dlen)
                        dbuf = realloc(dbuf, i);
                    p = dbuf;
                } else
                    p = sbuf;
                read = dr_read_file(f, p, i);
                assert(read == i);
                pc = p;
                while (pc - p < i) {
                    pc = dis(drcontext, pc, tag + (pc - p));
                }
            }
            dr_printf("END ORIGINAL CODE\n\n");
        }
        stubs = dr_global_alloc(hdrs.num_exits * STUB_STRUCT_MAX_SIZE);
        assert(stubs != NULL);
        next_stub_offs = hdrs.code_size;
        if (fhdr.linkcount_size > 0) {
            dr_printf("Exit stubs:\n");
        }
        for (i = 0; i < hdrs.num_exits; i++) {
            assert(STUB_DATA_FIXED_SIZE + fhdr.linkcount_size < BUF_SIZE);
            read = dr_read_file(f, sbuf, STUB_DATA_FIXED_SIZE + fhdr.linkcount_size);
            assert(read == (int)STUB_DATA_FIXED_SIZE + fhdr.linkcount_size);
            p = sbuf;

            /* We read in whole struct.  The union and code fields are of
             * course variable-sized so we update them after.
             */
            *STUBIDX(stubs, i) = *(tracedump_stub_data_t *)p;
            p += STUB_DATA_FIXED_SIZE;
            /* linkcounts are no longer available but we have backward compatibility */
            if (fhdr.linkcount_size == 8) {
                STUBIDX(stubs, i)->count.count64 = *((uint64 *)p);
                p += 8;
                dr_printf("\t#%d: target = " PFX ", %s, count = %" UINT64_FORMAT_CODE
                          "\n",
                          i, STUBIDX(stubs, i)->target,
                          STUBIDX(stubs, i)->linked ? "not linked" : "linked",
                          STUBIDX(stubs, i)->count.count64);
            } else if (fhdr.linkcount_size == 4) {
                STUBIDX(stubs, i)->count.count32 = *((uint *)p);
                p += 4;
                dr_printf("\t#%d: target = " PFX ", %s, count = %lu\n", i,
                          STUBIDX(stubs, i)->target,
                          STUBIDX(stubs, i)->linked ? "not linked" : "linked",
                          STUBIDX(stubs, i)->count.count32);
            } else {
                dr_printf("\t#%d: target = " PFX ", %s\n", i, STUBIDX(stubs, i)->target,
                          STUBIDX(stubs, i)->linked ? "not linked" : "linked");
            }
            assert(p - sbuf == (int)STUB_DATA_FIXED_SIZE + fhdr.linkcount_size);
            if (STUBIDX(stubs, i)->stub_pc < hdrs.cache_start_pc ||
                STUBIDX(stubs, i)->stub_pc >= hdrs.cache_start_pc + hdrs.code_size) {
                assert(STUBIDX(stubs, i)->stub_size < BUF_SIZE);
                assert(STUBIDX(stubs, i)->stub_size <= SEPARATE_STUB_MAX_SIZE);
                read = dr_read_file(f, sbuf, STUBIDX(stubs, i)->stub_size);
                assert(read == STUBIDX(stubs, i)->stub_size);
                p = sbuf;
                memcpy(STUBIDX(stubs, i)->stub_code, p, STUBIDX(stubs, i)->stub_size);
                p += STUBIDX(stubs, i)->stub_size;
            } else if (STUBIDX(stubs, i)->stub_pc - hdrs.cache_start_pc <
                       next_stub_offs) {
                next_stub_offs = (int)(STUBIDX(stubs, i)->stub_pc - hdrs.cache_start_pc);
            }
        }
        if (hdrs.code_size >= dlen)
            dbuf = realloc(dbuf, hdrs.code_size);
        p = dbuf;
        read = dr_read_file(f, p, hdrs.code_size);
        assert(read == hdrs.code_size);
        pc = p;
        dr_printf("Size = %d\n", hdrs.code_size);
        dr_printf("Body:\n");
        dr_printf("  -------- indirect branch target entry: --------\n");
        while (pc - p < next_stub_offs) {
            if (pc - p == hdrs.entry_offs)
                dr_printf("  -------- normal entry: --------\n");
            next_pc = decode_next_pc(drcontext, pc);
            if ((next_pc - p) == hdrs.entry_offs && (next_pc - pc == 6))
                dr_printf("  -------- prefix entry: --------\n");
            pc = dis(drcontext, pc, hdrs.cache_start_pc + (pc - p));
        }
        /* stubs */
        for (cur_stub = 0; cur_stub < hdrs.num_exits; cur_stub++) {
            bool separate = STUBIDX(stubs, cur_stub)->stub_pc < hdrs.cache_start_pc ||
                STUBIDX(stubs, cur_stub)->stub_pc >= hdrs.cache_start_pc + hdrs.code_size;
            app_pc stub_pc = (app_pc)STUBIDX(stubs, cur_stub)->stub_code;
            dr_printf("  -------- exit stub %d: -------- <target: " PFX ">\n", cur_stub,
                      STUBIDX(stubs, cur_stub)->target);
            next_stub_offs = hdrs.code_size;
            for (i = cur_stub + 1; i < hdrs.num_exits; i++) {
                if (STUBIDX(stubs, i)->stub_pc >= hdrs.cache_start_pc &&
                    STUBIDX(stubs, i)->stub_pc < hdrs.cache_start_pc + hdrs.code_size) {
                    next_stub_offs =
                        (int)(STUBIDX(stubs, i)->stub_pc - hdrs.cache_start_pc);
                    break;
                }
            }
            if (separate) {
                app_pc spc = stub_pc;
                while (spc - stub_pc < STUBIDX(stubs, cur_stub)->stub_size) {
                    spc = dis(drcontext, spc,
                              STUBIDX(stubs, cur_stub)->stub_pc + (spc - stub_pc));
                }
            } else {
                while (pc - p < next_stub_offs) {
                    pc = dis(drcontext, pc, hdrs.cache_start_pc + (pc - p));
                }
            }
        }
        dr_printf("END TRACE %d\n", hdrs.frag_id);
    }
    if (dbuf != NULL)
        free(dbuf);
}

int
main(int argc, char *argv[])
{
    file_t f;
    void *drcontext = dr_standalone_init();
    if (argc != 2) {
        dr_fprintf(STDERR, "Usage: %s <tracefile>\n", argv[0]);
        return 1;
    }
    f = dr_open_file(argv[1], DR_FILE_READ | DR_FILE_ALLOW_LARGE);
    if (f == INVALID_FILE) {
        dr_fprintf(STDERR, "Error opening %s\n", argv[1]);
        return 1;
    }
    read_data(f, drcontext);
    dr_close_file(f);
    return 0;
}
