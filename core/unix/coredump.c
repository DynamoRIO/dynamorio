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
#include <sys/mman.h>
#include "../globals.h"
#include "../os_shared.h"
#include "../hashtable.h"
#include "memquery.h"

#define MAX_SECTION_NAME_BUFFER_SIZE 8192
#define SECTION_HEADER_TABLE ".shstrtab"
#define VVAR_SECTION "[vvar]"

#ifdef X64
#    define ELF_HEADER_TYPE Elf64_Ehdr
#    define ELF_PROGRAM_HEADER_TYPE Elf64_Phdr
#    define ELF_SECTION_HEADER_TYPE Elf64_Shdr
#    define ELF_ADDR Elf64_Addr
#    define ELF_WORD Elf64_Xword
#    define ELF_OFF Elf64_Word
#else
#    define ELF_HEADER_TYPE Elf32_Ehdr
#    define ELF_PROGRAM_HEADER_TYPE Elf32_Phdr
#    define ELF_SECTION_HEADER_TYPE Elf32_Shdr
#    define ELF_ADDR Elf32_Addr
#    define ELF_WORD Elf32_Word
#    define ELF_OFF Elf32_Word
#endif

DECLARE_CXTSWPROT_VAR(static mutex_t dump_core_lock, INIT_LOCK_FREE(dump_core_lock));

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
    ehdr.e_ident[EI_DATA] = IF_AARCHXX_ELSE(ELFDATA2MSB, ELFDATA2LSB);
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

    return os_write(elf_file, (void *)&ehdr, sizeof(ELF_HEADER_TYPE)) ==
        sizeof(ELF_HEADER_TYPE);
}

/*
 * Writes a program header to the file. Returns true if the program header is written to
 * the core dump file, false otherwise.
 */
static bool
write_phdrs(DR_PARAM_IN file_t elf_file, DR_PARAM_IN ELF_WORD type,
            DR_PARAM_IN ELF_WORD flags, DR_PARAM_IN ELF_OFF offset,
            DR_PARAM_IN ELF_ADDR virtual_address, DR_PARAM_IN ELF_ADDR physical_address,
            DR_PARAM_IN ELF_WORD file_size, DR_PARAM_IN ELF_WORD memory_size,
            DR_PARAM_IN ELF_WORD alignment)
{
    ELF_PROGRAM_HEADER_TYPE phdr;
    phdr.p_type = type;              /* Segment type */
    phdr.p_flags = flags;            /* Segment flags */
    phdr.p_offset = offset;          /* Segment file offset */
    phdr.p_vaddr = virtual_address;  /* Segment virtual address */
    phdr.p_paddr = physical_address; /* Segment physical address */
    phdr.p_filesz = file_size;       /* Segment size in file */
    phdr.p_memsz = memory_size;      /* Segment size in memory */
    phdr.p_align = alignment;        /* Segment alignment */

    return os_write(elf_file, (void *)&phdr, sizeof(ELF_PROGRAM_HEADER_TYPE)) ==
        sizeof(ELF_PROGRAM_HEADER_TYPE);
}

/*
 * Write a section header to the file. Returns true if the section header is written to
 * the core dump file, false otherwise.
 */
static bool
write_shdr(DR_PARAM_IN file_t elf_file, DR_PARAM_IN ELF_WORD string_table_offset,
           DR_PARAM_IN ELF_WORD type, DR_PARAM_IN ELF_WORD flags,
           DR_PARAM_IN ELF_ADDR virtual_address, DR_PARAM_IN ELF_OFF offset,
           DR_PARAM_IN ELF_WORD section_size, DR_PARAM_IN ELF_WORD link,
           DR_PARAM_IN ELF_WORD info, DR_PARAM_IN ELF_WORD alignment,
           DR_PARAM_IN ELF_WORD entry_size)
{
    ELF_SECTION_HEADER_TYPE shdr;
    shdr.sh_name = string_table_offset; /* Section name (string tbl index) */
    shdr.sh_type = type;                /* Section type */
    shdr.sh_flags = flags;              /* Section flags */
    shdr.sh_addr = virtual_address;     /* Section virtual addr at execution */
    shdr.sh_offset = offset;            /* Section file offset */
    shdr.sh_size = section_size;        /* Section size in bytes */
    shdr.sh_link = link;                /* Link to another section */
    shdr.sh_info = info;                /* Additional section information */
    shdr.sh_addralign = alignment;      /* Section alignment */
    shdr.sh_entsize = entry_size;       /* Entry size if section holds table */

    return os_write(elf_file, (void *)&shdr, sizeof(ELF_SECTION_HEADER_TYPE)) ==
        sizeof(ELF_SECTION_HEADER_TYPE);
}

