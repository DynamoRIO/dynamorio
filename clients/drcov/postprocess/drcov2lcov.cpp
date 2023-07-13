/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* drcov2lcov.cpp
 *
 * Convert client drcov binary format to lcov text format.
 */
/* TODO:
 * - add other coverage: cbr, function, ...
 * - add documentation
 */

#include "dr_api.h"
#include "droption.h"
#include "drcovlib.h"
#include "drsyms.h"
#include "hashtable.h"
#include "dr_frontend.h"
#include <iostream>
#include <vector>

#include "../../common/utils.h"
#undef ASSERT /* we're standalone, so no client assert */

#include <string.h> /* strlen */
#include <stdlib.h> /* malloc */
#include <stdio.h>
#include <limits.h>

#ifdef UNIX
#    include <dirent.h> /* opendir, readdir */
#    include <unistd.h> /* getcwd */
#else
#    include <windows.h>
#    include <direct.h> /* _getcwd */
#    pragma comment(lib, "User32.lib")
#endif

namespace dynamorio {
namespace drcov {
namespace {

using ::dynamorio::droption::DROPTION_FLAG_INTERNAL;
using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;
using ::dynamorio::droption::twostring_t;

#define PRINT(lvl, ...)                                         \
    do {                                                        \
        if (dynamorio::drcov::op_verbose.get_value() >= lvl) {  \
            fprintf(stdout, "[DRCOV2LCOV] INFO(%d):    ", lvl); \
            fprintf(stdout, __VA_ARGS__);                       \
        }                                                       \
    } while (0)

#define WARN(lvl, ...)                                          \
    do {                                                        \
        if (dynamorio::drcov::op_warning.get_value() >= lvl) {  \
            fprintf(stderr, "[DRCOV2LCOV] WARNING(%d): ", lvl); \
            fprintf(stderr, __VA_ARGS__);                       \
        }                                                       \
    } while (0)

#define ASSERT(val, ...)                                  \
    do {                                                  \
        if (!(val)) {                                     \
            fprintf(stderr, "[DRCOV2LCOV] ERROR:      "); \
            fprintf(stderr, __VA_ARGS__);                 \
            exit(1);                                      \
        }                                                 \
    } while (0)

#define DEFAULT_OUTPUT_FILE "coverage.info"

/* Rather than skip these in the client and put them into the unknown module,
 * we give the user a chance to display these if desired.
 * But by default we hide them, as they are confusing in the output.
 * They are present on the app module list for various reasons (xref i#479).
 */
#ifdef WINDOWS
#    define DR_LIB_NAME "dynamorio.dll"
#    define DR_PRELOAD_NAME "preinject.dll"
#    define DRCOV_LIB_NAME "drcov.dll"
/* Often combined with Dr. Memory */
#    define DRMEM_LIB_NAME "drmemorylib.dll"
#else
#    define DR_LIB_NAME "libdynamorio." /* cover .so and .dylib */
#    define DR_PRELOAD_NAME "libdrpreload."
#    define DRCOV_LIB_NAME "libdrcov."
#    define DRMEM_LIB_NAME "libdrmemorylib."
#endif

/***************************************************************************
 * Options
 */

static droption_t<std::string>
    op_input(DROPTION_SCOPE_FRONTEND, "input", "", "Single drcov log file to process",
             "Specifies a single drcov output file for processing.");

static droption_t<std::string>
    op_dir(DROPTION_SCOPE_FRONTEND, "dir", "",
           "Directory with drcov.*.log files to process",
           "Specifies a directory within which all drcov.*.log files will be processed.");

static droption_t<std::string> op_list(
    DROPTION_SCOPE_FRONTEND, "list", "", "Text file listing log files to process",
    "Specifies a text file that contains a list of paths of log files for processing.");

static droption_t<std::string> op_output(DROPTION_SCOPE_FRONTEND, "output",
                                         DEFAULT_OUTPUT_FILE, "Names the output file",
                                         "Specifies the name for the output file.");

static droption_t<std::string> op_test_pattern(
    DROPTION_SCOPE_FRONTEND, "test_pattern", "", "Enable test coverage for this function",
    "Includes test coverage information in the output file (which means that the output "
    "is no longer compatible with lcov).  The test coverage information is based on "
    "matching the function specified in the pattern string.");

static droption_t<std::string> op_mod_filter(
    DROPTION_SCOPE_FRONTEND, "mod_filter", "", "Only include coverage for this library",
    "Requests that coverage information for all libraries and executables whose paths "
    "do not contain the given filter string be excluded from the output. "
    "Only one such filter can be specified.");

static droption_t<std::string> op_mod_skip_filter(
    DROPTION_SCOPE_FRONTEND, "mod_skip_filter", "", "Skip coverage for this library",
    "Requests that coverage information for all libraries and executables whose paths "
    "contain the given filter string be excluded from the output. "
    "Only one such filter can be specified.");

static droption_t<std::string> op_src_filter(
    DROPTION_SCOPE_FRONTEND, "src_filter", "", "Only include coverage for this source",
    "Requests that coverage information for all sources files whose paths do not "
    "contain the given filter string be excluded from the output. "
    "Only one such filter can be specified.");

static droption_t<std::string> op_src_skip_filter(
    DROPTION_SCOPE_FRONTEND, "src_skip_filter", "", "Skip coverage for this source",
    "Requests that coverage information for all sources files whose paths "
    "contain the given filter string be excluded from the output. "
    "Only one such filter can be specified.");

static droption_t<std::string> op_reduce_set(
    DROPTION_SCOPE_FRONTEND, "reduce_set", "", "Output minimal inputs with same coverage",
    "Results in drcov2lcov identifying a smaller set of log files from the inputs that "
    "have the same code coverage as the full set.  The smaller set's file paths are "
    "written to the given output file path.");

static droption_t<twostring_t> op_pathmap(
    DROPTION_SCOPE_FRONTEND, "pathmap", 0, twostring_t("", ""),
    "Map library to local path",
    "Takes two values: the first specifies the library path to look for in each drcov "
    "log file and the second specifies the path to replace it with before looking "
    "for debug information for that library.  Only one path is currently supported.");

static droption_t<bool> op_include_tool(
    DROPTION_SCOPE_FRONTEND, "include_tool_code", false,
    "Include execution of tool itself",
    "Requests that execution from the drcov tool libraries themselves be included in the "
    "coverage output.  Normally such execution is excluded and the output focuses on "
    "the application only.");

static droption_t<bool> op_help(DROPTION_SCOPE_FRONTEND, "help", false,
                                "Print this message", "Prints the usage message.");

static droption_t<unsigned int>
    op_verbose(DROPTION_SCOPE_FRONTEND, "verbose", 1, 0, 64, "Verbosity level",
               "Verbosity level for informational notifications.");

static droption_t<unsigned int>
    op_warning(DROPTION_SCOPE_FRONTEND, "warning", 1, 0, 64, "Warning level",
               "Level for enabling progressively less serious warning messages.");

static droption_t<bool>
    op_help_html(DROPTION_SCOPE_FRONTEND, "help_html", DROPTION_FLAG_INTERNAL, false,
                 "Print usage in html",
                 "For internal use.  Prints option usage in a longer html format.");

static char input_file_buf[MAXIMUM_PATH];
static char input_dir_buf[MAXIMUM_PATH];
static char input_list_buf[MAXIMUM_PATH];
static char output_file_buf[MAXIMUM_PATH];
static char set_file_buf[MAXIMUM_PATH];

static file_t set_log = INVALID_FILE;

/****************************************************************************
 * Utility Functions
 */

static inline const char *
move_to_next_line(const char *ptr)
{
    const char *end = strchr(ptr, '\n');
    if (end == NULL) {
        ptr += strlen(ptr);
    } else {
        for (ptr = end; *ptr == '\n' || *ptr == '\r'; ptr++)
            ; /* do nothing */
    }
    return ptr;
}

/* the path may contain newlines, so we remove them and null terminate it */
static inline void
null_terminate_path(char *path)
{
    size_t len = strlen(path);
    ASSERT(len != 0, "Wrong path length for %s\n", path);
    while (path[len] == '\n' || path[len] == '\r') {
        path[len] = '\0';
        if (len == 0)
            break;
        len--;
    }
}

/****************************************************************************
 * Line-Table Data Structures & Functions
 */

/* Line-Table Design:
 * - A hashtable stores all line tables for each source file.
 * - A line table uses a byte array to store source line exeuction info.
 *   Not knowing the total line number, we alloc one chunk byte array first
 *   and alloc larger chunks when necessary.
 * - Chunks are linked together as a linked-list, with largest chunk at front.
 */

#define LINE_HASH_TABLE_BITS 10
#define LINE_TABLE_INIT_SIZE 1024 /* first chunk holds 1024 lines */
#define LINE_TABLE_INIT_PRINT_BUF_SIZE (16 * 1024)
#define SOURCE_FILE_START_LINE_SIZE (MAXIMUM_PATH + 10) /* "SF:%s\n" */
#define SOURCE_FILE_END_LINE_SIZE 20                    /* "end_of_record\n" */
#define MAX_CHAR_PER_LINE 256 /* large enough to hold the test function name */
#define MAX_LINE_PER_FILE 0x20000

/* the hashtable for all line_table per source file */
static hashtable_t line_htable;
static uint num_line_htable_entries;

enum {
    SOURCE_LINE_STATUS_NONE = 0,  /* not compiled to object file */
    SOURCE_LINE_STATUS_SKIP = -1, /* not executed */
    SOURCE_LINE_STATUS_EXEC = 1,  /* executed */
};

/* i#1465: add unittest case coverage information in drcov */
static const char *cur_test = NULL;
static const char *non_test = "<NON-TEST>"; /* for case like initialization code */
static const char *non_exec = "<NON-EXEC>"; /* not executed code */

/* Not knowing the source file size, we may allocate several chunks per file,
 * and link them together as a linked-list to avoid realloc and copy overhead.
 */
typedef struct _line_chunk_t line_chunk_t;
struct _line_chunk_t {
    uint num_lines; /* the size of the chunk */
    uint first_num; /* the first line number of the chunk */
    uint last_num;  /* the last line number of the chunk */
    union {
        byte *exec;        /* array of the execution info on the line */
        const char **test; /* array of the test name ptr on the line */
    } info;
    line_chunk_t *next;
};

/* A linked-list line table for one source file.
 * The chunk at front holds larger number of lines than all the chunks behind it,
 * which makes the lookup faster by stopping at early chunk.
 */
typedef struct _line_table_t {
    const char *file;
    int num_chunks;
    line_chunk_t *chunk;
} line_table_t;

static line_chunk_t *
line_chunk_alloc(uint num_lines)
{
    line_chunk_t *chunk = (line_chunk_t *)malloc(sizeof(*chunk));
    void *line_info;
    ASSERT(chunk != NULL, "Failed to create line chunk\n");
    chunk->num_lines = num_lines;
    ASSERT(SOURCE_LINE_STATUS_NONE == 0, "SOURCE_LINE_STATUS_NONE is not 0");
    if (op_test_pattern.specified()) {
        /* init with NULL */
        line_info = calloc(num_lines, sizeof(chunk->info.test[0]));
        chunk->info.test = (const char **)line_info;
    } else {
        /* init with SOURCE_LINE_STATUS_NONE */
        line_info = calloc(num_lines, sizeof(chunk->info.exec[0]));
        chunk->info.exec = (byte *)line_info;
    }
    ASSERT(line_info != NULL, "Failed to alloc line info array\n");
    return chunk;
}

static void
line_chunk_free(line_chunk_t *chunk)
{
    if (op_test_pattern.specified())
        free((void *)chunk->info.test); /* cast from "const char **" to "void *" */
    else
        free(chunk->info.exec);
    free(chunk);
}

static char *
line_chunk_print(line_chunk_t *chunk, char *start)
{
    uint i, line_num;
    int res;
    for (i = 0, line_num = chunk->first_num; i < chunk->num_lines; i++, line_num++) {
        res = 0;
        /* only print lines that have test/exec info */
        if (op_test_pattern.specified()) {
            if (chunk->info.test[i] != NULL) {
                /* The output for per-line test coverage is something like:
                 * for code being executed within a test:
                 *   TDNA:52,net::HostResolver_DnsTask_Test::TestBody
                 * for code being executed without a test, e.g. init:
                 *   TDNA:11,<NON-TEST>
                 * for code not being executed:
                 *   TDNA:87,0
                 * Note: the output must agree with the assumption in
                 * third_party/lcov/genhtml about how TDNA is formated.
                 */
                res = dr_snprintf(start, MAX_CHAR_PER_LINE, "TNDA:%u,%s\n", line_num,
                                  chunk->info.test[i] == non_exec ? "0"
                                                                  : chunk->info.test[i]);
            }
        } else {
            if (chunk->info.exec[i] != (byte)SOURCE_LINE_STATUS_NONE) {
                res = dr_snprintf(
                    start, MAX_CHAR_PER_LINE, "DA:%u,%u\n", line_num,
                    chunk->info.exec[i] == (byte)SOURCE_LINE_STATUS_SKIP ? 0 : 1);
            }
        }
        ASSERT(res < MAX_CHAR_PER_LINE && res != -1, "Error on printing\n");
        start += res;
    }
    return start;
}

static char *
line_table_print(line_table_t *line_table, char *start)
{
    int i;
    line_chunk_t **array, *chunk;

    array = (line_chunk_t **)malloc(sizeof(*array) * line_table->num_chunks);
    /* We need print the chunks in reverse order, i.e., lower line number first,
     * so we put them into an array and then print them to avoid recursive call.
     */
    for (chunk = line_table->chunk, i = line_table->num_chunks - 1;
         chunk != NULL && i >= 0; chunk = chunk->next, i--) {
        array[i] = chunk;
    }
    ASSERT(i == -1 && chunk == NULL, "Wrong line-table\n");
    for (i = 0; i < line_table->num_chunks; i++)
        start = line_chunk_print(array[i], start);
    free(array);

    return start;
}

static inline size_t
line_table_print_buf_size(line_table_t *line_table)
{
    /* it is ok to over estimate */
    return (SOURCE_FILE_START_LINE_SIZE +
            /* assume the first chunk hold the largest line number */
            (MAX_CHAR_PER_LINE * line_table->chunk->last_num) +
            SOURCE_FILE_END_LINE_SIZE);
}

static line_table_t *
line_table_create(const char *file)
{
    line_table_t *table = (line_table_t *)malloc(sizeof(*table));
    line_chunk_t *chunk = line_chunk_alloc(LINE_TABLE_INIT_SIZE);
    ASSERT(table != NULL && chunk != NULL, "Failed to alloc line table");
    table->file = file;
    table->chunk = chunk;
    table->num_chunks = 1;
    chunk->first_num = 1;
    chunk->last_num = chunk->first_num + chunk->num_lines - 1;
    chunk->next = NULL;
    PRINT(5, "line table " PFX " added\n", table);
    PRINT(7, "Init chunk %u-%u (%u) for " PFX " @" PFX "\n", chunk->first_num,
          chunk->last_num, chunk->num_lines, table, chunk);
    return table;
}

static void
line_table_delete(void *p)
{
    line_table_t *table = (line_table_t *)p;
    line_chunk_t *chunk, *next;
    PRINT(5, "line table " PFX " delete\n", table);
    for (chunk = table->chunk; chunk != NULL; chunk = next) {
        next = chunk->next;
        line_chunk_free(chunk);
    }
    free(table);
}

static inline void
line_table_add(line_table_t *line_table, uint line, byte status, const char *test_info)
{
    line_chunk_t *chunk = line_table->chunk;

    if (line >= MAX_LINE_PER_FILE) {
        /* We see this and it seems to be erroneous data from the pdb,
         * xref drsym_enumerate_lines() from drsyms.
         */
        WARN(2, "Too large line number %u for %s\n", line, line_table->file);
        return;
    }

    if (line > chunk->last_num) {
        /* XXX: we need lock if we plan to parallelize it */
        uint num_lines;
        line_chunk_t *tmp;
        /* find right size for the new chunk */
        for (num_lines = chunk->last_num * 2; num_lines < line; num_lines *= 2)
            ; /* do nothing */
        num_lines -= chunk->last_num;
        tmp = line_chunk_alloc(num_lines);
        tmp->first_num = chunk->last_num + 1;
        tmp->last_num = tmp->first_num + num_lines - 1;
        tmp->next = chunk;
        chunk = tmp;
        line_table->chunk = chunk;
        line_table->num_chunks++;
        PRINT(7, "New chunk %u-%u (%u) for " PFX " @" PFX "\n", chunk->first_num,
              chunk->last_num, chunk->num_lines, line_table, chunk);
    }

    for (; chunk != NULL; chunk = chunk->next) {
        if (line >= chunk->first_num) {
            ASSERT(line <= chunk->last_num, "Wrong logic");
            if (op_test_pattern.specified()) {
                /* i#1465: add unittest case coverage information in drcov.
                 * Step 3: assocate test info with the source line.
                 */
                if (test_info != NULL) {
                    if ((chunk->info.test[line - chunk->first_num] == NULL) ||
                        /* prefer exec over non-exec */
                        (chunk->info.test[line - chunk->first_num] == non_exec &&
                         test_info != non_exec) ||
                        /* prefer test over non-test */
                        (chunk->info.test[line - chunk->first_num] == non_test &&
                         test_info != non_exec && test_info != non_test))
                        chunk->info.test[line - chunk->first_num] = test_info;
                }
            } else {
                /* If a line has both exec and skip status, we must honor
                 * SOURCE_LINE_STATUS_EXEC, because they may come from different
                 * modules.
                 */
                if (chunk->info.exec[line - chunk->first_num] != status &&
                    chunk->info.exec[line - chunk->first_num] !=
                        (byte)SOURCE_LINE_STATUS_EXEC)
                    chunk->info.exec[line - chunk->first_num] = status;
            }
            return;
        }
    }
}

/****************************************************************************
 * Module Table Data Structure & Functions
 */

#define TEST_HASH_TABLE_BITS 10

#define MODULE_TABLE_IGNORE ((void *)(ptr_int_t)(-1))
#define MIN_LOG_FILE_SIZE 20

/* when use bitmap as bb_table */
#define BITS_PER_BYTE 8
#define BITMAP_INDEX(x) ((x) / BITS_PER_BYTE)
#define BITMAP_OFFSET(x) ((x) % BITS_PER_BYTE)
#define BITMAP_MASK(offs) (1 << (offs))

/* bitmap_set[start_offs][end_offs]: the value that all bits are set
 * from start_offs to end_offs in a byte.
 */
byte bitmap_set[8][8] = {
    { 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff },
    { 0x0, 0x2, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe },
    { 0x0, 0x0, 0x4, 0xc, 0x1c, 0x3c, 0x7c, 0xfc },
    { 0x0, 0x0, 0x0, 0x8, 0x18, 0x38, 0x78, 0xf8 },
    { 0x0, 0x0, 0x0, 0x0, 0x10, 0x30, 0x70, 0xf0 },
    { 0x0, 0x0, 0x0, 0x0, 0x00, 0x20, 0x60, 0xe0 },
    { 0x0, 0x0, 0x0, 0x0, 0x00, 0x00, 0x40, 0xc0 },
    { 0x0, 0x0, 0x0, 0x0, 0x00, 0x00, 0x00, 0x80 },
};
#define BB_TABLE_RANGE_SET 0xff

enum {
    BB_TABLE_ENTRY_INVALID = -1, /* Invalid lookup in bb table */
    BB_TABLE_ENTRY_CLEAR = 0,
    BB_TABLE_ENTRY_SET = 1,
};

typedef struct _module_table_t {
    char *path;
    uintptr_t seg_start;
    size_t seg_offs;
    size_t size;
    union {
        byte *bitmap;        /* store exec info (bit) for each app byte */
        const char **array;  /* store test info (char *) for each app byte */
    } bb_table;              /* data structure storing which bb is seen */
    hashtable_t test_htable; /* hashtable for test functions found in the module */
} module_table_t;

#define MODULE_HASH_TABLE_BITS 6
static std::vector<module_table_t *> module_vec;

static void
module_vec_delete()
{
    for (auto *table : module_vec) {
        PRINT(3, "Delete module table " PFX "\n", table);
        if (table != MODULE_TABLE_IGNORE) {
            free(table->path);
            free(table->bb_table.bitmap);
            if (op_test_pattern.specified())
                hashtable_delete(&table->test_htable);
            free(table);
        }
    }
}

static inline int
bb_bitmap_lookup(module_table_t *table, uint addr)
{
    byte *bm = table->bb_table.bitmap;
    if (bm[BITMAP_INDEX(addr)] == 0xff ||
        TEST(BITMAP_MASK(BITMAP_OFFSET(addr)), bm[BITMAP_INDEX(addr)]))
        return BB_TABLE_ENTRY_SET;
    return BB_TABLE_ENTRY_CLEAR;
}

/* add an entry into a bitmap bb_table */
static inline bool
bb_bitmap_add(module_table_t *table, bb_entry_t *entry)
{
    uint idx, offs, addr_end, idx_end, offs_end, i;
    byte *bm = table->bb_table.bitmap;
    idx = BITMAP_INDEX(entry->start);
    /* we assume that the whole bb is seen if its start addr is seen */
    if (bm[idx] == BB_TABLE_RANGE_SET)
        return false;
    offs = BITMAP_OFFSET(entry->start);
    if (TEST(BITMAP_MASK(offs), bm[idx]))
        return false;
    /* now we add a new bb */
    PRINT(6, "Add 0x%x-0x%x in table " PFX "\n", entry->start, entry->start + entry->size,
          table);
    addr_end = entry->start + entry->size - 1;
    idx_end = BITMAP_INDEX(addr_end);
    offs_end = (idx_end > idx) ? BITS_PER_BYTE - 1 : BITMAP_OFFSET(addr_end);
    /* first byte in the bitmap */
    bm[idx] |= bitmap_set[offs][offs_end];
    /* set all the middle byte */
    for (i = idx + 1; i < idx_end; i++)
        bm[i] = BB_TABLE_RANGE_SET;
    /* last byte in the bitmap */
    if (idx_end > idx) {
        offs_end = BITMAP_OFFSET(addr_end);
        bm[idx_end] |= bitmap_set[0][offs_end];
    }
    return true;
}

static inline int
bb_array_lookup(module_table_t *table, uint offset, const char **info)
{
    const char **ba = table->bb_table.array;
    ASSERT(table->size > offset, "Offset is too large");
    ASSERT(info != NULL, "Info must not be NULL");
    if (ba[offset] != NULL) {
        *info = ba[offset];
        return BB_TABLE_ENTRY_SET;
    }
    *info = non_exec;
    return BB_TABLE_ENTRY_CLEAR;
}

static inline bool
bb_array_add(module_table_t *table, bb_entry_t *entry)
{
    /* i#1465: add unittest case coverage information in drcov.
     * Step 2: assocate bb with test name
     */
    const char **ba = table->bb_table.array;
    char *test_name;
    uint i, offset;
    offset = (uint)entry->start;
    /* we assume that the whole bb is seen if its start addr is seen */
    if (ba[offset] != NULL)
        return false;
    /* check if current bb starts a new test */
    test_name =
        (char *)hashtable_lookup(&table->test_htable, (void *)(ptr_int_t)entry->start);
    if (test_name != NULL) {
        PRINT(6, "start new test %s\n", test_name);
        cur_test = test_name;
    }
    for (i = 0; i < entry->size; i++)
        ba[offset + i] = cur_test;
    return true;
}

static int
module_table_bb_lookup(module_table_t *table, uint64 addr_from_abs_base,
                       const char **info)
{
    if (addr_from_abs_base - table->seg_offs > UINT_MAX)
        return BB_TABLE_ENTRY_INVALID;
    uint addr = (uint)(addr_from_abs_base - table->seg_offs);
    PRINT(5, "lookup 0x%x in module table " PFX "\n", addr, table);
    /* We see this and it seems to be erroneous data from the pdb,
     * xref drsym_enumerate_lines() from drsyms.
     */
    if (table->size <= addr)
        return BB_TABLE_ENTRY_INVALID;
    if (op_test_pattern.specified())
        return bb_array_lookup(table, addr, info);
    else
        return bb_bitmap_lookup(table, addr);
}

static inline bool
module_table_bb_add(module_table_t *table, bb_entry_t *entry)
{
    if (table == MODULE_TABLE_IGNORE)
        return false;
    if (table->size <= entry->start + entry->size) {
        WARN(3, "Wrong range 0x%x-0x%x or table size 0x%zx for table " PFX "\n",
             entry->start, entry->start + entry->size, table->size, table);
        return false;
    }
    if (op_test_pattern.specified())
        return bb_array_add(table, entry);
    else
        return bb_bitmap_add(table, entry);
}

static char *
my_strdup(const char *src)
{
    /* strdup is deprecated on Windows */
    size_t alloc_sz = strlen(src) + 1;
    char *res = (char *)malloc(alloc_sz);
    strncpy(res, src, alloc_sz);
    res[alloc_sz - 1] = '\0';
    return res;
}

static bool
search_cb(drsym_info_t *info, drsym_error_t status, void *data)
{
    module_table_t *table = (module_table_t *)data;
    if (info != NULL && info->name != NULL &&
        strstr(info->name, op_test_pattern.get_value().c_str()) != NULL) {
        char *name = my_strdup(info->name);
        PRINT(5, "function %s: 0x%zx-0x%zx\n", name, info->start_offs, info->end_offs);
        ASSERT(info->start_offs <= table->size, "wrong offset");
        hashtable_add(&table->test_htable, (void *)info->start_offs, name);
    }
    return true; /* continue iteration */
}

static void
module_table_search_testcase(const char *module, module_table_t *table)
{
    drsym_error_t symres;
    uint flags = DRSYM_DEMANGLE | DRSYM_DEMANGLE_PDB_TEMPLATES;

    ASSERT(op_test_pattern.specified(), "should not be called");
    hashtable_init_ex(&table->test_htable, TEST_HASH_TABLE_BITS, HASH_INTPTR,
                      false /* strdup */, false /* !synch */, free /* free */,
                      NULL /* hash */, NULL /* cmp */);
    symres = drsym_module_has_symbols(module);
    if (symres != DRSYM_SUCCESS)
        WARN(1, "Module %s does not have symbols\n", module);
#ifdef WINDOWS
    symres = drsym_search_symbols_ex(module, op_test_pattern.get_value().c_str(), flags,
                                     search_cb, sizeof(drsym_info_t), table);
#else
    symres =
        drsym_enumerate_symbols_ex(module, search_cb, sizeof(drsym_info_t), table, flags);
#endif
    if (symres != DRSYM_SUCCESS)
        WARN(1, "fail to search testcase in module %s\n", module);
}

static module_table_t *
module_table_create(const char *module, uintptr_t seg_start, size_t seg_offs, size_t size)
{
    module_table_t *table;
    ASSERT(ALIGNED(size, dr_page_size()), "Module size is not aligned");

    table = (module_table_t *)calloc(1, sizeof(*table));
    ASSERT(table != NULL, "Failed to allocate module table");
    table->path = my_strdup(module);
    table->seg_start = seg_start;
    table->seg_offs = seg_offs;
    table->size = size;
    PRINT(3, "module table %p, %u\n", table, (uint)size);
    if (op_test_pattern.specified()) {
        /* i#1465: add unittest case coverage information in drcov.
         * Step 1: search test case entries in the module
         */
        /* XXX: for 64-bit, we do calloc of size 8x the module size,
         * and we are doing this calloc for all modules simultaneously,
         * so we might use a huge amount of memory!
         */
        table->bb_table.array =
            (const char **)calloc(size, sizeof(char *) /* test name */);
        ASSERT(table->bb_table.array != NULL, "Failed to create module table");
        module_table_search_testcase(module, table);
    } else {
        /* we use bitmap for bb_table */
        table->bb_table.bitmap = (byte *)calloc(1, size / BITS_PER_BYTE);
        ASSERT(table->bb_table.bitmap != NULL, "Failed to create module table");
    }
    return table;
}

static bool
module_is_from_tool(const char *path)
{
    return (strstr(path, DR_LIB_NAME) != NULL || strstr(path, DR_PRELOAD_NAME) != NULL ||
            strstr(path, DRCOV_LIB_NAME) != NULL || strstr(path, DRMEM_LIB_NAME) != NULL);
}

static const char *
read_module_list(const char *buf, module_table_t ***tables, uint *num_mods)
{
    const char *modpath;
    char subst[MAXIMUM_PATH];
    uint i;
    void *handle;

    PRINT(3, "Reading module table...\n");
    /* module table header */
    if (drmodtrack_offline_read(INVALID_FILE, buf, &buf, &handle, num_mods) !=
        DRCOVLIB_SUCCESS) {
        WARN(1, "Failed to read module table");
        return NULL;
    }

    *tables = (module_table_t **)calloc(*num_mods, sizeof(*tables));
    for (i = 0; i < *num_mods; i++) {
        module_table_t *mod_table;
        drmodtrack_info_t info = {
            sizeof(info),
        };

        if (drmodtrack_offline_lookup(handle, i, &info) != DRCOVLIB_SUCCESS)
            ASSERT(false, "Failed to read module table");
        PRINT(5, "Module: %u, 0x%zx, %s\n", i, info.size, info.path);
        modpath = info.path;
        if (info.size >= UINT_MAX)
            ASSERT(false, "module size is too large");
        /* FIXME i#1445: we have seen the pdb convert paths to all-lowercase,
         * so these should be case-insensitive on Windows.
         */
        if (strstr(info.path, "<unknown>") != NULL ||
            (op_mod_filter.specified() &&
             strstr(info.path, op_mod_filter.get_value().c_str()) == NULL) ||
            (op_mod_skip_filter.specified() &&
             strstr(info.path, op_mod_skip_filter.get_value().c_str()) != NULL) ||
            (!op_include_tool.get_value() && module_is_from_tool(info.path)))
            mod_table = (module_table_t *)MODULE_TABLE_IGNORE;
        else {
            if (op_pathmap.specified()) {
                const char *tofind = op_pathmap.get_value().first.c_str();
                const char *match = strstr(info.path, tofind);
                if (match != NULL) {
                    if (dr_snprintf(subst, BUFFER_SIZE_ELEMENTS(subst), "%.*s%s%s",
                                    match - info.path, info.path,
                                    op_pathmap.get_value().second.c_str(),
                                    match + strlen(tofind)) <= 0) {
                        WARN(1, "Failed to replace %s in %s\n", tofind, info.path);
                    } else {
                        NULL_TERMINATE_BUFFER(subst);
                        PRINT(2, "Substituting |%s| for |%s|\n", subst, info.path);
                        modpath = subst;
                    }
                }
            }
            size_t seg_offs = 0;
            if (info.containing_index != i) {
                ASSERT(info.containing_index <= i, "invalid containing index");
                seg_offs =
                    (uintptr_t)info.start - (*tables)[info.containing_index]->seg_start;
            }
            mod_table =
                module_table_create(modpath, (uintptr_t)info.start, seg_offs, info.size);
        }
        PRINT(4, "Create module table " PFX " for module %s\n", mod_table, modpath);
        module_vec.push_back(mod_table);
        /* XXX: We could just use module_vec in the caller instead of this extra
         * array, now that module_vec is a vector instead of a hashtable.
         */
        (*tables)[i] = mod_table;
    }
    if (drmodtrack_offline_exit(handle) != DRCOVLIB_SUCCESS)
        ASSERT(false, "failed to clean up module table data");
    return buf;
}

static bool
read_bb_list(const char *buf, module_table_t **tables, uint num_mods, uint num_bbs)
{
    uint i;
    bb_entry_t *entry;
    bool add_new_bb = false;

    PRINT(4, "Reading %u basic blocks\n", num_bbs);
    if (op_test_pattern.specified()) {
        /* i#1465: add unittest case coverage information in drcov:
         * reset the current test name to be none
         */
        cur_test = non_test;
    }
    for (i = 0, entry = (bb_entry_t *)buf; i < num_bbs; i++, entry++) {
        PRINT(6, "BB: 0x%x, %u, %u\n", entry->start, entry->size, entry->mod_id);
        /* we could have mod id USHRT_MAX for unknown module e.g., [vdso] */
        if (entry->mod_id < num_mods)
            add_new_bb = module_table_bb_add(tables[entry->mod_id], entry) || add_new_bb;
    }
    free(tables);
    return add_new_bb;
}

static const char *
read_file_header(const char *buf)
{
    char str[MAXIMUM_PATH];
    uint version;

    PRINT(3, "Reading file header...\n");
    /* version number */
    /* XXX i#1842: we're violating abstraction barriers here with hardcoded
     * file format strings.  drcovlib should either have a formal file format
     * description in its header, or it should provide API routines to read
     * the file fields.
     */
    PRINT(4, "Reading version number\n");
    if (dr_sscanf(buf, "DRCOV VERSION: %u\n", &version) != 1) {
        WARN(1, "Failed to read version number");
        return NULL;
    }
    if (version != DRCOV_VERSION) {
        if (version == DRCOV_VERSION_MODULE_OFFSETS) {
            WARN(1,
                 "File is in legacy version 2 format: only code in the first "
                 "segment of each module will be reported\n");
        } else {
            WARN(1, "Version mismatch: file version %d vs tool version %d\n", version,
                 DRCOV_VERSION);
            return NULL;
        }
    }
    buf = move_to_next_line(buf);

    /* flavor */
    PRINT(4, "Reading flavor\n");
    if (dr_sscanf(buf, "DRCOV FLAVOR: %[^\n\r]\n", str) != 1) {
        WARN(1, "Failed to read version number");
        return NULL;
    }
    if (strcmp(str, DRCOV_FLAVOR) != 0) {
        WARN(1, "Fatal file mismatch: file %s vs tool %s\n", str, DRCOV_FLAVOR);
        return NULL;
    }
    buf = move_to_next_line(buf);

    return buf;
}

static file_t
open_input_file(const char *fname, const char **map_out OUT, size_t *map_size OUT,
                uint64 *file_sz OUT)
{
    uint64 file_size;
    char *map;
    file_t f;

    ASSERT(map_out != NULL && map_size != NULL, "map_out must not be NULL");
    f = dr_open_file(fname, DR_FILE_READ | DR_FILE_ALLOW_LARGE);
    if (f == INVALID_FILE) {
        WARN(1, "Failed to open file %s\n", fname);
        return INVALID_FILE;
    }
    if (!dr_file_size(f, &file_size)) {
        WARN(1, "Failed to get input file size for %s\n", fname);
        dr_close_file(f);
        return INVALID_FILE;
    }
    if (file_size <= MIN_LOG_FILE_SIZE) {
        WARN(1, "File size is 0 for %s\n", fname);
        dr_close_file(f);
        return INVALID_FILE;
    }
    *map_size = (size_t)file_size;
    map = (char *)dr_map_file(f, map_size, 0, NULL, DR_MEMPROT_READ, 0);
    if (map == NULL || (size_t)file_size > *map_size) {
        WARN(1, "Failed to map file %s\n", fname);
        dr_close_file(f);
        return INVALID_FILE;
    }
    *map_out = map;
    if (file_sz != NULL)
        *file_sz = file_size;
    return f;
}

static void
close_input_file(file_t f, const char *map, size_t map_size)
{
    dr_unmap_file((char *)map, map_size);
    dr_close_file(f);
}

static bool
read_drcov_file(const char *input)
{
    file_t log;
    const char *map, *ptr;
    size_t map_size;
    module_table_t **tables;
    uint num_mods, num_bbs;
    bool res;

    PRINT(2, "Reading drcov log file: %s\n", input);
    log = open_input_file(input, &map, &map_size, NULL);
    if (log == INVALID_FILE) {
        WARN(1, "Failed to read drcov log file %s\n", input);
        return false;
    }
    ptr = read_file_header(map);
    if (ptr == NULL) {
        WARN(1, "Invalid version or bitwidth in drcov log file %s\n", input);
        return false;
    }

    ptr = read_module_list(ptr, &tables, &num_mods);
    if (ptr == NULL)
        return false;

    if (dr_sscanf(ptr, "BB Table: %u bbs\n", &num_bbs) != 1) {
        WARN(1, "Failed to read bb list from %s\n", input);
        return false;
    }
    ptr = move_to_next_line(ptr);
    if (num_bbs * sizeof(bb_entry_t) > map_size) {
        WARN(1, "Wrong number of bbs, corrupt log file %s\n", input);
        close_input_file(log, map, map_size);
        return false;
    }
    res = read_bb_list(ptr, tables, num_mods, num_bbs);
    if (res && set_log != INVALID_FILE)
        dr_fprintf(set_log, "%s\n", input);
    close_input_file(log, map, map_size);
    return true;
}

static inline bool
is_drcov_log_file(const char *fname)
{
    if (((fname[0] == 'd' && fname[1] == 'r') ||
         /* legacy data files before rebranding */
         (fname[0] == 'b' && fname[1] == 'b')) &&
        fname[2] == 'c' && fname[3] == 'o' && fname[4] == 'v' && fname[5] == '.' &&
        strstr(fname, ".log") != NULL)
        return true;
    return false;
}

#ifdef UNIX
static bool
read_drcov_dir(void)
{
    DIR *dir;
    struct dirent *ent;
    char path[MAXIMUM_PATH];
    bool found_logs = false;

    PRINT(2, "Reading input directory %s\n", input_dir_buf);
    if ((dir = opendir(input_dir_buf)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (is_drcov_log_file(ent->d_name)) {
                ASSERT(ent->d_name[0] != '/',
                       "ent->d_name: %s should not be an absolute path\n", ent->d_name);
                if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s/%s", input_dir_buf,
                                ent->d_name) <= 0) {
                    WARN(1, "Fail to get full path of log file %s\n", ent->d_name);
                } else {
                    NULL_TERMINATE_BUFFER(path);
                    read_drcov_file(path);
                    found_logs = true;
                }
            }
        }
        closedir(dir);
    } else {
        /* could not open directory */
        WARN(1, "Failed to open directory %s\n", input_dir_buf);
        return false;
    }
    if (!found_logs)
        WARN(1, "Failed to find log files in dir %s\n", input_dir_buf);
    return found_logs;
}
#else
static bool
read_drcov_dir(void)
{
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA ffd;
    char path[MAXIMUM_PATH];
    bool has_sep;
    bool found_logs = false;

    /* append \* to the end */
    strcpy(path, input_dir_buf);
    if (path[strlen(path) - 1] == '\\') {
        strcat(path, "*");
        has_sep = true;
    } else {
        strcat(path, "\\*");
        has_sep = false;
    }
    PRINT(2, "Reading input directory %s\n", path);
    hFind = FindFirstFile(path, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        ASSERT(false, "Failed to read input directory %s\n", path);
        return false;
    }
    do {
        if (!TESTANY(ffd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY) &&
            is_drcov_log_file(ffd.cFileName)) {
            strcpy(path, input_dir_buf);
            if (!has_sep)
                strcat(path, "\\");
            strcat(path, ffd.cFileName);
            found_logs = read_drcov_file(path) || found_logs;
        }
    } while (FindNextFile(hFind, &ffd) != 0);
    FindClose(hFind);
    if (!found_logs)
        WARN(1, "Failed to find log files in dir %s\n", input_dir_buf);
    return found_logs;
}
#endif

static bool
read_drcov_list(void)
{
    file_t list;
    const char *map, *ptr;
    char path[MAXIMUM_PATH];
    size_t map_size;
    uint64 file_size;
    bool found_logs = false;

    PRINT(2, "Reading list %s\n", input_list_buf);
    list = open_input_file(input_list_buf, &map, &map_size, &file_size);
    if (list == INVALID_FILE) {
        WARN(1, "Failed to read list %s\n", input_list_buf);
        return false;
    }
    /* process each file in the list */
    for (ptr = map; ptr < map + file_size;) {
        if (dr_sscanf(ptr, "%s\n", path) != 1)
            break;
        NULL_TERMINATE_BUFFER(path);
        ptr = move_to_next_line(ptr);
        null_terminate_path(path);
        found_logs = read_drcov_file(path) || found_logs;
    }
    close_input_file(list, map, map_size);
    if (!found_logs)
        WARN(1, "Failed to find log files on list %s\n", input_list_buf);
    return found_logs;
}

static bool
read_drcov_input(void)
{
    bool res = true;
    if (op_input.specified())
        res = read_drcov_file(input_file_buf) && res;
    if (op_list.specified())
        res = read_drcov_list() && res;
    if (op_dir.specified())
        res = read_drcov_dir() && res;
    return res;
}

static bool
enum_line_cb(drsym_line_info_t *info, void *data)
{
    int status;
    module_table_t *table = (module_table_t *)data;
    line_table_t *line_table;
    const char *test_info = NULL;
    /* FIXME i#1445: we have seen the pdb convert paths to all-lowercase,
     * so these should be case-insensitive on Windows.
     */
    if (info->file == NULL ||
        (op_src_filter.specified() &&
         strstr(info->file, op_src_filter.get_value().c_str()) == NULL) ||
        (op_src_skip_filter.specified() &&
         strstr(info->file, op_src_skip_filter.get_value().c_str()) != NULL))
        return true;
    line_table = (line_table_t *)hashtable_lookup(&line_htable, (void *)info->file);
    if (line_table == NULL) {
        num_line_htable_entries++;
        line_table = line_table_create(info->file);
        if (!hashtable_add(&line_htable, (void *)info->file, line_table))
            ASSERT(false, "Failed to add new source line table");
    }
    status = module_table_bb_lookup(table, info->line_addr, &test_info);
    /* info->line is uint64 */
    ASSERT((uint)info->line == info->line, "info->line is too large");
    if (status == BB_TABLE_ENTRY_SET) {
        PRINT(5, "exec: ");
        line_table_add(line_table, (uint)info->line, (byte)SOURCE_LINE_STATUS_EXEC,
                       test_info);
    } else if (status == BB_TABLE_ENTRY_CLEAR) {
        PRINT(5, "skip: ");
        line_table_add(line_table, (uint)info->line, (byte)SOURCE_LINE_STATUS_SKIP,
                       test_info);
    } else {
        WARN(2, "Invalid bb lookup, Table: " PFX ", Addr: " PIFX "\n", table,
             IF_NOT_X64((uint)) info->line);
    }
    PRINT(5, "%s, %s, %llu, 0x%zx\n", info->cu_name, info->file,
          (unsigned long long)info->line, info->line_addr);
    return true;
}

static bool
enumerate_line_info(void)
{
    /* iterate module table */
    for (const auto *mod_table : module_vec) {
        if (mod_table == MODULE_TABLE_IGNORE)
            continue;
        if (strcmp(mod_table->path, "<unknown>") == 0)
            continue;
        bool has_lines = true;
        PRINT(3, "Enumerate line info for %s\n", mod_table->path);
        drsym_error_t res =
            drsym_enumerate_lines(mod_table->path, enum_line_cb, (void *)mod_table);
        if (res != DRSYM_SUCCESS) {
            WARN(1, "Failed to enumerate lines for %s\n", mod_table->path);
            has_lines = false;
        }
        res = drsym_free_resources(mod_table->path);
        /* I'm using has_lines to avoid warning on vdso. */
        if (res != DRSYM_SUCCESS && has_lines)
            WARN(1, "Failed to free resource for %s\n", mod_table->path);
    }
    return true;
}

/****************************************************************************
 * Output
 */

static int
compare_source_file(const void *a_in, const void *b_in)
{
    hash_entry_t *e1 = *(hash_entry_t **)a_in;
    hash_entry_t *e2 = *(hash_entry_t **)b_in;
    return strcmp((const char *)e1->key, (const char *)e2->key);
}

static bool
write_lcov_output(void)
{
    file_t log;
    uint i, num_entries = 0;
    hash_entry_t *e;
    hash_entry_t **src_array;
    char *buf, *ptr;
    size_t buf_sz;

    PRINT(2, "Writing output lcov file: %s\n", output_file_buf);
    log = dr_open_file(output_file_buf, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    if (log == INVALID_FILE) {
        ASSERT(false, "Failed to open output file %s\n", output_file_buf);
        return false;
    }

    /* sort them before print */
    src_array = (hash_entry_t **)calloc(line_htable.entries, sizeof(src_array[0]));
    for (i = 0; i < HASHTABLE_SIZE(line_htable.table_bits); i++) {
        for (e = line_htable.table[i]; e != NULL; e = e->next) {
            src_array[num_entries] = e;
            num_entries++;
        }
    }
    ASSERT(num_entries == num_line_htable_entries && line_htable.entries == num_entries,
           "Wrong number of hashtable entries");
    qsort(src_array, num_entries, sizeof(src_array[0]), compare_source_file);

    /* print */
    buf_sz = LINE_TABLE_INIT_PRINT_BUF_SIZE;
    buf = (char *)malloc(buf_sz);
    ASSERT(buf != NULL, "Failed to alloc print buffer\n");
    for (i = 0; i < num_entries; i++) {
        e = src_array[i];
        PRINT(4, "Writing coverage info for %s\n", (char *)e->key);
        if (line_table_print_buf_size((line_table_t *)e->payload) >= buf_sz) {
            free(buf);
            buf_sz = line_table_print_buf_size((line_table_t *)e->payload);
            buf = (char *)malloc(buf_sz);
            ASSERT(buf != NULL, "Failed to alloc print buffer\n");
        }
        ptr = buf;
        ptr += dr_snprintf(ptr, SOURCE_FILE_START_LINE_SIZE, "SF:%s\n", e->key);
        ptr = line_table_print((line_table_t *)e->payload, ptr);
        ptr += dr_snprintf(ptr, SOURCE_FILE_END_LINE_SIZE, "end_of_record\n");
        dr_write_file(log, buf, ptr - buf);
    }
    free(buf);
    free(src_array);
    dr_close_file(log);
    return true;
}

/****************************************************************************
 * Options Handling
 */

static void
print_usage()
{
    fprintf(stderr, "drcov2lcov: convert drcov file format to lcov file format\n");
    fprintf(stderr, "usage: drcov2lcov [options]\n%s",
            droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
}

static bool
option_init(int argc, const char *argv[])
{
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, argv, &parse_err,
                                       NULL)) {
        WARN(0, "Usage error: %s\n", parse_err.c_str());
        print_usage();
        return false;
    }
    if (op_help_html.specified()) {
        std::cout << droption_parser_t::usage_long(DROPTION_SCOPE_ALL, "- <b>", "</b>\n",
                                                   "  <br><i>", "</i>\n", "  <br>", "\n")
                  << std::endl;
        exit(0);
    }
    if (op_help.specified() ||
        (!op_input.specified() && !op_dir.specified() && !op_list.specified())) {
        print_usage();
        return false;
    }

    if (op_input.specified()) {
        if (drfront_get_absolute_path(op_input.get_value().c_str(), input_file_buf,
                                      BUFFER_SIZE_ELEMENTS(input_file_buf)) !=
            DRFRONT_SUCCESS) {
            WARN(1, "Failed to get full path of input file %s\n",
                 op_input.get_value().c_str());
            return false;
        }
        NULL_TERMINATE_BUFFER(input_file_buf);
        PRINT(2, "Input file: %s\n", input_file_buf);
    }

    if (op_list.specified()) {
        if (drfront_get_absolute_path(op_list.get_value().c_str(), input_list_buf,
                                      BUFFER_SIZE_ELEMENTS(input_list_buf)) !=
            DRFRONT_SUCCESS) {
            WARN(1, "Failed to get full path of input list %s\n",
                 op_list.get_value().c_str());
            return false;
        }
        NULL_TERMINATE_BUFFER(input_list_buf);
        PRINT(2, "Input list: %s\n", input_list_buf);
    }

    if (op_dir.specified() || (!op_input.specified() && !op_list.specified())) {
        std::string input_dir; /* don't use a local char* as c_str() will evaporate */
        if (!op_dir.specified()) {
            WARN(1, "Missing input, using current directory instead\n");
            input_dir = "./";
        } else
            input_dir = op_dir.get_value();
        if (drfront_get_absolute_path(input_dir.c_str(), input_dir_buf,
                                      BUFFER_SIZE_ELEMENTS(input_dir_buf)) !=
            DRFRONT_SUCCESS) {
            WARN(1, "Failed to get full path of input dir |%s|\n", input_dir.c_str());
            return false;
        }
        NULL_TERMINATE_BUFFER(input_dir_buf);
        PRINT(2, "Input dir: %s\n", input_dir_buf);
    }

    if (!op_output.specified())
        WARN(1, "No output file name specified: using default %s\n", DEFAULT_OUTPUT_FILE);
    if (drfront_get_absolute_path(
            !op_output.specified() ? DEFAULT_OUTPUT_FILE : op_output.get_value().c_str(),
            output_file_buf, BUFFER_SIZE_ELEMENTS(output_file_buf)) != DRFRONT_SUCCESS) {
        WARN(1, "Failed to get full path of output file\n");
        return false;
    }
    NULL_TERMINATE_BUFFER(output_file_buf);
    PRINT(2, "Output file: %s\n", output_file_buf);

    if (op_reduce_set.specified()) {
        if (drfront_get_absolute_path(op_reduce_set.get_value().c_str(), set_file_buf,
                                      BUFFER_SIZE_ELEMENTS(set_file_buf)) !=
            DRFRONT_SUCCESS) {
            WARN(1, "Failed to get full path of reduce_set file\n");
            return false;
        }
        NULL_TERMINATE_BUFFER(set_file_buf);
        PRINT(2, "Reduced set file: %s\n", set_file_buf);
        set_log = dr_open_file(set_file_buf, DR_FILE_WRITE_REQUIRE_NEW);
        if (set_log == INVALID_FILE) {
            ASSERT(false, "Failed to open reduce set output file %s\n", set_file_buf);
            return false;
        }
    }
    return true;
}

} // namespace
} // namespace drcov
} // namespace dynamorio

