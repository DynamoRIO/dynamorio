/* **********************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
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

/* drcov2lcov.c
 *
 * Covert client drcov binary format to lcov text format.
 */
/* TODO:
 * - add other coverage: cbr, function, ...
 * - add documentation
 */

#include "dr_api.h"
#include "drcov.h"
#include "drsyms.h"
#include "hashtable.h"

#include "../common/utils.h"
#undef ASSERT /* we're standalone, so no client assert */

#include <string.h> /* strlen */
#include <stdlib.h> /* malloc */
#include <stdio.h>
#include <limits.h>

#ifdef UNIX
# include <dirent.h> /* opendir, readdir */
# include <unistd.h> /* getcwd */
#else
# include <windows.h>
# include <direct.h> /* _getcwd */
# pragma comment(lib, "User32.lib")
#endif

static int verbose = 1;
static int warning = 1;

#define PRINT(lvl, ...) do {                                \
    if (verbose >= lvl) {                                   \
        fprintf(stdout, "[DRCOV2LCOV] INFO(%d):    ", lvl); \
        fprintf(stdout, __VA_ARGS__);                       \
    }                                                       \
} while (0)

#define WARN(lvl, ...) do {                                 \
    if (warning >= lvl) {                                   \
        fprintf(stderr, "[DRCOV2LCOV] WARNING(%d): ", lvl); \
        fprintf(stderr, __VA_ARGS__);                       \
    }                                                       \
} while (0)

#define ASSERT(val, ...) do {                               \
    if (!(val)) {                                           \
        fprintf(stderr, "[DRCOV2LCOV] ERROR:      ");       \
        fprintf(stderr, __VA_ARGS__);                       \
        exit(1);                                            \
    }                                                       \
} while (0)

const char *usage_str = 
    "drcov2lcov: covert drcov file format to lcov file format\n"
    "usage: drcov2lcov [options]\n"
    "      --help                          Print this message.\n"
    "      --verbose <int>                 Verbose level.\n"
    "      --warning <int>                 Warning level.\n"
    "      --list <input list file>        The file with a list of drcov files to be processed.\n"
    "      --dir <input directory>         The directory with all drcov.*.log files to be processed.\n"
    "      --output <output file>          The output file.\n"
    "      --mod_filter <module filter>    Only process the module whose path contains the filter string.\n"
    "      --src_filter <source filter>    Only process the source file whose path contains the filter string.\n"
    "      --reduce_set <reduce_set file>  Find a smaller set of log files from the inputs that have the same code coverage and write those file paths into <reduce_set file>.\n";

static char input_dir_buf[MAXIMUM_PATH];
static char input_list_buf[MAXIMUM_PATH];
static char output_file_buf[MAXIMUM_PATH];
static char set_file_buf[MAXIMUM_PATH];
static char *input_list;
static char *input_dir;
static char *output_file;
static char *src_filter;
static char *mod_filter;
static char *set_file;
static file_t set_log = INVALID_FILE;

/****************************************************************************
 * Utility Functions
 */

