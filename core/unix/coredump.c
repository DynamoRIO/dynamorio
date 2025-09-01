/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

#include <elf.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/procfs.h>
#include "../globals.h"

#include "../hashtable.h"
#include "../os_shared.h"
#include "../synch.h"
#include "../utils.h"
#include "../ir/decode.h"
#include "../lib/dr_ir_utils.h"
#include "../lib/globals_api.h"
#include "../lib/globals_shared.h"
#include "../lib/instrument.h"

#include "dr_tools.h"
#include "elf_defines.h"
#include "memquery.h"
#if defined(AARCH64)
#    include "tls.h"
#endif

/* Only X64 is supported. */
#ifndef X64
#    error Unsupported architecture
#endif

#define MAX_BUFFER_SIZE 4096
#define MAX_SECTION_HEADERS 300
#define MAX_SECTION_NAME_BUFFER_SIZE 8192
#define SECTION_HEADER_TABLE ".shstrtab"
#define VVAR_SECTION "[vvar]"
#define VSYSCALL_SECTION "[vsyscall]"
/*
 * The length of the name has to be a multiple of eight (for X64) to ensure the next
 * field (descriptor) is 8-byte aligned. Null characters are added at the end as
 * padding to increase the length to eight. The name CORE is used following the example
 * of core dump files.
 */
#define NOTE_OWNER "CORE\0\0\0\0"
#define NOTE_OWNER_LENGTH 8

#if defined(AARCH64)
/*
 * Three notes, NT_PRSTATUS (prstatus structure), NT_FPREGSET (floating point registers)
 * and NT_ARM_TLS (tpidr_el0) are written to the output file.
 */
#    define PROGRAM_HEADER_NOTE_LENGTH                                               \
        (sizeof(ELF_NOTE_HEADER_TYPE) + NOTE_OWNER_LENGTH +                          \
         sizeof(struct elf_prstatus) + sizeof(ELF_NOTE_HEADER_TYPE) +                \
         NOTE_OWNER_LENGTH + sizeof(elf_fpregset_t) + sizeof(ELF_NOTE_HEADER_TYPE) + \
         NOTE_OWNER_LENGTH + sizeof(reg_t))
#else
/*
 * Two notes, NT_PRSTATUS (prstatus structure) and NT_FPREGSET (floating point registers),
 * are written to the output file.
 */
#    define PROGRAM_HEADER_NOTE_LENGTH                                \
        (sizeof(ELF_NOTE_HEADER_TYPE) + NOTE_OWNER_LENGTH +           \
         sizeof(struct elf_prstatus) + sizeof(ELF_NOTE_HEADER_TYPE) + \
         NOTE_OWNER_LENGTH + sizeof(elf_fpregset_t))
#endif

typedef struct _section_header_info_t {
    app_pc vm_start;
    app_pc vm_end;
    uint prot;
    ELF_ADDR name_offset;
} section_header_info_t;

/*
 * Please reference
 * https://www.intel.com/content/www/us/en/docs/cpp-compiler/developer-guide-reference/
 * 2021-8/intrinsics-to-save-and-restore-ext-proc-states.html for the memory layout
 * used by fxsave64.
 */
typedef struct _fxsave64_map_t {
    unsigned short fcw;
    unsigned short fsw;
    unsigned short ftw;
    unsigned short reserved1;
    unsigned short fop;
    unsigned int fip;
    unsigned short fcs;
    unsigned short reserved2;
    unsigned int fdp;
    unsigned int fds;
    unsigned short reserved3;
    unsigned short mxcsr;
    unsigned short mxcsr_mask;
    unsigned int st_space[32];
    unsigned int xmm_space[64];
    unsigned int padding[24];
} fxsave64_map_t;

/*
 * Writes an ELF header to the file. Returns true if the ELF header is written to the core
 * dump file, false otherwise.
 */
