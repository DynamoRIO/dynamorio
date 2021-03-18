/* **********************************************************
 * Copyright (c) 2020-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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
#include <stdlib.h> /* for realloc */
#include <assert.h>
#include <stddef.h> /* for offsetof */
#include <string.h> /* for memcpy */
#include <imagehlp.h>
#include <stdio.h>

/*
 * NOTE - I haven't tried running this with the dynamorio.dll in this directory so it
 * might not work. I've been using a tot build of core.
 *
 * ls *.dll > LIST
 * for i in $( cat LIST ) ; do ../vista_hash.exe $i; done > ../OUTPUT
 */

void
dummy()
{
}

static bool v = false;
static bool vv = false;

#define VVERBOSE_PRINT \
    if (vv)            \
    printf
#define VERBOSE_PRINT \
    if (v)            \
    printf

#define ASSERT(x) ((x) ? 0 : dr_messagebox("Error %d %s\n", __LINE__, #x), 0)

#define ALIGN_BACKWARD(x, alignment) (((ptr_uint_t)x) & (~((alignment)-1)))
#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((alignment)-1)))

bool
compare_pages(void *drcontext, byte *start1, byte *start2, uint start_offset)
{
    static byte copy_buf1_i[2 * PAGE_SIZE + 100];
    static byte copy_buf2_i[2 * PAGE_SIZE + 100];
    static byte buf1_i[2 * PAGE_SIZE + 100];
    static byte buf2_i[2 * PAGE_SIZE + 100];

    byte *copy1 = (byte *)ALIGN_FORWARD(copy_buf1_i, PAGE_SIZE);
    byte *copy2 = (byte *)ALIGN_FORWARD(copy_buf2_i, PAGE_SIZE);
    byte *buf1 = (byte *)ALIGN_FORWARD(buf1_i, PAGE_SIZE);
    byte *buf2 = (byte *)ALIGN_FORWARD(buf2_i, PAGE_SIZE);

    byte *p1 = copy1 + start_offset, *p2 = copy2 + start_offset;
    byte *b1 = buf1, *c1 = buf1, *b2 = buf2, *c2 = buf2;

    int skipped_bytes1 = 0, skipped_identical_bytes1 = 0;
    int skipped_bytes2 = 0, skipped_identical_bytes2 = 0;

    /* we make a copy (zero extend the page) for decode to avoid walking to next,
     * potentially invalid page */
    memcpy(copy1, start1, PAGE_SIZE);
    memcpy(copy2, start2, PAGE_SIZE);

    /* Note - we do the compare ~1 instruction at a time, would be more effiecient
     * to do the whole page and compare, but this makes it easier to track down where
     * the differences originate from. */

    while (p1 < copy1 + PAGE_SIZE) {
        /* The idea is to do a lightweight decoding of the page and eliminate likely
         * relocations from the instruction stream.  I expect that relocations within
         * the stream to be 4-byte immeds or 4-byte displacements with pointer like
         * values.  For now just using decode_sizeof to get the size of the instuction
         * and based on that determine how much to chop off the end (instruction format
         * is [...][disp][imm]) to remove the potential relocation.  With better
         * information from decode_sizeof (it knows whether an immed/disp is present or
         * not and what offset it would be etc.) we could keep more of the bytes but
         * this works for testing. What we'll miss unless we get really lucky is
         * relocs in read only data (const string arrays, for dr the const decode
         * tables full of pointers for ex.). */

        /* How the assumptions work out.  The assumption that we quickly get to the
         * appropriate instruction frame seems valid.  Most cases we synchronize very
         * quickly, usually just a couple of bytes and very rarely more then 20.
         * I Haven't seen any relocations in instructions that weren't caught
         * below.  However, so far only matching ~60% of sibling pages because of
         * read only data relocations interspersed in the text sections. Instruction
         * frame alignment isn't an issue that often and the second pass is catching
         * most of those. */

        while (p1 - copy1 <= p2 - copy2) {
            uint num_bytes, i, num_prefix = 0;
            uint size = decode_sizeof(drcontext, p1, &num_prefix);
            size -= num_prefix;
            num_bytes = num_prefix;
            if (size == 0) {
                size = 1; /* treat invalid as size == 1 */
                num_bytes += 1;
            } else if (size <= 4) {
                num_bytes += size; /* no 4-byte disp or imm */
            } else if (size <= 6) {
                num_bytes += size - 4; /* could have 4-byte disp or immed */
            } else if (size <= 7) {
                num_bytes += size - 5; /* could have 4-byte disp and upto 1-byte immed
                                        * or 4-byte immed */
            } else if (size <= 9) {
                num_bytes += size - 6; /* could have 4-byte disp and upto 2-byte immed
                                        * or 4-byte immed */
            } else {
                num_bytes += size - 8; /* could have 4-byte disp and 4-byte immed */
            }
            for (i = 0; i < num_bytes; i++)
                *b1++ = *p1++;
            skipped_bytes1 += size - (num_bytes - num_prefix);
            for (i = 0; i < size - (num_bytes - num_prefix); i++) {
                if (*p1 == *(copy2 + (p1 - copy1)))
                    skipped_identical_bytes1++;
                p1++;
            }
        }

        while (p2 - copy2 < p1 - copy1) {
            uint num_bytes, i, num_prefix = 0;
            uint size = decode_sizeof(drcontext, p2, &num_prefix);
            size -= num_prefix;
            num_bytes = num_prefix;
            if (size == 0) {
                size = 1; /* treat invalid as size == 1 */
                num_bytes += 1;
            } else if (size <= 4) {
                num_bytes += size;
            } else if (size <= 6) {
                num_bytes += size - 4;
            } else if (size <= 7) {
                num_bytes += size - 5;
            } else if (size <= 9) {
                num_bytes += size - 6;
            } else {
                num_bytes += size - 8;
            }
            for (i = 0; i < num_bytes; i++)
                *b2++ = *p2++;
            skipped_bytes2 += size - (num_bytes - num_prefix);
            for (i = 0; i < size - (num_bytes - num_prefix); i++) {
                if (*p2 == *(copy1 + (p2 - copy2)))
                    skipped_identical_bytes2++;
                p2++;
            }
        }

        /* Do check */
        while (c1 < b1 && c2 < b2) {
            if (*c1++ != *c2++) {
                VVERBOSE_PRINT("Mismatch found near offset 0x%04x of page %08x\n",
                               p1 - copy1, start1);
                return false;
            }
        }
    }
    ASSERT(skipped_bytes1 == skipped_bytes2);
    ASSERT(skipped_identical_bytes1 == skipped_identical_bytes2);
    VVERBOSE_PRINT("Match found! skipped=%d skipped_identical=%d\n", skipped_bytes1,
                   skipped_identical_bytes1);
    return true;
}

