/* **********************************************************
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"

/* check if all bits in mask are set in var */
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
/* check if any bit in mask is set in var */
#define TESTANY(mask, var) (((mask) & (var)) != 0)
/* check if a single bit is set in var */
#define TEST TESTANY

#if LINUX
/* Need to declare memset.  It's intrinsic so should be ok. */
void *memset(void *s, int c, size_t n);
#endif

byte * find_prot_edge(const byte *start, uint prot_flag)
{
    uint prot;
    byte *base = (byte *)start;
    byte *last;
    size_t size = 0;

    do {
        last = base + size;
    } while (dr_query_memory(last, &base, &size, &prot) && TESTALL(prot_flag, prot));
    
    if (last == start)
        dr_printf("error in find_prot_edge");

    return last;
}

bool
memchk(byte *start, byte value, size_t size)
{
    size_t i;
    for (i = 0; i < size && *(start+i) == value; i++) ;
    return i == size;
}

const byte read_only_buf[2*PAGE_SIZE] = {0};

/* NOTE - we need to initialize the writable buffers to some non-zero value so that they
 * will all be placed in the same memory region (on Linux only the first page is part
 * of the map and the remaining pages are just allocated instead of mapped if these are
 * 0). */
byte safe_buf[2*PAGE_SIZE+100] = {1};

byte writable_buf[2*PAGE_SIZE] = {1};

bool dummy_func()
{
    return true;
}

