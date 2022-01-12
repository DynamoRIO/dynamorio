/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#include "client_tools.h" /* For BUFFER_SIZE_ELEMENTS */

#include <string.h>

#ifdef WINDOWS
#    include <windows.h>
#else
#    include <stdlib.h>
#    include <unistd.h>
#    include <time.h>
#endif

/* check if all bits in mask are set in var */
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
/* check if any bit in mask is set in var */
#define TESTANY(mask, var) (((mask) & (var)) != 0)
/* check if a single bit is set in var */
#define TEST TESTANY

static void
test_dr_rename_delete(void);
static void
test_dir(void);
static void
test_relative(void);
static void
test_map_exe(void);
static void
test_times(void);
static void
test_vfprintf(void);

byte *
find_prot_edge(const byte *start, uint prot_flag)
{
    uint prot;
    byte *base = (byte *)start;
    byte *last;
    size_t size = 0;

    do {
        last = base + size;
    } while (dr_query_memory(last, &base, &size, &prot) && TESTALL(prot_flag, prot));

    if (last == start)
        dr_fprintf(STDERR, "error in find_prot_edge");

    return last;
}

bool
memchk(byte *start, byte value, size_t size)
{
    size_t i;
    for (i = 0; i < size && *(start + i) == value; i++)
        ;
    return i == size;
}

const byte read_only_buf[2 * PAGE_SIZE_MAX] = { 0 };

/* NOTE - we need to initialize the writable buffers to some non-zero value so that they
 * will all be placed in the same memory region (on Linux only the first page is part
 * of the map and the remaining pages are just allocated instead of mapped if these are
 * 0). */
byte safe_buf[2 * PAGE_SIZE_MAX + 100] = { 1 };

byte writable_buf[2 * PAGE_SIZE_MAX] = { 1 };

static file_t file;
static client_id_t client_id;

bool
dummy_func()
{
    return true;
}