/****************************************************************************
 * Main Function
 */

int
main(int argc, const char *argv[])
{
    if (!dynamorio::drcov::option_init(argc, argv))
        return 1;

    dr_standalone_init();
    if (drsym_init(IF_WINDOWS_ELSE(NULL, 0)) != DRSYM_SUCCESS) {
        ASSERT(false, "Unable to initialize symbol translation");
        return 1;
    }
    hashtable_init_ex(&dynamorio::drcov::line_htable, LINE_HASH_TABLE_BITS, HASH_STRING,
                      true /* strdup */, false /* !synch */,
                      dynamorio::drcov::line_table_delete /* free */, NULL /* hash */,
                      NULL /* cmp */);

    PRINT(1, "Reading input files...\n");
    if (!dynamorio::drcov::read_drcov_input()) {
        ASSERT(false, "Failed to read input files\n");
        return 1;
    }

    PRINT(1, "Enumerating line info...\n");
    if (!dynamorio::drcov::enumerate_line_info()) {
        ASSERT(false, "Failed to enumerate line info\n");
        return 1;
    }

    PRINT(1, "Writing output file...\n");
    if (!dynamorio::drcov::write_lcov_output()) {
        ASSERT(false, "Failed to write output file\n");
        return 1;
    }

    dynamorio::drcov::module_vec_delete();
    hashtable_delete(&dynamorio::drcov::line_htable);
    if (drsym_exit() != DRSYM_SUCCESS) {
        ASSERT(false, "Failed to clean up symbol library\n");
        return 1;
    }
    if (dynamorio::drcov::set_log != INVALID_FILE)
        dr_close_file(dynamorio::drcov::set_log);
    dr_standalone_exit();
    return 0;
}