static bool
write_elf_header(DR_PARAM_IN file_t elf_file, DR_PARAM_IN ELF_ADDR entry_point,
                 DR_PARAM_IN ELF_OFF section_header_table_offset,
                 DR_PARAM_IN ELF_OFF flags, DR_PARAM_IN ELF_OFF program_header_count,
                 DR_PARAM_IN ELF_OFF section_header_count,
                 DR_PARAM_IN ELF_OFF section_string_table_index)
{
    ELF_HEADER_TYPE ehdr;
    ehdr.e_ident[0] = ELFMAG0;
    ehdr.e_ident[1] = ELFMAG1;
    ehdr.e_ident[2] = ELFMAG2;
    ehdr.e_ident[3] = ELFMAG3;
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_LINUX;
    ehdr.e_ident[EI_ABIVERSION] = 0;
    ehdr.e_type = ET_CORE;
    ehdr.e_machine = IF_AARCHXX_ELSE(EM_AARCH64, EM_X86_64);
    ehdr.e_version = EV_CURRENT;
    /* This is the memory address of the entry point from where the process starts
     * executing. */
    ehdr.e_entry = entry_point;
    /* Points to the start of the program header table. */
    ehdr.e_phoff = sizeof(ehdr);
    /* Points to the start of the section header table. */
    ehdr.e_shoff = section_header_table_offset;
    ehdr.e_flags = 0;
    /* Contains the size of this header */
    ehdr.e_ehsize = sizeof(ehdr);
    /* Contains the size of a program header table entry. As explained below, this will
     * typically be 0x20 (32 bit) or 0x38 (64 bit). */
    ehdr.e_phentsize = sizeof(ELF_PROGRAM_HEADER_TYPE);
    /* Contains the number of entries in the program header table. */
    ehdr.e_phnum = program_header_count;
    ehdr.e_shentsize = sizeof(ELF_SECTION_HEADER_TYPE);
    /* Contains the number of entries in the section header table. */
    ehdr.e_shnum = section_header_count;
    /* Contains index of the section header table entry that contains the section names.
     */
    ehdr.e_shstrndx = section_string_table_index;

    return os_write(elf_file, (void *)&ehdr, sizeof(ehdr)) == sizeof(ehdr);
}

/*
 * Writes a program header to the file. Returns true if the program header is written to
 * the core dump file, false otherwise.
 */
static bool
write_program_header(DR_PARAM_IN file_t elf_file, DR_PARAM_IN ELF_WORD type,
                     DR_PARAM_IN ELF_WORD flags, DR_PARAM_IN ELF_OFF offset,
                     DR_PARAM_IN ELF_ADDR virtual_address,
                     DR_PARAM_IN ELF_ADDR physical_address,
                     DR_PARAM_IN ELF_WORD file_size, DR_PARAM_IN ELF_WORD memory_size,
                     DR_PARAM_IN ELF_WORD alignment)
{
    ELF_PROGRAM_HEADER_TYPE phdr;
    phdr.p_type = type;              /* Segment type. */
    phdr.p_flags = flags;            /* Segment flags. */
    phdr.p_offset = offset;          /* Segment file offset. */
    phdr.p_vaddr = virtual_address;  /* Segment virtual address. */
    phdr.p_paddr = physical_address; /* Segment physical address. */
    phdr.p_filesz = file_size;       /* Segment size in file. */
    phdr.p_memsz = memory_size;      /* Segment size in memory. */
    phdr.p_align = alignment;        /* Segment alignment. */

    return os_write(elf_file, (void *)&phdr, sizeof(phdr)) == sizeof(phdr);
}

/*
 * Write a section header to the file. Returns true if the section header is written to
 * the core dump file, false otherwise.
 */