/*
 * Writes a memory dump file in ELF format. Returns true if a core dump file is written,
 * false otherwise.
 */
static bool
os_dump_core_internal(void)
{
    strhash_table_t *string_htable =
        strhash_hash_create(GLOBAL_DCONTEXT, /*bits=*/8, /*load_factor_percent=*/80,
                            /*table_flags=*/0, NULL _IF_DEBUG("mmap-string-table"));

    // Insert a null string at the beginning for sections with no comment.
    char string_table[MAX_SECTION_NAME_BUFFER_SIZE];
    string_table[0] = '\0';
    string_table[1] = '\0';
    ELF_ADDR string_table_offset = 1;
    ELF_OFF section_count = 0;
    ELF_OFF seciion_data_size = 0;
    memquery_iter_t iter;
    if (memquery_iterator_start(&iter, NULL, /*may_alloc=*/false)) {
        while (memquery_iterator_next(&iter)) {
            // Skip non-readable section.
            if (iter.prot == MEMPROT_NONE || strcmp(iter.comment, VVAR_SECTION) == 0) {
                continue;
            }
            ELF_ADDR offset = 0;
            if (iter.comment != NULL && iter.comment[0] != '\0') {
                offset = (ELF_ADDR)strhash_hash_lookup(GLOBAL_DCONTEXT, string_htable,
                                                       iter.comment);
                if (offset == 0 &&
                    (string_table[1] == '\0' ||
                     d_r_strcmp(string_table, iter.comment) != 0)) {
                    strhash_hash_add(GLOBAL_DCONTEXT, string_htable, iter.comment,
                                     (void *)string_table_offset);
                    offset = string_table_offset;
                    const size_t comment_len = d_r_strlen(iter.comment);
                    if (comment_len + string_table_offset >
                        MAX_SECTION_NAME_BUFFER_SIZE) {
                        SYSLOG_INTERNAL_ERROR("Section name table is too small to store "
                                              "all the section names.");
                        return false;
                    }
                    d_r_strncpy(&string_table[string_table_offset], iter.comment,
                                comment_len);
                    string_table_offset += comment_len + 1;
                    string_table[string_table_offset - 1] = '\0';
                }
            }
            seciion_data_size += iter.vm_end - iter.vm_start;
            ++section_count;
        }
        // Add the string table section. Append the section name ".shstrtab" to the
        // section names table.
        const int section_header_table_len = d_r_strlen(SECTION_HEADER_TABLE) + 1;
        d_r_strncpy(&string_table[string_table_offset], SECTION_HEADER_TABLE,
                    section_header_table_len);
        string_table_offset += section_header_table_len;
        ++section_count;
        seciion_data_size += string_table_offset;
        memquery_iterator_stop(&iter);
    }

    file_t elf_file;
    char dump_core_file_name[MAXIMUM_PATH];
    if (!get_unique_logfile(".elf", dump_core_file_name, sizeof(dump_core_file_name),
                            false, &elf_file) ||
        elf_file == INVALID_FILE) {
        SYSLOG_INTERNAL_ERROR("Unable to open the core dump file.");
        return false;
    }

    if (!write_elf_header(elf_file, /*entry_point=*/0,
                          /*section_header_table_offset*/ sizeof(ELF_HEADER_TYPE) +
                              1 /*program_header_count*/ *
                                  sizeof(ELF_PROGRAM_HEADER_TYPE) +
                              seciion_data_size,
                          /*flags=*/0,
                          /*program_header_count=*/1,
                          /*section_header_count=*/section_count,
                          /*section_string_table_index=*/section_count - 1)) {
        os_close(elf_file);
        return false;
    }
    // TODO i#7046: Fill the program header with valid data.
    if (!write_phdrs(elf_file, PT_NULL, PF_X, /*offset=*/0, /*virtual_address=*/0,
                     /*physical_address=*/0,
                     /*file_size=*/0, /*memory_size=*/0, /*alignment=*/0)) {
        os_close(elf_file);
        return false;
    }
    int total = 0;
    if (memquery_iterator_start(&iter, NULL, /*may_alloc=*/false)) {
        while (memquery_iterator_next(&iter)) {
            // Skip non-readable sections.
            if (iter.prot == MEMPROT_NONE || strcmp(iter.comment, VVAR_SECTION) == 0) {
                continue;
            }
            const size_t length = iter.vm_end - iter.vm_start;
            const int written = os_write(elf_file, (void *)iter.vm_start, length);
            if (written != length) {
                SYSLOG_INTERNAL_ERROR("Failed to write the requested memory content into "
                                      "the core dump file.");
                os_close(elf_file);
                return false;
            }
            total += length;
        }
        // Write the section names section.
        if (os_write(elf_file, (void *)string_table, string_table_offset) !=
            string_table_offset) {
            os_close(elf_file);
            return false;
        }
        memquery_iterator_stop(&iter);
    }

    if (memquery_iterator_start(&iter, NULL, /*may_alloc=*/false)) {
        // TODO i#7046: Handle multiple program headers.
        ELF_OFF file_offset = sizeof(ELF_HEADER_TYPE) +
            1 /*program_header_count*/ * sizeof(ELF_PROGRAM_HEADER_TYPE);
        while (memquery_iterator_next(&iter)) {
            // Skip non-readable section.
            if (iter.prot == MEMPROT_NONE || strcmp(iter.comment, VVAR_SECTION) == 0) {
                continue;
            }
            ELF_WORD flags = SHF_ALLOC | SHF_MERGE;
            if (iter.prot & PROT_WRITE) {
                flags |= SHF_WRITE;
            }
            ELF_ADDR name_offset = 0;
            if (iter.comment != NULL && iter.comment[0] != '\0') {
                name_offset = (ELF_ADDR)strhash_hash_lookup(GLOBAL_DCONTEXT,
                                                            string_htable, iter.comment);
            }
            if (!write_shdr(elf_file, name_offset, SHT_PROGBITS, flags,
                            (ELF_ADDR)iter.vm_start, file_offset,
                            iter.vm_end - iter.vm_start, /*link=*/0,
                            /*info=*/0, /*alignment=*/sizeof(ELF_WORD),
                            /*entry_size=*/0)) {
                os_close(elf_file);
                return false;
            }
            file_offset += iter.vm_end - iter.vm_start;
        }
        memquery_iterator_stop(&iter);
        // Write the section names section.
        if (!write_shdr(elf_file, string_table_offset - strlen(".shstrtab"), SHT_STRTAB,
                        /*flags=*/0, /*virtual_address=*/0, file_offset,
                        /*section_size=*/string_table_offset, /*link=*/0,
                        /*info=*/0, /*alignment=*/1,
                        /*entry_size=*/0)) {
            os_close(elf_file);
            return false;
        }
    }
    os_close(elf_file);
    strhash_hash_destroy(GLOBAL_DCONTEXT, string_htable);
    return true;
}