DR_EXPORT
void dr_init(client_id_t id)
{
    file_t file;
    char buf[100];
    int64 pos;
    int i;
    uint prot;
    byte *base_pc;
    size_t size;
    size_t bytes_read, bytes_written;
    byte *edge;

    /* The Makefile will pass a full absolute path (for Windows and Linux) as the client
     * option to a dummy file in the which we use to exercise the file api routines.
     * TODO - these tests should be a lot more thorough, but the basic functionality
     * is there (should add write tests, file_exists, directory etc. tests). */
    file = dr_open_file(dr_get_options(id), DR_FILE_READ);
    if (file == INVALID_FILE)
        dr_printf("Error opening file\n");
    memset(buf, 0, sizeof(buf));
    dr_read_file(file, buf, 10);
    pos = dr_file_tell(file);
    if (pos < 0)
        dr_printf("tell error\n");
    dr_printf("%s\n", buf);
    if (!dr_file_seek(file, 0, DR_SEEK_SET))
        dr_printf("seek error\n");
    memset(buf, 0, sizeof(buf));
    dr_read_file(file, buf, 5);
    dr_printf("%s\n", buf);
    for (i = 0; i < 100; i++) buf[i] = 0;
    if (!dr_file_seek(file, pos - 5, DR_SEEK_CUR))
        dr_printf("seek error\n");
    memset(buf, 0, sizeof(buf));
    dr_read_file(file, buf, 7);
    dr_printf("%s\n", buf);
    if (!dr_file_seek(file, -6, DR_SEEK_END))
        dr_printf("seek error\n");
    memset(buf, 0, sizeof(buf));
    dr_read_file(file, buf, 6);
    dr_printf("%s\n", buf);
    dr_close_file(file);

    /* Test the memory query routines */
    dummy_func();
    if (!dr_memory_is_readable((byte *)dummy_func, 1) ||
        !dr_memory_is_readable(read_only_buf+1000, 4000) ||
        !dr_memory_is_readable(writable_buf+1000, 4000)) {
        dr_printf("ERROR : dr_memory_is_readable() incorrect results\n");
    }

    if (!dr_query_memory((byte *)dummy_func, &base_pc, &size, &prot))
        dr_printf("ERROR : can't find dummy_func mem region\n");
    dr_printf("dummy_func is %s%s%s\n", TEST(DR_MEMPROT_READ, prot) ? "r" : "",
              TEST(DR_MEMPROT_WRITE, prot) ? "w" : "",
              TEST(DR_MEMPROT_EXEC, prot) ? "x" : "");
    if (base_pc > (byte *)dummy_func || base_pc + size < (byte *)dummy_func)
        dr_printf("dummy_func region mismatch");

    memset(writable_buf, 0, sizeof(writable_buf)); /* strip off write copy */
    if (!dr_query_memory(writable_buf+100, &base_pc, &size, &prot))
        dr_printf("ERROR : can't find dummy_func mem region\n");
    dr_printf("writable_buf is %s%s%s\n", TEST(DR_MEMPROT_READ, prot) ? "r" : "",
              TEST(DR_MEMPROT_WRITE, prot) ? "w" : "",
#if LINUX
	      /* Linux sometimes (probably depends on version and hardware NX
	       * support) lists all readable regions as also exectuable in the
	       * maps file.  We just skip checking here for Linux to make
	       * matching the template file easier. */
	      ""
#else
              TEST(DR_MEMPROT_EXEC, prot) ? "x" : ""
#endif
	      );
    if (base_pc > writable_buf || base_pc + size < writable_buf)
        dr_printf("writable_buf region mismatch\n");
    if (base_pc + size < writable_buf + sizeof(writable_buf))
        dr_printf("writable_buf size mismatch "PFX" "PFX" "PFX" "PFX"\n",
                  base_pc, size, writable_buf, sizeof(writable_buf));

    if (!dr_query_memory(read_only_buf+100, &base_pc, &size, &prot))
        dr_printf("ERROR : can't find dummy_func mem region\n");
    dr_printf("read_only_buf is %s%s\n", TEST(DR_MEMPROT_READ, prot) ? "r" : "",
              TEST(DR_MEMPROT_WRITE, prot) ? "w" : "");
    if (base_pc > read_only_buf || base_pc + size < read_only_buf)
        dr_printf("read_only_buf region mismatch");
    if (base_pc + size < read_only_buf + sizeof(read_only_buf))
        dr_printf("read_only_buf size mismatch");
    
    /* test the safe_read functions */
    /* TODO - extend test to cover racy writes and reads (won't work on Linux yet). */
    memset(safe_buf, 0xcd, sizeof(safe_buf));
    if (!dr_safe_read(read_only_buf + 4000, 1000, safe_buf, &bytes_read) ||
        bytes_read != 1000 || !memchk(safe_buf, 0, 1000) || *(safe_buf+1000) != 0xcd) {
        dr_printf("ERROR in plain dr_safe_read()\n");
    }
    memset(safe_buf, 0xcd, sizeof(safe_buf));
    edge = find_prot_edge(read_only_buf, DR_MEMPROT_READ);
    bytes_read = 0xcdcdcdcd;
    if (dr_safe_read(edge - (PAGE_SIZE + 10), PAGE_SIZE+20, safe_buf, &bytes_read) ||
        bytes_read == 0xcdcdcdcd || bytes_read > PAGE_SIZE+10 || 
        !memchk(safe_buf, 0, bytes_read)) {
        dr_printf("ERROR in overlap dr_safe_read()\n");
    }
    dr_printf("dr_safe_read() check\n");

    memset(safe_buf, 0xcd, sizeof(safe_buf));
    if (!dr_safe_write(writable_buf, 1000, safe_buf, &bytes_written) ||
        bytes_written != 1000 || !memchk(writable_buf, 0xcd, 1000) ||
        !memchk(writable_buf+1000, 0, 1000)) {
        dr_printf("ERROR in plain dr_safe_write()\n");
    }
    memset(writable_buf, 0, sizeof(writable_buf));
    edge = find_prot_edge(writable_buf, DR_MEMPROT_WRITE);
    bytes_written = 0xcdcdcdcd;
    if (dr_safe_write(edge - (PAGE_SIZE + 10), PAGE_SIZE+20, safe_buf, &bytes_written) ||
        bytes_written == 0xcdcdcdcd || bytes_written > PAGE_SIZE+10 ||
        !memchk(edge - (PAGE_SIZE + 10), 0xcd, bytes_written)) {
        dr_printf("ERROR in overlap dr_safe_write() "PFX" "PFX" %d\n", writable_buf, edge, bytes_written);
    }
    dr_printf("dr_safe_write() check\n");

    dr_printf("done\n");
}