static bool
write_section_header(DR_PARAM_IN file_t elf_file,
                     DR_PARAM_IN ELF_WORD string_table_offset, DR_PARAM_IN ELF_WORD type,
                     DR_PARAM_IN ELF_WORD flags, DR_PARAM_IN ELF_ADDR virtual_address,
                     DR_PARAM_IN ELF_OFF offset, DR_PARAM_IN ELF_WORD section_size,
                     DR_PARAM_IN ELF_WORD link, DR_PARAM_IN ELF_WORD info,
                     DR_PARAM_IN ELF_WORD alignment, DR_PARAM_IN ELF_WORD entry_size)
{
    ELF_SECTION_HEADER_TYPE shdr;
    shdr.sh_name = string_table_offset; /* Section name (string tbl index). */
    shdr.sh_type = type;                /* Section type. */
    shdr.sh_flags = flags;              /* Section flags. */
    shdr.sh_addr = virtual_address;     /* Section virtual addr at execution. */
    shdr.sh_offset = offset;            /* Section file offset. */
    shdr.sh_size = section_size;        /* Section size in bytes. */
    shdr.sh_link = link;                /* Link to another section. */
    shdr.sh_info = info;                /* Additional section information. */
    shdr.sh_addralign = alignment;      /* Section alignment. */
    shdr.sh_entsize = entry_size;       /* Entry size if section holds table. */

    return os_write(elf_file, (void *)&shdr, sizeof(shdr)) == sizeof(shdr);
}

/*
 * Copy dr_mcontext_t register values to user_regs_struct.
 */
static void
mcontext_to_user_regs(DR_PARAM_IN priv_mcontext_t *mcontext,
                      DR_PARAM_OUT struct user_regs_struct *regs)
{
#ifdef DR_HOST_NOT_TARGET
    return;
#elif defined(X86)
    regs->rax = mcontext->rax;
    regs->rcx = mcontext->rcx;
    regs->rdx = mcontext->rdx;
    regs->rbx = mcontext->rbx;
    regs->rsp = mcontext->rsp;
    regs->rbp = mcontext->rbp;
    regs->rsi = mcontext->rsi;
    regs->rdi = mcontext->rdi;
    regs->r8 = mcontext->r8;
    regs->r9 = mcontext->r9;
    regs->r10 = mcontext->r10;
    regs->r11 = mcontext->r11;
    regs->r12 = mcontext->r12;
    regs->r13 = mcontext->r13;
    regs->r14 = mcontext->r14;
    regs->r15 = mcontext->r15;
    regs->rip = (uint64_t)mcontext->rip;
    regs->eflags = mcontext->rflags;
#elif defined(AARCH64)
    regs->regs[0] = mcontext->r0;
    regs->regs[1] = mcontext->r1;
    regs->regs[2] = mcontext->r2;
    regs->regs[3] = mcontext->r3;
    regs->regs[4] = mcontext->r4;
    regs->regs[5] = mcontext->r5;
    regs->regs[6] = mcontext->r6;
    regs->regs[7] = mcontext->r7;
    regs->regs[8] = mcontext->r8;
    regs->regs[9] = mcontext->r9;
    regs->regs[10] = mcontext->r10;
    regs->regs[11] = mcontext->r11;
    regs->regs[12] = mcontext->r12;
    regs->regs[13] = mcontext->r13;
    regs->regs[14] = mcontext->r14;
    regs->regs[15] = mcontext->r15;
    regs->regs[16] = mcontext->r16;
    regs->regs[17] = mcontext->r17;
    regs->regs[18] = mcontext->r18;
    regs->regs[19] = mcontext->r19;
    regs->regs[20] = mcontext->r20;
    regs->regs[21] = mcontext->r21;
    regs->regs[22] = mcontext->r22;
    regs->regs[23] = mcontext->r23;
    regs->regs[24] = mcontext->r24;
    regs->regs[25] = mcontext->r25;
    regs->regs[26] = mcontext->r26;
    regs->regs[27] = mcontext->r27;
    regs->regs[28] = mcontext->r28;
    regs->regs[29] = mcontext->r29;
    regs->regs[30] = mcontext->r30;
    regs->sp = mcontext->sp;
    regs->pc = (uint64_t)mcontext->pc;
#else
#    error Unsupported architecture
#endif
}

