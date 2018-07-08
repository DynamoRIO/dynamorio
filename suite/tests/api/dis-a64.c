/* **********************************************************
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test the disassembler, decoder and encoder. */

#include "configure.h"
#include "dr_api.h"
#include <stdlib.h>
#include <string.h>

/* An arbitrary PC for more readable disassembly of PC-relative operands. */
#define ORIG_PC ((byte *)0x10000000)

bool
map_file(const char *file_name, byte **map_base, size_t *map_size)
{
    file_t f;
    uint64 file_size;
    void *base;
    size_t size;

    f = dr_open_file(file_name, DR_FILE_READ);
    if (f == INVALID_FILE)
        return false;
    if (!dr_file_size(f, &file_size))
        return false;
    size = (size_t)file_size;
    base = dr_map_file(f, &size, 0, NULL, DR_MEMPROT_READ, DR_MAP_PRIVATE);
    if (base == NULL || size < file_size)
        return false;
    *map_base = base;
    *map_size = size;
    return true;
}

void
check_inst(void *dc, uint enc, const char *dis, size_t len, bool verbose, bool *failed)
{
    size_t buflen = (len < 100 ? 100 : len) + 2;
    char *buf = malloc(buflen);
    char *end;
    instr_t instr;
    byte *pc2;
    uint enc2;

    if (verbose) {
        dr_printf("> ");
        dr_write_file(STDOUT, dis, len);
        dr_printf("\n");
    }

    /* Test disassembler. */
    disassemble_to_buffer(dc, (byte *)&enc, ORIG_PC, false, false, buf, buflen, NULL);
    end = buf + strlen(buf);
    if (end > buf && *(end - 1) == '\n')
        --end;
    if (end - buf != len || memcmp(buf, dis, len) != 0) {
        if (verbose)
            dr_printf("\n");
        dr_printf("Error: Disassembly differs:\n%08x  ", enc);
        dr_write_file(STDOUT, dis, len);
        dr_printf(" .\n          ");
        dr_write_file(STDOUT, buf, end - buf);
        dr_printf(" .\n\n");
        *failed = true;
    }

    /* Test decode and reencode. */
    instr_init(dc, &instr);
    decode_from_copy(dc, (byte *)&enc, ORIG_PC, &instr);
    pc2 = instr_encode_to_copy(dc, &instr, (byte *)&enc2, ORIG_PC);
    if (pc2 != (byte *)&enc2 + 4 || enc2 != enc) {
        if (verbose)
            dr_printf("\n");
        dr_printf("Error: Reencoding differs:\n%08x  ", enc);
        dr_write_file(STDOUT, dis, len);
        dr_printf("\n%08x  ", enc2);
        disassemble_from_copy(dc, (byte *)&enc2, ORIG_PC, STDOUT, false, false);
        dr_printf("\n");
        *failed = true;
    }
}

void
error(const char *line, size_t len, const char *s)
{
    dr_printf("Error: %s\n", s);
    dr_write_file(STDOUT, line, len);
    dr_printf("\n");
    exit(1);
}

void
do_line(void *dc, const char *line, size_t len, bool verbose, bool *failed)
{
    const char *end = line + len;
    const char *s = line;
    uint enc = 0;
    const char *t1, *t2;

    while (s < end && strchr(" \t", *s) != NULL)
        ++s;
    if (s >= end || *s == '#')
        return; /* blank line or comment */
    t1 = s;
    while (s < end) {
        const char *hex = "0123456789abcdef";
        char *t = strchr(hex, *s);
        if (t == NULL)
            break;
        enc = enc << 4 | (t - hex);
        ++s;
    }
    t2 = s;
    while (s < end && strchr(" \t", *s) != NULL)
        ++s;
    if (t1 == t2 || s >= end || *s != ':')
        error(line, len, "First colon-separated field should contain lower-case hex.");
    ++s;
    while (s < end && *s != ':')
        ++s;
    if (s >= end)
        error(line, len, "Third colons-separated field is missing.");
    ++s;
    while (s < end && strchr(" \t", *s) != NULL)
        ++s;
    check_inst(dc, enc, s, end - s, verbose, failed);
}

int
run_test(void *dc, const char *file, bool verbose)
{
    bool failed = false;
    byte *map_base;
    size_t map_size;
    byte *s, *end;

    if (!map_file(file, &map_base, &map_size)) {
        dr_printf("Failed to map file '%s'\n", file);
        return 1;
    }

    s = map_base;
    end = map_base + map_size;
    while (s < end) {
        byte *t = memchr(s, '\n', end - s);
        do_line(dc, (const char *)s, (t == NULL ? end : t) - s, verbose, &failed);
        s = (t == NULL ? end : t + 1);
    }

    if (failed) {
        dr_printf("FAIL\n");
        return 1;
    } else {
        dr_printf("PASS\n");
        return 0;
    }
}

void
run_decode(void *dc, const char *encoding)
{
    uint enc = strtol(encoding, NULL, 16);
    disassemble_from_copy(dc, (byte *)&enc, ORIG_PC, STDOUT, false, false);
}

int
main(int argc, char *argv[])
{
    void *dc = dr_standalone_init();

    if (argc != 3 ||
        (strcmp(argv[1], "-q") != 0 && strcmp(argv[1], "-v") != 0 &&
         strcmp(argv[1], "-d") != 0)) {
        dr_printf("Usage: %s [-q |- v] FILE\n", argv[0]);
        dr_printf("   Or: %s -d NUMBER\n", argv[0]);
        dr_printf("Test the disassembler, decoder and encoder on a set of test cases.\n"
                  "\n"
                  "  -q FILE    Run test quietly.\n"
                  "  -v FILE    Run test verbosely.\n"
                  "  -d NUMBER  Disassemble a single instruction.\n");
        return 0;
    }

    if (strcmp(argv[1], "-d") == 0) {
        run_decode(dc, argv[2]);
        return 0;
    }

    return run_test(dc, argv[2], (strcmp(argv[1], "-v") == 0));
}