/* Note that we should keep an eye for potential additional qualifier
 * flags.  Alternatively we may simply mask off ~0xff to allow for any
 * future flags added here.
 */
#define PAGE_PROTECTION_QUALIFIERS (PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)

bool
prot_is_writable(uint prot)
{
    prot &= ~PAGE_PROTECTION_QUALIFIERS;
    return (prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
            prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY);
}

#define OPT_HDR(nt_hdr_p, field) ((nt_hdr_p)->OptionalHeader.field)
#define OPT_HDR_P(nt_hdr_p, field) (&((nt_hdr_p)->OptionalHeader.field))
bool
get_IAT_section_bounds(app_pc module_base, app_pc *iat_start, app_pc *iat_end)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module_base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt == NULL ||
        nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    dir = &OPT_HDR(nt, DataDirectory)[IMAGE_DIRECTORY_ENTRY_IAT];
    *iat_start = module_base + dir->VirtualAddress;
    *iat_end = module_base + dir->VirtualAddress + dir->Size;
    return true;
}

int
usage(char *name)
{
    printf("Usage: %s [-v] [-vv] [-no_second_pass] [-second_pass_offset <val>] "
           "[-no_assume_IAT_written] [-spin_for_debugger] <dll>\n",
           name);
    return -1;
}