/*
 * Write prstatus structure to the file. Returns true if the note is written to
 * the file, false otherwise.
 */
static bool
write_prstatus_note(DR_PARAM_IN priv_mcontext_t *mc, DR_PARAM_IN file_t elf_file)
{
    struct elf_prstatus prstatus;
    struct user_regs_struct *regs = (struct user_regs_struct *)&prstatus.pr_reg;
    mcontext_to_user_regs(mc, regs);

    ELF_NOTE_HEADER_TYPE nhdr;
    // Add one to include the terminating null character.
    nhdr.n_namesz = strlen(NOTE_OWNER) + 1;
    nhdr.n_descsz = sizeof(prstatus);
    nhdr.n_type = NT_PRSTATUS;
    if (os_write(elf_file, &nhdr, sizeof(nhdr)) != sizeof(nhdr)) {
        return false;
    }
    if (os_write(elf_file, NOTE_OWNER, NOTE_OWNER_LENGTH) != NOTE_OWNER_LENGTH) {
        return false;
    }
    return os_write(elf_file, &prstatus, sizeof(prstatus)) == sizeof(prstatus);
}

/*
 * Copy register values from dr_mcontext_t to elf_fpregset_t for AARCH64. For
 * X86, copy the floating point registers from the output of fxsave64.
 */
static bool
get_floating_point_registers(DR_PARAM_IN dcontext_t *dcontext,
                             DR_PARAM_IN priv_mcontext_t *mc,
                             DR_PARAM_OUT elf_fpregset_t *regs)
{
#ifdef DR_HOST_NOT_TARGET
    return false;
#elif defined(X86)
    // Fast FP save and restore support is required.
    ASSERT(proc_has_feature(FEATURE_FXSR));
    // Mixed mode is not supported.
    ASSERT(X64_MODE_DC(dcontext));
    byte fpstate_buf[MAX_FP_STATE_SIZE];
    fxsave64_map_t *fpstate =
        (fxsave64_map_t *)ALIGN_FORWARD(fpstate_buf, DR_FPSTATE_ALIGN);
    if (proc_save_fpstate((byte *)fpstate) != DR_FPSTATE_BUF_SIZE) {
        return false;
    }

    regs->cwd = fpstate->fcw;
    regs->swd = fpstate->fsw;
    regs->ftw = fpstate->ftw;
    regs->fop = fpstate->fop;
    regs->mxcsr = fpstate->mxcsr;
    regs->mxcr_mask = fpstate->mxcsr;
    /* 8*16 bytes for each FP-reg = 128 bytes */
    for (int i = 0; i < 32; ++i) {
        regs->st_space[i] = fpstate->st_space[i];
    }
    /* 16*16 bytes for each XMM-reg = 256 bytes */
    for (int i = 0; i < 64; ++i) {
        regs->xmm_space[i] = fpstate->xmm_space[i];
    }
    return true;
#elif defined(AARCH64)
    regs->fpsr = mc->fpsr;
    regs->fpcr = mc->fpcr;
    for (int i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; ++i) {
        memcpy(&regs->vregs[i], &mc->simd[i].q, sizeof(mc->simd[i].q));
    }
    return true;
#else
    return false;
#endif
}

/*
 * Write floating point registers to the file. Returns true if the note is written to
 * the file, false otherwise.
 */