static void
event_exit(void)
{
    /* ensure our file was not closed by the app */
    if (!dr_file_seek(file, 0, DR_SEEK_SET))
        dr_fprintf(STDERR, "seek error in exit event\n");
    dr_close_file(file);
    dr_fprintf(STDERR, "file separation check\n");

    /* i#1213: test float i/o.
     * Technically we should save fpstate (for detach) but we're not bothering.
     */
    dr_fprintf(STDERR, "float i/o test: %6.5g\n", 3.1415916);

    dr_fprintf(STDERR, "done\n");
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    char buf[MAXIMUM_PATH];
    int64 pos;
    int i;
    uint prot;
    byte *base_pc;
    size_t size;
    size_t bytes_read, bytes_written;
    byte *edge, *mbuf;
    bool ok;
    byte *f_map;
    client_id = id;

    /* The Makefile will pass a full absolute path (for Windows and Linux) as the client
     * option to a dummy file in the which we use to exercise the file api routines.
     * TODO - these tests should be a lot more thorough, but the basic functionality
     * is there (should add write tests, file_exists, directory etc. tests). */
    file = dr_open_file(dr_get_options(id), DR_FILE_READ);
    if (file == INVALID_FILE)
        dr_fprintf(STDERR, "Error opening file\n");
    memset(buf, 0, sizeof(buf));
    dr_read_file(file, buf, 10);
    pos = dr_file_tell(file);
    if (pos < 0)
        dr_fprintf(STDERR, "tell error\n");
    dr_fprintf(STDERR, "%s\n", buf);
    if (!dr_file_seek(file, 0, DR_SEEK_SET))
        dr_fprintf(STDERR, "seek error\n");
    memset(buf, 0, sizeof(buf));
    dr_read_file(file, buf, 5);
    dr_fprintf(STDERR, "%s\n", buf);
    for (i = 0; i < 100; i++)
        buf[i] = 0;
    if (!dr_file_seek(file, pos - 5, DR_SEEK_CUR))
        dr_fprintf(STDERR, "seek error\n");
    memset(buf, 0, sizeof(buf));
    dr_read_file(file, buf, 7);
    dr_fprintf(STDERR, "%s\n", buf);
    if (!dr_file_seek(file, -6, DR_SEEK_END))
        dr_fprintf(STDERR, "seek error\n");
    memset(buf, 0, sizeof(buf));
    /* read "x\nEOF\n" from the data file */
    dr_read_file(file, buf, 6);
    /* check for DOS line ending */
    if (buf[4] == '\r') {
        /* Account for two line endings: the snippet is "x\r\nEOF\r\n".
         * No conversion required--ctest will discard the '\r' when comparing results.
         */
        if (!dr_file_seek(file, -8, DR_SEEK_END))
            dr_fprintf(STDERR, "seek error\n");
        memset(buf, 0, sizeof(buf));
        dr_read_file(file, buf, 8);
    }
    dr_fprintf(STDERR, "%s\n", buf);
#define EXTRA_SIZE 0x60
    size = PAGE_SIZE + EXTRA_SIZE;
    f_map = dr_map_file(file, &size, 0, NULL, DR_MEMPROT_READ, DR_MAP_PRIVATE);
    if (f_map == NULL || size < (PAGE_SIZE + EXTRA_SIZE))
        dr_fprintf(STDERR, "map error\n");
    /* test unaligned unmap */
    if (!dr_unmap_file(f_map + PAGE_SIZE, EXTRA_SIZE))
        dr_fprintf(STDERR, "unmap error\n");

    /* leave file open and check in exit event that it's still open after
     * app tries to close it
     */
    dr_register_exit_event(event_exit);

    /* Test dr_rename_file. */
    test_dr_rename_delete();

    /* Test the memory query routines */
    dummy_func();
    if (!dr_memory_is_readable((byte *)dummy_func, 1) ||
        !dr_memory_is_readable(read_only_buf + 1000, 4000) ||
        !dr_memory_is_readable(writable_buf + 1000, 4000)) {
        dr_fprintf(STDERR, "ERROR : dr_memory_is_readable() incorrect results\n");
    }

    if (!dr_query_memory((byte *)dummy_func, &base_pc, &size, &prot))
        dr_fprintf(STDERR, "ERROR : can't find dummy_func mem region\n");
    dr_fprintf(STDERR, "dummy_func is %s%s%s\n", TEST(DR_MEMPROT_READ, prot) ? "r" : "",
               TEST(DR_MEMPROT_WRITE, prot) ? "w" : "",
               TEST(DR_MEMPROT_EXEC, prot) ? "x" : "");
    if (base_pc > (byte *)dummy_func || base_pc + size < (byte *)dummy_func)
        dr_fprintf(STDERR, "dummy_func region mismatch");

    memset(writable_buf, 0, sizeof(writable_buf)); /* strip off write copy */
    if (!dr_query_memory(writable_buf + 100, &base_pc, &size, &prot))
        dr_fprintf(STDERR, "ERROR : can't find dummy_func mem region\n");
    dr_fprintf(STDERR, "writable_buf is %s%s%s\n", TEST(DR_MEMPROT_READ, prot) ? "r" : "",
               TEST(DR_MEMPROT_WRITE, prot) ? "w" : "",
#ifdef UNIX
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
        dr_fprintf(STDERR, "writable_buf region mismatch\n");
    if (base_pc + size < writable_buf + sizeof(writable_buf))
        dr_fprintf(STDERR, "writable_buf size mismatch " PFX " " PFX " " PFX " " PFX "\n",
                   base_pc, size, writable_buf, sizeof(writable_buf));

    if (!dr_query_memory(read_only_buf + 100, &base_pc, &size, &prot))
        dr_fprintf(STDERR, "ERROR : can't find dummy_func mem region\n");
    dr_fprintf(STDERR, "read_only_buf is %s%s\n", TEST(DR_MEMPROT_READ, prot) ? "r" : "",
               TEST(DR_MEMPROT_WRITE, prot) ? "w" : "");
    if (base_pc > read_only_buf || base_pc + size < read_only_buf)
        dr_fprintf(STDERR, "read_only_buf region mismatch");
    if (base_pc + size < read_only_buf + sizeof(read_only_buf))
        dr_fprintf(STDERR, "read_only_buf size mismatch");

    /* test the safe_read functions */
    /* TODO - extend test to cover racy writes and reads (won't work on Linux yet). */
    memset(safe_buf, 0xcd, sizeof(safe_buf));
    if (!dr_safe_read(read_only_buf + 4000, 1000, safe_buf, &bytes_read) ||
        bytes_read != 1000 || !memchk(safe_buf, 0, 1000) || *(safe_buf + 1000) != 0xcd) {
        dr_fprintf(STDERR, "ERROR in plain dr_safe_read()\n");
    }
    memset(safe_buf, 0xcd, sizeof(safe_buf));
    /* read_only_buf will be in .rodata on Linux, and can be followed by string
     * constants with the same page protections.  In order to be sure that we're
     * copying zeroes, we map our own memory.
     */
    mbuf = dr_nonheap_alloc(PAGE_SIZE * 3, DR_MEMPROT_READ | DR_MEMPROT_WRITE);
    memset(mbuf, 0, PAGE_SIZE * 3);
    dr_memory_protect(mbuf + PAGE_SIZE * 2, PAGE_SIZE, DR_MEMPROT_NONE);
    edge = find_prot_edge(mbuf, DR_MEMPROT_READ);
    bytes_read = 0xcdcdcdcd;
    if (dr_safe_read(edge - (PAGE_SIZE + 10), PAGE_SIZE + 20, safe_buf, &bytes_read) ||
        bytes_read == 0xcdcdcdcd || bytes_read > PAGE_SIZE + 10 ||
        !memchk(safe_buf, 0, bytes_read)) {
        dr_fprintf(STDERR, "ERROR in overlap dr_safe_read()\n");
    }
    dr_nonheap_free(mbuf, PAGE_SIZE * 3);
    dr_fprintf(STDERR, "dr_safe_read() check\n");

    /* test DR_TRY_EXCEPT */
    DR_TRY_EXCEPT(
        dr_get_current_drcontext(),
        {
            ok = false;
            *((int *)4) = 37;
        },
        { /* EXCEPT */
          ok = true;
        });
    if (!ok)
        dr_fprintf(STDERR, "ERROR in DR_TRY_EXCEPT\n");
    dr_fprintf(STDERR, "DR_TRY_EXCEPT check\n");

    memset(safe_buf, 0xcd, sizeof(safe_buf));
    if (!dr_safe_write(writable_buf, 1000, safe_buf, &bytes_written) ||
        bytes_written != 1000 || !memchk(writable_buf, 0xcd, 1000) ||
        !memchk(writable_buf + 1000, 0, 1000)) {
        dr_fprintf(STDERR, "ERROR in plain dr_safe_write()\n");
    }
    /* so we don't clobber other global vars we use an allocated buffer here */
    mbuf = dr_nonheap_alloc(PAGE_SIZE * 3, DR_MEMPROT_READ | DR_MEMPROT_WRITE);
    if (!dr_memory_protect(mbuf + PAGE_SIZE * 2, PAGE_SIZE, DR_MEMPROT_READ))
        dr_fprintf(STDERR, "ERROR in dr_memory_protect\n");
    memset(mbuf, 0, PAGE_SIZE * 2);
    edge = find_prot_edge(mbuf, DR_MEMPROT_WRITE);
    bytes_written = 0xcdcdcdcd;
    if (dr_safe_write(edge - (PAGE_SIZE + 10), PAGE_SIZE + 20, safe_buf,
                      &bytes_written) ||
        bytes_written == 0xcdcdcdcd || bytes_written > PAGE_SIZE + 10 ||
        !memchk(edge - (PAGE_SIZE + 10), 0xcd, bytes_written)) {
        dr_fprintf(STDERR, "ERROR in overlap dr_safe_write() " PFX " " PFX " %d\n", mbuf,
                   edge, bytes_written);
    }
    dr_nonheap_free(mbuf, PAGE_SIZE * 3);
    dr_fprintf(STDERR, "dr_safe_write() check\n");

    test_dir();

    test_relative();

    test_map_exe();

    test_times();

    test_vfprintf();
}

/* Creates a closed, unique temporary file and returns its filename.
 * XXX: Uses the private loader for OS temp file facilities, so this test cannot
 * be run with -no_private_loader.
 */
static void
get_temp_filename(char filename_out[MAXIMUM_PATH])
{
#ifdef WINDOWS
    char tmppath_buf[MAXIMUM_PATH];
    filename_out[0] = '\0'; /* Empty on failure. */
    if (!GetTempPath(BUFFER_SIZE_ELEMENTS(tmppath_buf), tmppath_buf)) {
        dr_printf("Failed to create temp file.\n");
        return;
    }
    /* A uuid of 0 causes the OS to generate one for us. */
    if (!GetTempFileName(tmppath_buf, "tmp_file_io", /*uuid*/ 0, filename_out)) {
        dr_printf("Failed to create temp file.\n");
        return;
    }
#else
    int fd;
    strcpy(filename_out, "tmp_file_io_XXXXXX");
    fd = mkstemp(filename_out);
    close(fd);
#endif
}

static void
test_dr_rename_delete(void)
{
    char tmp_src[MAXIMUM_PATH];
    char tmp_dst[MAXIMUM_PATH];
    const char *contents = "abcdefg";
    char contents_buf[100];
    file_t fd;
    ssize_t bytes_read, bytes_left;
    bool success;
    char *cur;

    get_temp_filename(tmp_src);
    get_temp_filename(tmp_dst);
    fd = dr_open_file(tmp_src, DR_FILE_WRITE_OVERWRITE);
    dr_write_file(fd, contents, strlen(contents));
    dr_close_file(fd);

    /* Should fail, dst exists. */
    success = dr_rename_file(tmp_src, tmp_dst, /*replace*/ false);
    if (success)
        dr_fprintf(STDERR, "rename replaced an existing file!\n");

    /* Should succeed. */
    success = dr_rename_file(tmp_src, tmp_dst, /*replace*/ true);
    if (!success)
        dr_fprintf(STDERR, "rename failed to replace existing file!\n");

    /* Contents should match. */
    fd = dr_open_file(tmp_dst, DR_FILE_READ);
    cur = &contents_buf[0];
    bytes_left = BUFFER_SIZE_ELEMENTS(contents_buf);
    while ((bytes_read = dr_read_file(fd, cur, bytes_left)) > 0) {
        cur += bytes_read;
        bytes_left -= bytes_read;
    }
    dr_close_file(fd);
    if (strncmp(contents, contents_buf, strlen(contents)) != 0) {
        dr_fprintf(STDERR,
                   "renamed file contents don't match!\n"
                   " expected: %s\n actual: %s\n",
                   contents, contents_buf);
    }

    /* Rename back should succeed. */
    success = dr_rename_file(tmp_dst, tmp_src, /*replace*/ false);
    if (!success)
        dr_fprintf(STDERR, "rename back to tmp_src failed!\n");

    /* Deleting src should succeed. */
    success = dr_delete_file(tmp_src);
    if (!success)
        dr_fprintf(STDERR, "deleting tmp_src failed!\n");

    /* Try to delete any files still existing. */
    if (dr_file_exists(tmp_src))
        dr_delete_file(tmp_src);
    if (dr_file_exists(tmp_dst))
        dr_delete_file(tmp_dst);
}

static void
test_dir(void)
{
    char cwd[MAXIMUM_PATH];
    char buf[MAXIMUM_PATH];

    if (!dr_get_current_directory(cwd, BUFFER_SIZE_ELEMENTS(cwd)))
        dr_fprintf(STDERR, "failed to get current directory\n");
    dr_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s/newdir", cwd);
    if (!dr_create_dir(buf))
        dr_fprintf(STDERR, "failed to create dir\n");
    if (!dr_directory_exists(buf))
        dr_fprintf(STDERR, "failed to detect dir\n");
    if (!dr_delete_dir(buf))
        dr_fprintf(STDERR, "failed to delete newly created dir\n");
}

static void
test_relative_path(const char *path)
{
    const char *towrite = "test\n";
    file_t fd = dr_open_file(path, DR_FILE_WRITE_OVERWRITE);
    if (fd != INVALID_FILE) {
        dr_write_file(fd, towrite, strlen(towrite));
        dr_close_file(fd);
    } else
        dr_fprintf(STDERR, "failed to open %s\n", path);
    if (!dr_file_exists(path) || !dr_delete_file(path))
        dr_fprintf(STDERR, "failed to delete newly created relative file\n");
}

static void
test_relative(void)
{
    char cwd[MAXIMUM_PATH];
    char buf[MAXIMUM_PATH];
    bool ok;

    if (!dr_get_current_directory(cwd, BUFFER_SIZE_ELEMENTS(cwd)))
        dr_fprintf(STDERR, "failed to get current directory\n");

    test_relative_path("./foo");
    test_relative_path("../foo");
    /* we should be in <build_dir>/suite/tests, so ok to go up two levels */
    test_relative_path("../../foo");

    ok = dr_create_dir("newdir");
    if (!ok)
        dr_fprintf(STDERR, "failed to create dir\n");
    if (!dr_directory_exists("newdir"))
        dr_fprintf(STDERR, "failed to detect dir rel\n");
    dr_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s/newdir", cwd);
    if (!dr_directory_exists(buf))
        dr_fprintf(STDERR, "failed to detect dir abs\n");
    if (!dr_delete_dir("newdir"))
        dr_fprintf(STDERR, "failed to delete newly created dir\n");
}

static void
test_map_exe(void)
{
    /* Test dr_map_executable_file() */
    app_pc base_pc;
    size_t size_full, size_code;

    base_pc = dr_map_executable_file(dr_get_client_path(client_id), 0, &size_full);
    if (base_pc == NULL || size_full == 0)
        dr_fprintf(STDERR, "Failed to map exe\n");
    if (!dr_unmap_executable_file(base_pc, size_full))
        dr_fprintf(STDERR, "Failed to unmap exe\n");

    base_pc = dr_map_executable_file(dr_get_client_path(client_id),
                                     DR_MAPEXE_SKIP_WRITABLE, &size_code);
    if (base_pc == NULL || size_code == 0)
        dr_fprintf(STDERR, "Failed to map exe just code\n");
#ifdef LINUX /* on Windows we always map the whole thing */
    if (size_code >= size_full)
        dr_fprintf(STDERR, "Failed to avoid mapping the data segment\n");
#endif
    if (!dr_unmap_executable_file(base_pc, size_code))
        dr_fprintf(STDERR, "Failed to unmap exe\n");
}

static void
test_times(void)
{
    /* Test time functions */
    uint64 micro = dr_get_microseconds();
    uint64 milli = dr_get_milliseconds();
    uint64 micro2 = dr_get_microseconds();
    dr_time_t drtime;
    if (micro < milli || micro2 < micro)
        dr_fprintf(STDERR, "times are way off\n");
    /* We tried to compare drtime fields with localtime() on UNIX and
     * GetSystemTime() on Windows but it's just too complex to easily compare
     * in a non-flaky manner (i#2041) so we just ensure it doesn't crash.
     */
    dr_get_time(&drtime);
}

static void
test_vfprintf_helper(file_t f, const char *fmt, ...)
{
    va_list ap;
    ssize_t len1, len2;

    va_start(ap, fmt);
    len1 = dr_vfprintf(f, fmt, ap);
    va_end(ap);

    /* check length consistency, because why not. */
    char buf[100];
    va_start(ap, fmt);
    len2 = dr_vsnprintf(buf, BUFFER_SIZE_ELEMENTS(buf), fmt, ap);
    va_end(ap);

    if (len1 != len2 && !(len1 > BUFFER_SIZE_ELEMENTS(buf) && len2 == -1)) {
        dr_fprintf(STDERR, "dr_vfprintf and dr_vsnprintf disagree.\n");
    }
}

static void
test_vfprintf(void)
{
    test_vfprintf_helper(STDERR, "vfprintf check: %d\n", 1234);
}