static inline char *
move_to_next_line(char *ptr)
{
    char *end = strchr(ptr, '\n');
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

#ifdef UNIX
/* XXX: i#1079, the code is copied from drdeploy.c, we should share them
 * via a front-end lib.
 * Simply concatenates the cwd with the given relative path.  Previously we
 * called realpath, but this requires the path to exist and expands symlinks,
 * which is inconsistent with Windows GetFullPathName().
 */
static int
GetFullPathName(const char *rel, size_t abs_len, char *abs, char **ext)
{
    size_t len = 0;
    ASSERT(ext == NULL, "invalid param");
    if (rel[0] != '/') {
        char *err = getcwd(abs, abs_len);
        if (err != NULL) {
            len = strlen(abs);
            /* Append a slash if it doesn't have a trailing slash. */
            if (abs[len-1] != '/' && len < abs_len) {
                abs[len++] = '/';
                abs[len] = '\0';
            }
            /* Omit any leading ./. */
            if (rel[0] == '.' && rel[0] == '/') {
                rel += 2;
            }
        }
    }
    strncpy(abs + len, rel, abs_len - len);
    abs[abs_len-1] = '\0';
    return strlen(abs);
}
#endif

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

#define LINE_HASH_TABLE_BITS   10
#define LINE_TABLE_INIT_SIZE 1024 /* first chunk holds 1024 lines */
#define LINE_TABLE_INIT_PRINT_BUF_SIZE (4*PAGE_SIZE)
#define SOURCE_FILE_START_LINE_SIZE (MAXIMUM_PATH + 10) /* "SF:%s\n" */
#define SOURCE_FILE_END_LINE_SIZE   20 /* "end_of_record\n" */
#define MAX_CHAR_PER_LINE (3/*DA:*/+10/*line_no*/+1/*.*/+1/*0/1*/+1/*\n*/)
#define MAX_LINE_PER_FILE 0x20000

/* the hashtable for all line_table per source file */
static hashtable_t line_htable;
static uint num_line_htable_entries;

enum {
    SOURCE_LINE_STATUS_SKIP   = 0, /* not executed */
    SOURCE_LINE_STATUS_EXEC   = 1, /* executed */
    SOURCE_LINE_STATUS_NONE   = 2, /* not compiled to object file */
};

/* Not knowing the source file size, we may allocate several chunks per file,
 * and link them together as a linked-list to avoid realloc and copy overhead.
 */
typedef struct _line_chunk_t line_chunk_t;
struct _line_chunk_t {
    uint num_lines;   /* the size of the chunk */
    uint first_num;   /* the first line number of the chunk */
    uint last_num;    /* the last line number of the chunk */
    byte *line_info;  /* byte array the line execution info */
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
    line_chunk_t *chunk = malloc(sizeof(*chunk));
    size_t size = num_lines * sizeof(chunk->line_info[0]);
    ASSERT(chunk != NULL, "Failed to create line chunk\n");
    chunk->num_lines = num_lines;
    chunk->line_info = malloc(size);
    memset(chunk->line_info, SOURCE_LINE_STATUS_NONE, size);
    ASSERT(chunk->line_info != NULL, "Failed to alloc line array\n");
    return chunk;
}

static void
line_chunk_free(line_chunk_t *chunk)
{
    free(chunk->line_info);
    free(chunk);
}

/* We should only have a very small number of chunks (O(log(n)) per file,
 * so it is ok to use recursive call for printing.
 */
static char *
line_chunk_print(line_chunk_t *chunk, char *start)
{
    uint i, line_num;
    int res;
    for (i = 0, line_num = chunk->first_num;
         i < chunk->num_lines;
         i++, line_num++) {
        if (chunk->line_info[i] != SOURCE_LINE_STATUS_NONE) {
            res = dr_snprintf(start, MAX_CHAR_PER_LINE,
                              "DA:%u,%u\n", line_num, chunk->line_info[i]);
            ASSERT(res < MAX_CHAR_PER_LINE && res != -1, "Error on printing\n");
            start += res;
        }
    }
    return start;
}

static char *
line_table_print(line_table_t *line_table, char *start)
{
    int i;
    line_chunk_t **array, *chunk;

    array = malloc(sizeof(*array) * line_table->num_chunks);
    /* We need print the chunks in reverse order, i.e., lower line number first,
     * so we put them into an array and then print them to avoid recursive call.
     */
    for (chunk  = line_table->chunk, i = line_table->num_chunks - 1;
         chunk != NULL && i >= 0;
         chunk  = chunk->next, i--) {
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

static void *
line_table_create(const char *file)
{
    line_table_t *table = malloc(sizeof(*table));
    line_chunk_t *chunk = line_chunk_alloc(LINE_TABLE_INIT_SIZE);
    ASSERT(table != NULL && chunk != NULL, "Failed to alloc line table");
    table->file       = file;
    table->chunk      = chunk;
    table->num_chunks = 1;
    chunk->first_num  = 1;
    chunk->last_num   = chunk->first_num + chunk->num_lines - 1;
    chunk->next       = NULL;
    PRINT(5, "line table "PFX" added\n", (ptr_uint_t)table);
    PRINT(7, "Init chunk %u-%u (%u) for "PFX" @"PFX"\n",
          chunk->first_num, chunk->last_num, chunk->num_lines,
          (ptr_uint_t)table, (ptr_uint_t)chunk);
    return table;
}

static void
line_table_delete(void *p)
{
    line_table_t *table = (line_table_t *)p;
    line_chunk_t *chunk, *next;
    PRINT(5, "line table "PFX" delete\n", (ptr_uint_t)table);
    for (chunk = table->chunk; chunk != NULL; chunk = next) {
        next = chunk->next;
        line_chunk_free(chunk);
    }
    free(table);
}

static inline void
line_table_add(line_table_t *line_table, uint64 line, int status)
{
    line_chunk_t *chunk = line_table->chunk;
    /* We see this and it seems to be erroneous data from the pdb,
     * xref drsym_enumerate_lines() from drsyms.
     */
    if (line >= MAX_LINE_PER_FILE) {
        WARN(2, "Too large line number %u for %s\n",
             (uint)line, line_table->file);
        return;
    }
    if (line > chunk->last_num) {
        /* XXX: we need lock if we plan to parallelize it */
        uint num_lines;
        line_chunk_t *tmp;
        /* find right size for the new chunk */
        for (num_lines = chunk->last_num * 2; num_lines < line; num_lines *= 2)
            ; /* do nothing */
        num_lines        -= chunk->last_num;
        tmp               = line_chunk_alloc(num_lines);
        tmp->first_num    = chunk->last_num + 1;
        tmp->last_num     = tmp->first_num + num_lines - 1;
        tmp->next         = chunk;
        chunk             = tmp;
        line_table->chunk = chunk;
        line_table->num_chunks++;
        PRINT(7, "New chunk %u-%u (%u) for "PFX" @"PFX"\n",
              chunk->first_num, chunk->last_num, chunk->num_lines,
              (ptr_uint_t)line_table, (ptr_uint_t)chunk);
    }
    for (; chunk != NULL; chunk = chunk->next) {
        if (line >= chunk->first_num) {
            ASSERT(line <= chunk->last_num, "Wrong logic");
            /* If a line have both exec and skip status, we must honor
             * SOURCE_LINE_STATUS_EXEC, because they may come from different
             * modules.
             */
            if (chunk->line_info[line - chunk->first_num] != (byte)status &&
                chunk->line_info[line - chunk->first_num] !=
                (byte)SOURCE_LINE_STATUS_EXEC)
                chunk->line_info[line - chunk->first_num]  = (byte)status;
            return;
        }
    }
}

/****************************************************************************
 * Basic Block Table Data Structure & Functions
 */

#define MODULE_HASH_TABLE_BITS 6
static hashtable_t module_htable;
static uint num_module_htable_entries;

#define BB_TABLE_IGNORE  ((void *)(ptr_int_t)(-1))
#define MIN_LOG_FILE_SIZE 20

/* we use bitmap as bb_table */
#define BITS_PER_BYTE        8
#define BITMAP_INDEX(x)      ((x) / BITS_PER_BYTE)
#define BITMAP_OFFSET(x)     ((x) % BITS_PER_BYTE)
#define BITMAP_MASK(offs)    (1 << (offs))

/* bitmap_set[start_offs][end_offs]: the value that all bits are set
 * from start_offs to end_offs in a byte.
 */
byte bitmap_set[8][8] = {
    { 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff},
    { 0x0, 0x2, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe},
    { 0x0, 0x0, 0x4, 0xc, 0x1c, 0x3c, 0x7c, 0xfc},
    { 0x0, 0x0, 0x0, 0x8, 0x18, 0x38, 0x78, 0xf8},
    { 0x0, 0x0, 0x0, 0x0, 0x10, 0x30, 0x70, 0xf0},
    { 0x0, 0x0, 0x0, 0x0, 0x00, 0x20, 0x60, 0xe0},
    { 0x0, 0x0, 0x0, 0x0, 0x00, 0x00, 0x40, 0xc0},
    { 0x0, 0x0, 0x0, 0x0, 0x00, 0x00, 0x00, 0x80},
};
#define BB_TABLE_RANGE_SET 0xff

enum {
    BB_TABLE_ENTRY_INVALID = -1, /* Invalid lookup in bb table */
    BB_TABLE_ENTRY_CLEAR   = 0,
    BB_TABLE_ENTRY_SET     = 1,
};

typedef struct _bb_table_t {
    uint size;
    byte bm[1];
} bb_table_t;

static void *
bb_table_create(uint mod_size)
{
    bb_table_t *table;
    ASSERT(ALIGNED(mod_size, BITS_PER_BYTE), "Module size is not aligned");

    table = (bb_table_t *)
        calloc(1, sizeof(uint) + (size_t)mod_size/BITS_PER_BYTE);
    PRINT(3, "bb table %p, %u\n", table, mod_size/BITS_PER_BYTE);
    ASSERT(table != NULL, "Failed to create bb table");
    table->size = mod_size;
    return table;
}

static void
bb_table_delete(void *p)
{
    PRINT(3, "Delete bb table "PFX"\n", (ptr_uint_t)p);
    if (p != BB_TABLE_IGNORE)
        free(p);
}

static inline int
bb_table_lookup(bb_table_t *table, uint addr)
{
    byte *bm = table->bm;
    PRINT(5, "lookup "PFX" in bb table "PFX"\n",
          (ptr_uint_t)addr, (ptr_uint_t)table);
    /* We see this and it seems to be erroneous data from the pdb,
     * xref drsym_enumerate_lines() from drsyms.
     */
    if (table->size <= addr)
        return BB_TABLE_ENTRY_INVALID;
    if (bm[BITMAP_INDEX(addr)] == 0xff ||
        TEST(BITMAP_MASK(BITMAP_OFFSET(addr)), bm[BITMAP_INDEX(addr)]))
        return BB_TABLE_ENTRY_SET;
    return BB_TABLE_ENTRY_CLEAR;
}

static inline bool
bb_table_add(bb_table_t *table, bb_entry_t *entry)
{
    byte *bm;
    uint idx, offs, addr_end, idx_end, offs_end, i;
    if (table == BB_TABLE_IGNORE)
        return false;
    if (table->size <= entry->start + entry->size) {
        WARN(3, "Wrong range "PFX"-"PFX" or table size "PFX" for table "PFX"\n",
             (ptr_uint_t)entry->start,
             (ptr_uint_t)entry->start + entry->size,
             (ptr_uint_t)table->size,
             (ptr_uint_t)table);
        return false;
    }
    bm  = table->bm;
    idx = BITMAP_INDEX(entry->start);
    /* we assume that the whole bb is seen if its start addr is seen */
    if (bm[idx] == BB_TABLE_RANGE_SET)
        return false;
    offs = BITMAP_OFFSET(entry->start);
    if (TEST(BITMAP_MASK(offs), bm[idx]))
        return false;
    /* now we add a new bb */
    PRINT(6, "Add "PFX"-"PFX" in table "PFX"\n",
          (ptr_uint_t)entry->start,
          (ptr_uint_t)entry->start + entry->size,
          (ptr_uint_t)table);
    addr_end = entry->start + entry->size - 1;
    idx_end  = BITMAP_INDEX(addr_end);
    offs_end = (idx_end > idx) ? BITS_PER_BYTE-1 : BITMAP_OFFSET(addr_end);
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

static char *
read_module_list(char *buf, void ***tables, uint *num_mods)
{
    char  path[MAXIMUM_PATH];
    uint  i;
    uint  version;

    PRINT(3, "Reading module table...\n");
    /* versione number */
    PRINT(4, "Reading version number");
    if (dr_sscanf(buf, "DRCOV VERSION: %u\n", &version) != 1 &&
        version != DRCOV_VERSION) {
        WARN(2, "Failed to read version number");
        return NULL;
    }
    buf = move_to_next_line(buf);

    /* module table header */
    PRINT(4, "Reading Module Table Header\n");
    if (dr_sscanf(buf, "Module Table: %d\n", num_mods) != 1) {
        WARN(2, "Failed to read module table");
        return NULL;
    }
    buf = move_to_next_line(buf);

    /* module lists */
    PRINT(4, "Reading Module Lists\n");
    *tables = calloc(*num_mods, sizeof(*tables));
    for (i = 0; i < *num_mods; i++) {
        uint   mod_id;
        uint64 mod_size;
        void  *bb_table;
        /* assuming the string is something like:  "0, 2207744, /bin/ls" */
        /* XXX: i#1143: we do not use dr_sscanf since it does not support %[] */
        if (sscanf(buf, " %u, %"INT64_FORMAT"u, %[^\n\r]",
                   &mod_id, &mod_size, path) != 3)
            ASSERT(false, "Failed to read module table");
        buf = move_to_next_line(buf);
        PRINT(5, "Module: %u, "PFX", %s\n", mod_id, (ptr_uint_t)mod_size, path);
        bb_table = hashtable_lookup(&module_htable, path);
        if (bb_table == NULL) {
            if (mod_size >= UINT_MAX)
                ASSERT(false, "module size is too large");
            if (strstr(path, "<unknown>") != NULL ||
                (mod_filter != NULL && strstr(path, mod_filter) == NULL))
                bb_table = BB_TABLE_IGNORE;
             else
                bb_table = bb_table_create((uint)mod_size);
            PRINT(4, "Create bb table "PFX" for module %s\n",
                  (ptr_uint_t)bb_table, path);
            num_module_htable_entries++;
            if (!hashtable_add(&module_htable, path, bb_table))
                ASSERT(false, "Failed to add new module");
        }
        (*tables)[i] = bb_table;
    }
    return buf;
}

static bool
read_bb_list(char *buf, void **tables, uint num_mods, uint num_bbs)
{
    uint i;
    bb_entry_t *entry;
    bool add_new_bb = false;

    PRINT(4, "Reading %u basic blocks\n", num_bbs);
    for (i = 0, entry = (bb_entry_t *)buf; i < num_bbs; i++, entry++) {
        PRINT(6, "BB: "PFX", %u, %u\n",
              (ptr_uint_t)entry->start, entry->size, entry->mod_id);
        /* we could have mod id USHRT_MAX for unknown module e.g., [vdso] */
        if (entry->mod_id < num_mods)
            add_new_bb = bb_table_add(tables[entry->mod_id], entry) || add_new_bb;
    }
    free(tables);
    return add_new_bb;
}

static file_t
open_input_file(const char *fname, char **map_out OUT,
                size_t *map_size OUT, uint64 *file_sz OUT)
{
    uint64 file_size;
    char *map;
    file_t f;

    ASSERT(map_out != NULL && map_size != NULL, "map_out must not be NULL");
    f = dr_open_file(fname, DR_FILE_READ | DR_FILE_ALLOW_LARGE);
    if (f == INVALID_FILE) {
        WARN(2, "Failed to open file %s\n", fname);
        return INVALID_FILE;
    }
    if (!dr_file_size(f, &file_size)) {
        WARN(2, "Failed to get input file size for %s\n", fname);
        dr_close_file(f);
        return INVALID_FILE;
    }
    if (file_size <= MIN_LOG_FILE_SIZE) {
        WARN(2, "File size is 0 for %s\n", fname);
        dr_close_file(f);
        return INVALID_FILE;
    }
    *map_size = (size_t)file_size;
    map = dr_map_file(f, map_size, 0, NULL, DR_MEMPROT_READ, 0);
    if (map == NULL || (size_t)file_size > *map_size) {
        WARN(2, "Failed to map file %s\n", fname);
        dr_close_file(f);
        return INVALID_FILE;
    }
    *map_out = map;
    if (file_sz != NULL)
        *file_sz = file_size;
    return f;
}

static void
close_input_file(file_t f, char *map, size_t map_size)
{
    dr_unmap_file(map, map_size);
    dr_close_file(f);
}

static bool
read_drcov_file(char *input)
{
    file_t log;
    char  *map, *ptr;
    size_t map_size;
    void **tables;
    uint   num_mods, num_bbs;
    bool   res;

    PRINT(2, "Reading drcov log file: %s\n", input);
    log = open_input_file(input, &map, &map_size, NULL);
    if (log == INVALID_FILE) {
        WARN(1, "Failed to read drcov log file %s\n", input);
        return false;
    }
    ptr = read_module_list(map, &tables, &num_mods);
    if (ptr == NULL)
        return false;

    if (dr_sscanf(ptr, "BB Table: %u bbs\n", &num_bbs) != 1) {
        WARN(1, "Failed to read bb list from %s\n", input);
        return false;
    }
    ptr = move_to_next_line(ptr);
    if (num_bbs*sizeof(bb_entry_t) > map_size) {
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
        fname[2] == 'c' && fname[3] == 'o' &&
        fname[4] == 'v' && fname[5] == '.' &&
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

    PRINT(2, "Reading input directory %s\n", input_dir);
    if ((dir = opendir(input_dir)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (is_drcov_log_file(ent->d_name)) {
                if (GetFullPathName(ent->d_name, MAXIMUM_PATH, path, NULL) == 0) {
                    WARN(2, "Fail to get full path of log file %s\n", ent->d_name);
                } else {
                    NULL_TERMINATE_BUFFER(path);
                    read_drcov_file(path);
                    found_logs = true;
                }
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */
        WARN(1, "Failed to open directory %s\n", input_dir);
        return false;
    }
    if (!found_logs)
        WARN(1, "Failed to find log files in dir %s\n", input_dir);
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
    strcpy(path, input_dir);
    if (path[strlen(path)-1] == '\\') {
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
            strcpy(path, input_dir);
            if (!has_sep)
                strcat(path, "\\");
            strcat(path, ffd.cFileName);
            read_drcov_file(path);
            found_logs = true;
        }
    } while (FindNextFile(hFind, &ffd) != 0);
    FindClose(hFind);
    if (!found_logs)
        WARN(1, "Failed to find log files in dir %s\n", input_dir);
    return found_logs;
}
#endif

static bool
read_drcov_list(void)
{
    file_t list;
    char  *map, *ptr;
    char   path[MAXIMUM_PATH];
    size_t map_size;
    uint64 file_size;
    bool found_logs = false;

    PRINT(2, "Reading list %s\n", input_list);
    list = open_input_file(input_list, &map, &map_size, &file_size);
    if (list == INVALID_FILE) {
        WARN(1, "Failed to read list %s\n", input_list);
        return false;
    }
    /* process each file in the list */
    for (ptr = map; ptr < map + file_size; ) {
        if (dr_sscanf(ptr, "%s\n", path) != 1)
            break;
        NULL_TERMINATE_BUFFER(path);
        ptr = move_to_next_line(ptr);
        null_terminate_path(path);
        read_drcov_file(path);
        found_logs = true;
    }
    close_input_file(list, map, map_size);
    if (!found_logs)
        WARN(1, "Failed to find log files on list %s\n", input_list);
    return found_logs;
}

static bool
read_drcov_input(void)
{
    bool res = true;
    if (input_list != NULL)
        res = res && read_drcov_list();
    if (input_dir  != NULL)
        res = res && read_drcov_dir();
    return res;
}

static bool
enum_line_cb(drsym_line_info_t *info, void *data)
{
    int   status;
    void *bb_table = data;
    line_table_t *line_table;

    if (info->file == NULL ||
        (src_filter != NULL && strstr(info->file, src_filter) == NULL))
        return true;
    line_table = hashtable_lookup(&line_htable, (void *)info->file);
    if (line_table == NULL) {
        num_line_htable_entries++;
        line_table = line_table_create(info->file);
        if (!hashtable_add(&line_htable, (void *)info->file, line_table))
            ASSERT(false, "Failed to add new source line table");
    }
    status = bb_table_lookup(bb_table, (uint)info->line_addr);
    if (status == BB_TABLE_ENTRY_SET) {
        PRINT(5, "exec: ");
        line_table_add(line_table, info->line, SOURCE_LINE_STATUS_EXEC);
    } else if (status == BB_TABLE_ENTRY_CLEAR) {
        PRINT(5, "skip: ");
        line_table_add(line_table, info->line, SOURCE_LINE_STATUS_SKIP);
    } else {
        WARN(2, "Invalid bb table lookup, Table: "PFX", Addr: "PFX"\n",
             (ptr_uint_t)bb_table, (ptr_uint_t)info->line);
    }
    PRINT(5, "%s, %s, %llu, "PFX"\n",
          info->cu_name, info->file, (unsigned long long)info->line,
          (ptr_uint_t)info->line_addr);
    return true;
}

static bool
read_debug_info(void)
{
    uint i, num_entries = 0;
    /* iterate module table */
    for (i = 0; i < HASHTABLE_SIZE(module_htable.table_bits); i++) {
        hash_entry_t *e;
        drsym_error_t res;
        for (e = module_htable.table[i]; e != NULL; e = e->next) {
            num_entries++;
            PRINT(3, "Read debug info for %s\n", (char *)e->key);
            if (strcmp((char *)e->key, "<unknown>") == 0)
                continue;
            if (mod_filter != NULL && strstr((char *)e->key, mod_filter) == NULL)
                continue;
            res = drsym_enumerate_lines(e->key, enum_line_cb, e->payload);
            if (res != DRSYM_SUCCESS)
                WARN(1, "Failed to enumerate lines for %s\n", (char *)e->key);
            res = drsym_free_resources((char *)e->key);
            if (res != DRSYM_SUCCESS)
                WARN(1, "Failed to free resource for %s\n", (char *)e->key);
        }
    }
    ASSERT(num_entries == num_module_htable_entries,
           "Wrong number of hashtable entries");
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
    return strcmp(e1->key, e2->key);
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

    PRINT(2, "Writing output lcov file: %s\n", output_file);
    log = dr_open_file(output_file,
                       DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    if (log == INVALID_FILE) {
        ASSERT(false, "Failed to open output file %s\n", output_file);
        return false;
    }

    /* sort them before print */
    src_array = calloc(line_htable.entries, sizeof(src_array[0]));
    for (i = 0; i < HASHTABLE_SIZE(line_htable.table_bits); i++) {
        for (e = line_htable.table[i]; e != NULL; e = e->next) {
            src_array[num_entries] = e;
            num_entries++;
        }
    }
    ASSERT(num_entries == num_line_htable_entries &&
           line_htable.entries == num_entries,
           "Wrong number of hashtable entries");
    qsort(src_array, num_entries, sizeof(src_array[0]), compare_source_file);

    /* print */
    buf_sz = LINE_TABLE_INIT_PRINT_BUF_SIZE;
    buf = malloc(buf_sz);
    ASSERT(buf != NULL, "Failed to alloc print buffer\n");
    for (i = 0; i < num_entries; i++) {
        e = src_array[i];
        PRINT(4, "Writing coverage info for %s\n", (char *)e->key);
        if (line_table_print_buf_size((line_table_t *)e->payload) >= buf_sz) {
            free(buf);
            buf_sz = line_table_print_buf_size(e->payload);
            buf = malloc(buf_sz);
            ASSERT(buf != NULL, "Failed to alloc print buffer\n");
        }
        ptr  = buf;
        ptr += dr_snprintf(ptr, SOURCE_FILE_START_LINE_SIZE, "SF:%s\n", e->key);
        ptr  = line_table_print(e->payload, ptr);
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

static bool
option_init(int argc, char *argv[])
{
    int i;
    if (argc == 1)
        return false;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0)
            return false;
        if (strcmp(argv[i], "--list") == 0) {
            if (++i >= argc)
                return false;
            input_list = argv[i];
        } else if (strcmp(argv[i], "--dir") == 0) {
            if (++i >= argc)
                return false;
            input_dir = argv[i];
        } else if (strcmp(argv[i], "--output") == 0) {
            if (++i >= argc)
                return false;
            output_file = argv[i];
        } else if (strcmp(argv[i], "--src_filter") == 0) {
            if (++i >= argc)
                return false;
            src_filter = argv[i];
        } else if (strcmp(argv[i], "--mod_filter") == 0) {
            if (++i >= argc)
                return false;
            mod_filter = argv[i];
        } else if (strcmp(argv[i], "--reduce_set") == 0) {
            if (++i >= argc)
                return false;
            set_file = argv[i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            char *end;
            long int res;
            if (++i >= argc)
                return false;
            res = strtol(argv[i], &end, 10);
            if (res == LONG_MAX || res < 0)
                WARN(1, "Wrong verbose level, use %d instead\n", verbose);
            else
                verbose = res;
        } else if (strcmp(argv[i], "--warning") == 0) {
            char *end;
            long int res;
            if (++i >= argc)
                return false;
            res = strtol(argv[i], &end, 10);
            if (res == LONG_MAX || res < 0)
                WARN(1, "Wrong warning level, use %d instead\n", warning);
            else
                warning = res;
        }
    }

    if (input_list != NULL) {
        if (GetFullPathName(input_list,
                            BUFFER_SIZE_ELEMENTS(input_list_buf),
                            input_list_buf, NULL) == 0) {
            WARN(1, "Failed to get full path of input list\n");
            return false;
        }
        NULL_TERMINATE_BUFFER(input_list_buf);
        input_list = input_list_buf;
        PRINT(2, "Input list: %s\n", input_list);
    } else {
        if (input_dir == NULL)
            WARN(0, "Missing input, use current directory instead\n");
        if (GetFullPathName(input_dir == NULL ? "./" : input_dir,
                            BUFFER_SIZE_ELEMENTS(input_dir_buf),
                            input_dir_buf, NULL) == 0) {
            WARN(1, "Failed to get full path of input dir\n");
            return false;
        }
        NULL_TERMINATE_BUFFER(input_dir_buf);
        input_dir = input_dir_buf;
        PRINT(2, "Input dir: %s\n", input_dir);
    }

    if (output_file == NULL)
        WARN(1, "Missing output, use coverage.info instead\n");
    if (GetFullPathName(output_file == NULL ? "coverage.info" : output_file,
                        BUFFER_SIZE_ELEMENTS(output_file_buf),
                        output_file_buf, NULL) == 0) {
        WARN(1, "Failed to get full path of output file");
        return false;
    }
    NULL_TERMINATE_BUFFER(output_file_buf);
    output_file = output_file_buf;
    PRINT(2, "Output file: %s\n", output_file);
    if (set_file != NULL) {
        if (GetFullPathName(set_file,
                            BUFFER_SIZE_ELEMENTS(set_file_buf),
                            set_file_buf, NULL) == 0) {
            WARN(1, "Failed to get full path of reduce_set file\n");
            return false;
        }
        NULL_TERMINATE_BUFFER(set_file_buf);
        set_file = set_file_buf;
        PRINT(2, "Reduced set file: %s\n", set_file);
        set_log = dr_open_file(set_file, DR_FILE_WRITE_REQUIRE_NEW);
        if (set_log == INVALID_FILE) {
            ASSERT(false, "Failed to open reduce set output file %s\n", set_file);
            return false;
        }
    }
    return true;
}

/****************************************************************************
 * Main Function
 */

int
main(int argc, char *argv[])
{
    if (!option_init(argc, argv)) {
        ASSERT(false, "%s\n", usage_str);
        return 1;
    }

    dr_standalone_init();
    if (drsym_init(IF_WINDOWS_ELSE(NULL, 0)) != DRSYM_SUCCESS) {
        ASSERT(false, "Unable to initialize symbol translation");
        return 1;
    }
    hashtable_init_ex(&module_htable, MODULE_HASH_TABLE_BITS, HASH_STRING,
                      true /* strdup */, false /* !synch */,
                      bb_table_delete /* free */,
                      NULL /* hash */, NULL /* cmp */);
    hashtable_init_ex(&line_htable, LINE_HASH_TABLE_BITS, HASH_STRING,
                      true /* strdup */, false /* !synch */,
                      line_table_delete /* free */,
                      NULL /* hash */, NULL /* cmp */);

    PRINT(1, "Reading input files...\n");
    if (!read_drcov_input()) {
        ASSERT(false, "Failed to read input files\n");
        return 1;
    }

    PRINT(1, "Reading debug info...\n");
    if (!read_debug_info()) {
        ASSERT(false, "Failed to read debug info\n");
        return 1;
    }

    PRINT(1, "Writing output file...\n");
    if (!write_lcov_output()) {
        ASSERT(false, "Failed to write output file\n");
        return 1;
    }

    hashtable_delete(&module_htable);
    hashtable_delete(&line_htable);
    if (drsym_exit() != DRSYM_SUCCESS) {
        ASSERT(false, "Failed to clean up symbol library\n");
        return 1;
    }
    if (set_log != INVALID_FILE)
        dr_close_file(set_log);
    return 0;
}