int
main(int argc, char *argv[])
{
    byte *dll_1, *dll_2, *p1, *p2, *iat_start1, *iat_end1, *iat_start2, *iat_end2;
    bool has_iat = false;
    MEMORY_BASIC_INFORMATION info;
    void *drcontext = dr_standalone_init();
    uint writable_pages = 0, reserved_pages = 0, IAT_pages = 0;
    uint matched_pages = 0, second_matched_pages = 0, unmatched_pages = 0;
    uint exact_match_pages = 0, exact_no_match_pages = 0;
    char reloc_file[MAX_PATH] = { 0 }, orig_file[MAX_PATH], *input_file;
    uint old_size = 0, new_size = 0;
    uint old_base = 0, new_base = 0x69000000; /* unlikely to collide */

    /* user specified option defaults */
    uint arg_offs = 1;
    bool use_second_pass = true;
    bool assume_header_match = true;
    uint second_pass_offset = 16; /* FIXME arbitrary, what's a good choice? */
    bool assume_IAT_written = true;
    bool spin_for_debugger = false;

    if (argc < 2)
        return usage(argv[0]);
    while (argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-vv") == 0) {
            vv = true;
        } else if (strcmp(argv[arg_offs], "-v") == 0) {
            v = true;
        } else if (strcmp(argv[arg_offs], "-no_second_pass") == 0) {
            use_second_pass = false;
        } else if (strcmp(argv[arg_offs], "-second_pass_offset") == 0) {
            if ((uint)argc <= arg_offs + 1)
                return usage(argv[0]);
            second_pass_offset = atoi(argv[++arg_offs]);
        } else if (strcmp(argv[arg_offs], "-no_assume_IAT_written") == 0) {
            assume_IAT_written = false;
        } else if (strcmp(argv[arg_offs], "-spin_for_debugger") == 0) {
            spin_for_debugger = true;
        } else {
            return usage(argv[0]);
        }
        arg_offs++;
    }
    input_file = argv[arg_offs++];
    if (arg_offs != argc)
        return usage(argv[0]);

    _snprintf(reloc_file, sizeof(reloc_file), "%s.reloc.dll", input_file);
    reloc_file[sizeof(reloc_file) - 1] = '\0';
    if (!CopyFile(input_file, reloc_file, FALSE)) {
        LPSTR msg = NULL;
        uint error = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0,
                      GetLastError(), 0, msg, 0, NULL);
        VERBOSE_PRINT("Copy Error %d (0x%x) = %s\n", error, error, msg);
        return 1;
    }
    snprintf(orig_file, sizeof(orig_file), "%s.orig.dll", input_file);
    orig_file[sizeof(orig_file) - 1] = '\0';
    if (!CopyFile(input_file, orig_file, FALSE)) {
        LPSTR msg = NULL;
        uint error = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0,
                      GetLastError(), 0, msg, 0, NULL);
        VERBOSE_PRINT("Copy Error %d (0x%x) = %s\n", error, error, msg);
        return 1;
    }
    if (ReBaseImage(reloc_file, "", TRUE, FALSE, FALSE, 0, &old_size, &old_base,
                    &new_size, &new_base, 0)) {
        VERBOSE_PRINT("Rebased imsage \"%s\" from 0x%08x to 0x%08x\n"
                      "Size changed from %d bytes to %d bytes\n",
                      input_file, old_base, new_base, old_size, new_size);
    } else {
        LPSTR msg = NULL;
        uint error = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0,
                      GetLastError(), 0, msg, 0, NULL);
        VERBOSE_PRINT("Rebase Error %d (0x%x) = %s\n", error, error, msg);
        return 1;
    }

    dll_1 = (byte *)ALIGN_BACKWARD(
        LoadLibraryExA(orig_file, NULL, DONT_RESOLVE_DLL_REFERENCES), PAGE_SIZE);
    p1 = dll_1;
    dll_2 = (byte *)ALIGN_BACKWARD(
        LoadLibraryExA(reloc_file, NULL, DONT_RESOLVE_DLL_REFERENCES), PAGE_SIZE);
    p2 = dll_2;
    VVERBOSE_PRINT("Loaded dll @ 0x%08x and 0x%08x\n", dll_1, dll_2);

    if (dll_1 == NULL || dll_2 == NULL) {
        VERBOSE_PRINT("Error loading %s\n", input_file);
        return 1;
    }

    /* Handle the first page specially since I'm seeing problems with a handful of
     * dlls that aren't really getting rebased. mcupdate_GenuineIntel.dll for ex.
     * (which does have relocations etc.) not sure what's up, but it's only a couple of
     * dlls so will ignore them. If we really rebased the header should differ. */
    if (memcmp(dll_1, dll_2, PAGE_SIZE) == 0) {
        printf("%s - ERROR during relocating\n", input_file);
        return 1;
    } else {
        exact_no_match_pages++;
        if (assume_header_match)
            /* We could modify the hash function to catch header pages. */
            matched_pages++;
        else
            unmatched_pages++;
    }
    p1 += PAGE_SIZE;
    p2 += PAGE_SIZE;

    if (assume_IAT_written && get_IAT_section_bounds(dll_1, &iat_start1, &iat_end1)) {
        has_iat = true;
        ASSERT(get_IAT_section_bounds(dll_2, &iat_start2, &iat_end2) &&
               iat_start1 - dll_1 == iat_start2 - dll_2 &&
               iat_end1 - dll_1 == iat_end2 - dll_2);
    }

    while (dr_virtual_query(p1, &info, sizeof(info)) == sizeof(info) &&
           info.State != MEM_FREE && info.AllocationBase == dll_1) {
        /* we only check read-only pages (assumption writable pages aren't shareable) */
        ASSERT(p1 == info.BaseAddress);
        if (info.State != MEM_COMMIT) {
            reserved_pages += info.RegionSize / PAGE_SIZE;
            VVERBOSE_PRINT("skipping %d reserved pages\n", info.RegionSize / PAGE_SIZE);
            p1 += info.RegionSize;
            p2 += info.RegionSize;
        } else if (!prot_is_writable(info.Protect)) {
            uint i;
            for (i = 0; i < info.RegionSize / PAGE_SIZE; i++) {
                bool exact = false;
                if (assume_IAT_written && has_iat && iat_end1 > p1 &&
                    iat_start1 < p1 + PAGE_SIZE) {
                    /* overlaps an IAT page */
                    IAT_pages++;
                    p1 += PAGE_SIZE;
                    p2 += PAGE_SIZE;
                    continue;
                }
                if (memcmp(p1, p2, PAGE_SIZE) == 0) {
                    VVERBOSE_PRINT("Page Exact Match\n");
                    exact_match_pages++;
                    exact = true;
                } else {
                    VVERBOSE_PRINT("Page Exact Mismatch\n");
                    exact_no_match_pages++;
                }
                if (compare_pages(drcontext, p1, p2, 0)) {
                    VVERBOSE_PRINT("Matched page\n");
                    matched_pages++;
                } else {
                    VVERBOSE_PRINT("Failed to match page\n");
                    if (use_second_pass &&
                        compare_pages(drcontext, p1, p2, second_pass_offset)) {
                        second_matched_pages++;
                    } else {
                        unmatched_pages++;
                    }
                    ASSERT(!exact);
                }
                p1 += PAGE_SIZE;
                p2 += PAGE_SIZE;
            }
        } else {
            writable_pages += info.RegionSize / PAGE_SIZE;
            VVERBOSE_PRINT("skipping %d writable pages\n", info.RegionSize / PAGE_SIZE);
            p1 += info.RegionSize;
            p2 += info.RegionSize;
        }
    }

    VERBOSE_PRINT("%d exact match, %d not exact match\n%d hash_match, %d "
                  "second_hash_match, %d hash_mismatch\n",
                  exact_match_pages, exact_no_match_pages, matched_pages,
                  second_matched_pages, unmatched_pages);

    printf("%s : %d pages - %d w %d res %d IAT = %d same %d differ : %d hash differ %d "
           "first hash differ : %d%% found, %d%% found first hash\n",
           input_file,
           writable_pages + reserved_pages + IAT_pages + exact_match_pages +
               exact_no_match_pages,
           writable_pages, reserved_pages, IAT_pages, exact_match_pages,
           exact_no_match_pages, unmatched_pages, unmatched_pages + second_matched_pages,
           (100 * (matched_pages + second_matched_pages - exact_match_pages)) /
               exact_no_match_pages,
           (100 * (matched_pages - exact_match_pages)) / exact_no_match_pages);

    while (spin_for_debugger)
        Sleep(1000);

    dr_standalone_exit();
    return 0;
}