static bool
write_fpregset_note(DR_PARAM_IN dcontext_t *dcontext, DR_PARAM_IN priv_mcontext_t *mc,
                    DR_PARAM_IN file_t elf_file)
{
    elf_fpregset_t fpregset;
    if (!get_floating_point_registers(dcontext, mc, &fpregset)) {
        return false;
    }

    ELF_NOTE_HEADER_TYPE nhdr;
    // Add one to include the terminating null character.
    nhdr.n_namesz = strlen(NOTE_OWNER) + 1;
    nhdr.n_descsz = sizeof(fpregset);
    nhdr.n_type = NT_FPREGSET;
    if (os_write(elf_file, &nhdr, sizeof(nhdr)) != sizeof(nhdr)) {
        return false;
    }
    if (os_write(elf_file, NOTE_OWNER, NOTE_OWNER_LENGTH) != NOTE_OWNER_LENGTH) {
        return false;
    }
    return os_write(elf_file, &fpregset, sizeof(fpregset)) == sizeof(fpregset);
}

#if defined(AARCH64)
/*
 * Write aarch64 tpidr_el0 register to the file. This function is only
 * applicable for aarch64. Returns true if the note is written to
 * the file, false otherwise.
 */
static bool
write_arm_tls_note(DR_PARAM_IN dcontext_t *dcontext, DR_PARAM_IN file_t elf_file)
{
    ELF_NOTE_HEADER_TYPE nhdr;
    // Add one to include the terminating null character.
    nhdr.n_namesz = strlen(NOTE_OWNER) + 1;
    nhdr.n_descsz = sizeof(reg_t);
    nhdr.n_type = NT_ARM_TLS;
    if (os_write(elf_file, &nhdr, sizeof(nhdr)) != sizeof(nhdr)) {
        return false;
    }
    if (os_write(elf_file, NOTE_OWNER, NOTE_OWNER_LENGTH) != NOTE_OWNER_LENGTH) {
        return false;
    }
    const reg_t tpidr_el0 = (reg_t)os_get_app_tls_base(dcontext, TLS_REG_LIB);
    return os_write(elf_file, &tpidr_el0, sizeof(tpidr_el0)) == sizeof(tpidr_el0);
}
#endif

/*
 * Writes a memory dump file in ELF format. Returns true if a core dump file is written,
 * false otherwise.
 */