/*
 * Returns true if a core dump file is written, false otherwise.
 */
bool
os_dump_core_live(void)
{
    static thread_id_t current_dumping_thread_id VAR_IN_SECTION(NEVER_PROTECTED_SECTION) =
        0;
    thread_id_t current_id = d_r_get_thread_id();
#ifdef DEADLOCK_AVOIDANCE
    dcontext_t *dcontext = get_thread_private_dcontext();
    thread_locks_t *old_thread_owned_locks = NULL;
#endif

    if (current_id == current_dumping_thread_id) {
        return false; /* avoid infinite loop */
    }

#ifdef DEADLOCK_AVOIDANCE
    /* first turn off deadlock avoidance for this thread (needed for live dump
     * to try to grab all_threads and thread_initexit locks) */
    if (dcontext != NULL) {
        old_thread_owned_locks = dcontext->thread_owned_locks;
        dcontext->thread_owned_locks = NULL;
    }
#endif
    /* only allow one thread to dumpcore at a time, also protects static
     * buffers and current_dumping_thread_id */
    d_r_mutex_lock(&dump_core_lock);
    current_dumping_thread_id = current_id;
    const bool ret = os_dump_core_internal();
    current_dumping_thread_id = 0;
    d_r_mutex_unlock(&dump_core_lock);

#ifdef DEADLOCK_AVOIDANCE
    /* restore deadlock avoidance for this thread */
    if (dcontext != NULL) {
        dcontext->thread_owned_locks = old_thread_owned_locks;
    }
#endif
    return ret;
}