#if 0
/* First tried something like this, but we hit too many issues in decode and encode */
bool
compare_pages(void *drcontext, byte *start1, byte *start2)
{
    byte *p1 = start1, *p2 = start2;
    int skipped_bytes = 0, identical_skipped_bytes = 0;

    while (p1 < start1 + PAGE_SIZE) {
        int instr_size = decode_sizeof(drcontext, p1, NULL _IF_X64(NULL));
        if (p1 + instr_size > start1 + PAGE_SIZE) {
            /* We're overlapping the end of the page, skip these. */
            int end_skip = start1 + PAGE_SIZE - p1;
            VVERBOSE_PRINT("Passing PAGE_END %d bytes", end_skip);
            skipped_bytes += end_skip;
            if (memcmp(p1, p2, end_skip) == 0)
                identical_skipped_bytes += end_skip;
            break;
        }
        if (decode_sizeof(drcontext, p2, NULL _IF_X64(NULL)) != instr_size) {
            VVERBOSE_PRINT("Instruction alignment mismatch\n");
            return false;
        }
        /* assumption - instructions <= 4 bytes in size won't have relocations */
        if (instr_size < 5) {
            if (memcmp(p1, p2, instr_size) != 0) {
                VVERBOSE_PRINT("Difference found in small instr\n");
                return false;
            }
            p1 += size;
            p2 += size;
        } else {
            /* guess if there could be a relocation */
            instr_t *instr1 = instr_create(drcontext);
            instr_t *instr2 = instr_create(drcontext);
            p1 = decode(drcontext, p1, instr1);
            p2 = decode(drcontext, p2, instr2);
            if (p1 - start1 != p2 - start2) {
                VVERBOSE_PRINT("Instruction alignment mismatch on full decode\n");
                /* Fixme - free instr, don't expect this to happen */
                return false;
            }
            if (instr_get_num_srcs(instr1) != instr_get_num_srcs(instr2) ||
                instr_get_num_dsts(instr1) != instr_get_num_dsts(instr2)) {
                VVERBOSE_PRINT("Full decode operand mismatch");
                return false;
            }
            for (i = instr_get_num_srcs(instr1); i > 0; i--) {
                opnd_t opnd = instr_get_src(instr1, i);
                if (opnd_is_immed_int(opnd) && opnd_get_immed_int(opnd) > 0x10000) {
                    instr_set_src(instr1, i, opnd_create_immed_int(opnd_get_immed_int(opnd), opnd_get_size(opnd)));
                }
            }
        }
    }
}