static bool
os_dump_core_internal(dcontext_t *dcontext, const char *output_directory DR_PARAM_IN,
                      char *path DR_PARAM_OUT, size_t path_sz)
{
    priv_mcontext_t mc;
    if (!dr_get_mcontext_priv(dcontext, NULL, &mc))
        return false;

    // Insert a null string at the beginning for sections with no comment.
    char string_table[MAX_SECTION_NAME_BUFFER_SIZE];
    // Reserve the first byte for sections without a name.
    string_table[0] = '\0';
    string_table[1] = '\0';
    ELF_ADDR string_table_offset = 1;
    ELF_OFF section_count = 0;
    ELF_OFF section_data_size = 0;
    // Reserve a section for the section name string table.
    section_header_info_t section_header_info[MAX_SECTION_HEADERS + 1];

    memquery_iter_t iter;
    if (!memquery_iterator_start(&iter, NULL, /*may_alloc=*/true)) {
        SYSLOG_INTERNAL_ERROR("memquery_iterator_start failed.");
        return false;
    }

    ASSERT(d_r_get_num_threads() == 1);
    // When GLOBAL_DCONTEXT is used to create a hash table, the HASHTABLE_SHARED
    // flag has to be set. With the HASHTABLE_SHARED flag, a lock has to be used even
    // though all other threads have been suspended.
    strhash_table_t *string_htable = strhash_hash_create(
        GLOBAL_DCONTEXT, /*bits=*/8, /*load_factor_percent=*/80,
        /*table_flags=*/HASHTABLE_SHARED, NULL _IF_DEBUG("mmap-string-table"));

    // Iterate through memory regions to store the start, end, protection, and
    // the offset to the section name string table. The first byte of the
    // section name table is a null character for regions without a comment. The
    // section name table is built based on the name of the memory region stored
    // in iter.comment. Region names are stored in the section name table
    // without duplications. An offset is used in the section header to locate
    // the section name in the section name table.
    while (memquery_iterator_next(&iter)) {
        // Skip non-readable section.
        if (iter.prot == MEMPROT_NONE || strcmp(iter.comment, VVAR_SECTION) == 0 ||
            strcmp(iter.comment, VSYSCALL_SECTION) == 0) {
            continue;
        }
        ELF_ADDR offset = 0;
        if (iter.comment != NULL && iter.comment[0] != '\0') {
            TABLE_RWLOCK(string_htable, write, lock);
            offset = (ELF_ADDR)strhash_hash_lookup(GLOBAL_DCONTEXT, string_htable,
                                                   iter.comment);
            if (offset == 0) {
                strhash_hash_add(GLOBAL_DCONTEXT, string_htable, iter.comment,
                                 (void *)string_table_offset);
                const size_t comment_len = d_r_strlen(iter.comment);
                if (comment_len + string_table_offset > MAX_SECTION_NAME_BUFFER_SIZE) {
                    SYSLOG_INTERNAL_ERROR("Section name table is too small to store "
                                          "all the section names.");
                    return false;
                }
                offset = string_table_offset;
                d_r_strncpy(&string_table[string_table_offset], iter.comment,
                            comment_len);
                string_table_offset += comment_len + 1;
                string_table[string_table_offset - 1] = '\0';
            }
            TABLE_RWLOCK(string_htable, write, unlock);
        }
        section_header_info[section_count].vm_start = iter.vm_start;
        section_header_info[section_count].vm_end = iter.vm_end;
        section_header_info[section_count].prot = iter.prot;
        section_header_info[section_count].name_offset = offset;
        section_data_size += iter.vm_end - iter.vm_start;
        ++section_count;
        if (section_count > MAX_SECTION_HEADERS) {
            SYSLOG_INTERNAL_ERROR("Too many section headers.");
            return false;
        }
    }
    strhash_hash_destroy(GLOBAL_DCONTEXT, string_htable);
    memquery_iterator_stop(&iter);

    // Add the string table section. Append the section name ".shstrtab" to the
    // section name table.
    const size_t section_header_table_len = d_r_strlen(SECTION_HEADER_TABLE) + 1;
    d_r_strncpy(&string_table[string_table_offset], SECTION_HEADER_TABLE,
                section_header_table_len);
    string_table_offset += section_header_table_len;
    ++section_count;
    section_data_size += string_table_offset;

    file_t elf_file;
    char dump_core_file_name[MAXIMUM_PATH];
    if (!get_unique_logfile(".elf", dump_core_file_name, sizeof(dump_core_file_name),
                            /*open_directory=*/false, output_directory,
                            /*embed_timestamp=*/true, &elf_file) ||
        elf_file == INVALID_FILE) {
        SYSLOG_INTERNAL_ERROR("Unable to open the core dump file.");
        return false;
    }
    if (path != NULL) {
        d_r_strncpy(path, dump_core_file_name, path_sz);
    }
    // We use two types of program headers. NOTE is used to store prstatus
    // structure and floating point registers. LOAD is used to specify loadable
    // segments. All but one section (shstrtab which stores section names)
    // require a corresponding LOAD program header. The total number of
    // program headers equals the number of NOTE program header (1) and the number
    // of LOAD program header (number of sections minus one since shstrtab does not
    // require a LOAD header).
    const ELF_OFF program_header_count = section_count;
    // core_file_offset is the current offset of the core file to write the next
    // section data.
    ELF_OFF core_file_offset = sizeof(ELF_HEADER_TYPE) +
        sizeof(ELF_PROGRAM_HEADER_TYPE) * program_header_count +
        PROGRAM_HEADER_NOTE_LENGTH;
    // We add padding to the core file so that loadable segments in the
    // core file are page size aligned. The p_align field in the program header
    // is applied to both memory and the offset to the core file.
    const size_t loadable_segment_padding =
        ALIGN_FORWARD(core_file_offset, os_page_size()) - core_file_offset;
    core_file_offset = ALIGN_FORWARD(core_file_offset, os_page_size());

    if (!write_elf_header(elf_file, /*entry_point=*/(uint64_t)mc.pc,
                          /*section_header_table_offset=*/sizeof(ELF_HEADER_TYPE) +
                              PROGRAM_HEADER_NOTE_LENGTH +
                              sizeof(ELF_PROGRAM_HEADER_TYPE) * program_header_count +
                              loadable_segment_padding + section_data_size,
                          /*flags=*/0,
                          /*program_header_count=*/program_header_count,
                          /*section_header_count=*/section_count,
                          /*section_string_table_index=*/section_count - 1)) {
        os_close(elf_file);
        return false;
    }
    // Write program header table entry.
    if (!write_program_header(elf_file, PT_NOTE, /*flags=*/0,
                              /*offset=*/sizeof(ELF_HEADER_TYPE) +
                                  sizeof(ELF_PROGRAM_HEADER_TYPE) * program_header_count,
                              /*virtual_address=*/0,
                              /*physical_address=*/0,
                              /*file_size=*/PROGRAM_HEADER_NOTE_LENGTH,
                              /*memory_size=*/PROGRAM_HEADER_NOTE_LENGTH,
                              /*alignment=*/sizeof(ELF_HALF))) {
        os_close(elf_file);
        return false;
    }
    // Write loadable program segment program headers.
    // TODO i#7046: Merge adjacent sections with the same prot values.
    // The last section is shstrtab which stores the section names and it does
    // not require a LOAD program header.
    for (int section_index = 0; section_index < section_count - 1; ++section_index) {
        ELF_WORD flags = 0;
        if (TEST(PROT_EXEC, section_header_info[section_index].prot)) {
            flags |= PF_X;
        }
        if (TEST(PROT_WRITE, section_header_info[section_index].prot)) {
            flags |= PF_W;
        }
        if (TEST(PROT_READ, section_header_info[section_index].prot)) {
            flags |= PF_R;
        }
        const ELF_WORD size = section_header_info[section_index].vm_end -
            section_header_info[section_index].vm_start;
        if (!write_program_header(
                elf_file, PT_LOAD, flags,
                /*offset=*/core_file_offset,
                /*virtual_address=*/(ELF_ADDR)section_header_info[section_index].vm_start,
                /*physical_address=*/
                (ELF_ADDR)section_header_info[section_index].vm_start,
                /*file_size=*/size,
                /*memory_size=*/size,
                /*alignment=*/os_page_size())) {
            os_close(elf_file);
            return false;
        }
        core_file_offset += section_header_info[section_index].vm_end -
            section_header_info[section_index].vm_start;
    }
    if (!write_prstatus_note(&mc, elf_file)) {
        os_close(elf_file);
        return false;
    }
    if (!write_fpregset_note(dcontext, &mc, elf_file)) {
        os_close(elf_file);
        return false;
    }
    // XXX: We need to add support for model specific registers like FS and GS for
    // x86_64.
#if defined(AARCH64)
    if (!write_arm_tls_note(dcontext, elf_file)) {
        os_close(elf_file);
        return false;
    }
#endif
    // Add padding to the core file such that loadable segments are aligned to
    // the page size in the core file.
    char buffer[MAX_BUFFER_SIZE];
    size_t remaining_bytes = loadable_segment_padding;
    memset(buffer, 0, MIN(MAX_BUFFER_SIZE, loadable_segment_padding));
    while (remaining_bytes > 0) {
        const size_t bytes = MIN(remaining_bytes, MAX_BUFFER_SIZE);
        if (os_write(elf_file, buffer, bytes) != bytes) {
            SYSLOG_INTERNAL_ERROR("Failed to add padding to the core dump file.");
            os_close(elf_file);
            return false;
        }
        remaining_bytes -= bytes;
    }
    // Write memory content to the core dump file.
    for (int section_index = 0; section_index < section_count - 1; ++section_index) {
        const size_t length = section_header_info[section_index].vm_end -
            section_header_info[section_index].vm_start;
        const size_t written = os_write(
            elf_file, (void *)section_header_info[section_index].vm_start, length);
        if (written != length) {
            SYSLOG_INTERNAL_ERROR("Failed to write the requested memory content into "
                                  "the core dump file.");
            SYSLOG_INTERNAL_ERROR(
                "section: %s, prot: %x, length: %d, written: %d\n",
                &string_table[section_header_info[section_index].name_offset],
                section_header_info[section_index].prot, length, written);
            os_close(elf_file);
            return false;
        }
    }
    // Write the section name section.
    if (os_write(elf_file, (void *)string_table, string_table_offset) !=
        string_table_offset) {
        SYSLOG_INTERNAL_ERROR("Failed to write section name string table into the "
                              "core dump file.");
        os_close(elf_file);
        return false;
    }
    // Write section headers to the core dump file.
    core_file_offset = sizeof(ELF_HEADER_TYPE) +
        sizeof(ELF_PROGRAM_HEADER_TYPE) * program_header_count +
        PROGRAM_HEADER_NOTE_LENGTH + loadable_segment_padding;

    // The section_count includes the section name section, so we need to skip
    // it in the loop. The section name section is handled differently after
    // this loop.
    for (int section_index = 0; section_index < section_count - 1; ++section_index) {
        ELF_WORD flags = SHF_ALLOC | SHF_MERGE;
        if (TEST(PROT_WRITE, section_header_info[section_index].prot)) {
            flags |= SHF_WRITE;
        }
        if (TEST(PROT_EXEC, section_header_info[section_index].prot)) {
            flags |= SHF_EXECINSTR;
        }
        if (!write_section_header(
                elf_file, section_header_info[section_index].name_offset, SHT_PROGBITS,
                flags, (ELF_ADDR)section_header_info[section_index].vm_start,
                core_file_offset,
                section_header_info[section_index].vm_end -
                    section_header_info[section_index].vm_start,
                /*link=*/0,
                /*info=*/0, /*alignment=*/sizeof(ELF_WORD),
                /*entry_size=*/0)) {
            os_close(elf_file);
            return false;
        }
        core_file_offset += section_header_info[section_index].vm_end -
            section_header_info[section_index].vm_start;
    }
    // Write the section name section.
    if (!write_section_header(
            elf_file, string_table_offset - strlen(SECTION_HEADER_TABLE), SHT_STRTAB,
            /*flags=*/0, /*virtual_address=*/0, core_file_offset,
            /*section_size=*/string_table_offset, /*link=*/0,
            /*info=*/0, /*alignment=*/1,
            /*entry_size=*/0)) {
        os_close(elf_file);
        return false;
    }

    os_close(elf_file);
    return true;
}

/*
 * Returns true if a core dump file is written, false otherwise.
 */
bool
os_dump_core_live(dcontext_t *dcontext, char *output_directory DR_PARAM_IN,
                  char *path DR_PARAM_OUT, size_t path_sz)
{
#ifdef DR_HOST_NOT_TARGET
    // Memory dump is supported only when the host and the target are the same.
    return false;
#endif
    // Suspend all threads including native threads to ensure the memory regions
    // do not change in the middle of the core dump.
    int num_threads;
    thread_record_t **threads;
    if (!synch_with_all_threads(THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER,
                                &threads, &num_threads, THREAD_SYNCH_NO_LOCKS_NO_XFER,
                                /* If we fail to suspend a thread, there is a
                                 * risk of deadlock in the child, so it's worth
                                 * retrying on failure.
                                 */
                                THREAD_SYNCH_SUSPEND_FAILURE_IGNORE)) {
        return false;
    }

    // TODO i#7046: Add support to save register values for all threads.
    const bool ret = os_dump_core_internal(dcontext, output_directory, path, path_sz);

    end_synch_with_all_threads(threads, num_threads,
                               /*resume=*/true);
    return ret;
}