void
modify_instr_for_relocations(void *drcontext, instr_t *inst,
                             ptr_uint_t *immed, ptr_uint_t *disp)
{
    int i;
    ptr_uint_t limmed = 0, ldisp = 0;
    for (i = instr_num_srcs(inst) - 1; i >= 0; i--) {
        opnd_t opnd = instr_get_src(inst, i);
        if (opnd_is_immed_int(opnd) && opnd_get_immed_int(opnd) > 0x10000) {
            if (limmed != 0) {
                ASSERT(false);
            } else {
                limmed = opnd_get_immed_int(opnd);
            }
            instr_set_src(inst, i, opnd_create_immed_int(0, opnd_get_size(opnd)));
        }
        if (opnd_is_base_disp(opnd) && opnd_get_disp(opnd) > 0x10000) {
            if (ldisp != 0 && ldisp != opnd_get_disp(opnd)) {
                ASSERT(false);
            } else {
                ldisp = opnd_get_disp(opnd);
            }
            instr_set_src(inst, i, opnd_create_base_disp(opnd_get_base(opnd), opnd_get_index(opnd), opnd_get_scale(opnd), 0, opnd_get_size(opnd)));
        }
    }
    for (i = instr_num_dsts(inst) - 1; i >= 0; i--) {
        opnd_t opnd = instr_get_dst(inst, i);
        ASSERT(!opnd_is_immed(opnd));
        if (opnd_is_base_disp(opnd) && opnd_get_disp(opnd) > 0x10000) {
            if (ldisp != 0 && ldisp != opnd_get_disp(opnd)) {
                ASSERT(false);
            } else {
                ldisp = opnd_get_disp(opnd);
            }
            instr_set_dst(inst, i, opnd_create_base_disp(opnd_get_base(opnd), opnd_get_index(opnd), opnd_get_scale(opnd), 0, opnd_get_size(opnd)));
        }
    }
    if (limmed != 0)
        *immed = limmed;
    if (ldisp != 0)
        *disp = ldisp;
}

#endif
