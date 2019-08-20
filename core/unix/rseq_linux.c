/* *******************************************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/**************************************************************************************
 * Restartable sequence ("rseq") support (i#2350).
 * This is a kernel feature which provides cpu-atomic regions: if a thread
 * is pre-empted within an rseq region, an abort handler is invoked.
 * The feature is difficult to handle under binary instrumentation.
 * We rely on the app following certain conventions, including containing a
 * section holding a table of all rseq sequences.
 */

#include "../globals.h"
#include "../module_shared.h"
#include "module_private.h"
#include "os_private.h"
#include "rseq_linux.h"
#include "../fragment.h"
#include "decode.h"
#ifdef HAVE_RSEQ
#    include <linux/rseq.h>
#else
struct rseq_cs {
    uint version;
    uint flags;
    uint64 start_ip;
    uint64 post_commit_offset;
    uint64 abort_ip;
} __attribute__((aligned(4 * sizeof(uint64))));
struct rseq {
    uint cpu_id_start;
    uint cpu_id;
    uint64 ptr64;
    uint flags;
} __attribute__((aligned(4 * sizeof(uint64))));
#    define RSEQ_FLAG_UNREGISTER 1
#endif
#include "include/syscall.h"
#include <errno.h>

vm_area_vector_t *d_r_rseq_areas;
DECLARE_CXTSWPROT_VAR(static mutex_t rseq_trigger_lock,
                      INIT_LOCK_FREE(rseq_trigger_lock));
static volatile bool rseq_enabled;

typedef struct _rseq_region_t {
    app_pc start;
    app_pc end;
    app_pc handler;
    /* We need to preserve input registers for targeting "start" instead of "handler"
     * for our 2nd invocation, if they're written in the rseq region.  We only support
     * GPR inputs.  We document that we do not support any other inputs (no flags, no
     * SIMD registers).
     */
    bool reg_written[DR_NUM_GPR_REGS];
} rseq_region_t;

/* vmvector callbacks */
static void
rseq_area_free(void *data)
{
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, data, rseq_region_t, ACCT_VMAREAS, PROTECTED);
}

static void *
rseq_area_dup(void *data)
{
    rseq_region_t *src = (rseq_region_t *)data;
    rseq_region_t *dst =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, rseq_region_t, ACCT_VMAREAS, PROTECTED);
    ASSERT(src != NULL);
    *dst = *src;
    return dst;
}

void
d_r_rseq_init(void)
{
    VMVECTOR_ALLOC_VECTOR(d_r_rseq_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED | VECTOR_NEVER_MERGE, rseq_areas);
    vmvector_set_callbacks(d_r_rseq_areas, rseq_area_free, rseq_area_dup, NULL, NULL);
    if (rseq_is_registered_for_current_thread())
        rseq_enabled = true;
}

void
d_r_rseq_exit(void)
{
    vmvector_delete_vector(GLOBAL_DCONTEXT, d_r_rseq_areas);
    DELETE_LOCK(rseq_trigger_lock);
}

void
rseq_thread_attach(dcontext_t *dcontext)
{
    rseq_region_t *info;
    if (!vmvector_lookup_data(d_r_rseq_areas, dcontext->next_tag, NULL, NULL,
                              (void **)&info))
        return;
    /* The thread missed the save of its state on rseq entry.  We could try to save here
     * so the restore on rseq exit won't read incorrect values, but it's simpler and
     * less error-prone to send it to the abort handler, like we do on detach or other
     * translation points.
     */
    dcontext->next_tag = info->handler;
}

bool
rseq_get_region_info(app_pc pc, app_pc *start OUT, app_pc *end OUT, app_pc *handler OUT,
                     bool **reg_written OUT, int *reg_written_size OUT)
{
    rseq_region_t *info;
    if (!vmvector_lookup_data(d_r_rseq_areas, pc, start, end, (void **)&info))
        return false;
    if (handler != NULL)
        *handler = info->handler;
    if (reg_written != NULL)
        *reg_written = info->reg_written;
    if (reg_written_size != NULL)
        *reg_written_size = sizeof(info->reg_written) / sizeof(info->reg_written[0]);
    return true;
}

bool
rseq_is_registered_for_current_thread(void)
{
    /* Unfortunately there's no way to query the current rseq struct.
     * For 64-bit we can pass a kernel address and look for EFAULT
     * vs EINVAL, but there is no kernel address for 32-bit.
     * So we try to perform a legitimate registration.
     */
    struct rseq test_rseq = {};
    int res = dynamorio_syscall(SYS_rseq, 4, &test_rseq, sizeof(test_rseq), 0, 0);
    if (res == -EINVAL) /* Our struct != registered struct. */
        return true;
    if (res == -ENOSYS)
        return false;
    ASSERT(res == 0); /* If not, the struct size or sthg changed! */
    if (dynamorio_syscall(SYS_rseq, 4, &test_rseq, sizeof(test_rseq),
                          RSEQ_FLAG_UNREGISTER, 0) != 0) {
        ASSERT_NOT_REACHED();
    }
    return false;
}

static void
rseq_analyze_instructions(rseq_region_t *info)
{
    /* We analyze the instructions inside [start,end) looking for register state that we
     * need to preserve for our restart.  We do not want to blindly spill and restore
     * 16+ registers for every sequence (too much overhead).
     */
    instr_t instr;
    instr_init(GLOBAL_DCONTEXT, &instr);
    app_pc pc = info->start;
    int i;
    bool reached_cti = false;
    memset(info->reg_written, 0, sizeof(info->reg_written));
    while (pc < info->end) {
        instr_reset(GLOBAL_DCONTEXT, &instr);
        app_pc next_pc = decode(GLOBAL_DCONTEXT, pc, &instr);
        if (next_pc == NULL) {
            REPORT_FATAL_ERROR_AND_EXIT(RSEQ_BEHAVIOR_UNSUPPORTED, 3,
                                        get_application_name(), get_application_pid(),
                                        "Rseq sequence contains invalid instructions");
            ASSERT_NOT_REACHED();
        }
        if (instr_is_syscall(&instr)) {
            REPORT_FATAL_ERROR_AND_EXIT(RSEQ_BEHAVIOR_UNSUPPORTED, 3,
                                        get_application_name(), get_application_pid(),
                                        "Rseq sequence contains a system call");
            ASSERT_NOT_REACHED();
        }
        if (instr_is_call(&instr)) {
            REPORT_FATAL_ERROR_AND_EXIT(RSEQ_BEHAVIOR_UNSUPPORTED, 3,
                                        get_application_name(), get_application_pid(),
                                        "Rseq sequence contains a call");
            ASSERT_NOT_REACHED();
        }
        if (instr_is_cti(&instr))
            reached_cti = true;
        /* We potentially need to preserve any register written anywhere inside
         * the sequence.  We can't limit ourselves to registers clearly live on
         * input, since code *after* the sequence could read them.  We do disallow
         * callouts to helper functions to simplify our lives.
         *
         * We only preserve GPR's, for simplicity, and because they are far more likely
         * as inputs than flags or SIMD registers.  We'd like to verify that only GPR's
         * are used, but A) we can't easily check values read *after* the sequence (the
         * handler could set up state read afterward and sometimes clobbered inside), B)
         * we do want to support SIMD and flags writes in the sequence, and C) even
         * checking for values read in the sequence would want new interfaces like
         * DR_REG_START_SIMD or register iterators for reasonable code.
         */
        for (i = 0; i < DR_NUM_GPR_REGS; i++) {
            if (info->reg_written[i])
                continue;
            reg_id_t reg = DR_REG_START_GPR + (reg_id_t)i;
            if (instr_writes_to_reg(&instr, reg, DR_QUERY_DEFAULT)) {
                LOG(GLOBAL, LOG_LOADER, 3,
                    "Rseq region @" PFX " writes register %s at " PFX "\n", info->start,
                    reg_names[reg], pc);
                info->reg_written[i] = true;
            }
        }
        pc = next_pc;
    }
    instr_free(GLOBAL_DCONTEXT, &instr);
}

static void
rseq_process_entry(struct rseq_cs *entry, ssize_t load_offs)
{
    LOG(GLOBAL, LOG_LOADER, 2,
        "Found rseq region: ver=%u; flags=%u; start=" PFX "; end=" PFX "; abort=" PFX
        "\n",
        entry->version, entry->flags, entry->start_ip + load_offs,
        entry->start_ip + entry->post_commit_offset + load_offs,
        entry->abort_ip + load_offs);
    rseq_region_t *info =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, rseq_region_t, ACCT_VMAREAS, PROTECTED);
    info->start = (app_pc)(ptr_uint_t)entry->start_ip + load_offs;
    info->end = info->start + entry->post_commit_offset;
    info->handler = (app_pc)(ptr_uint_t)entry->abort_ip + load_offs;
    rseq_analyze_instructions(info);
    vmvector_add(d_r_rseq_areas, info->start, info->end, (void *)info);
    RSTATS_INC(num_rseq_regions);
    /* Check the start pc.  We don't take the effort to check for non-tags or
     * interior pc's.
     */
    if (fragment_lookup(GLOBAL_DCONTEXT, info->start) != NULL) {
        /* We rely on the app not running rseq code for non-rseq purposes (since we
         * can't easily tell the difference; plus we avoid a flush for lazy rseq
         * activation).
         */
        REPORT_FATAL_ERROR_AND_EXIT(
            RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(), get_application_pid(),
            "Rseq sequences must not be used for non-rseq purposes");
        ASSERT_NOT_REACHED();
    }
}

static void
rseq_process_elf_sections(module_area_t *ma, bool at_map,
                          ELF_SECTION_HEADER_TYPE *sec_hdr_start, const char *strtab,
                          ssize_t load_offs)
{
    bool found_array = false;
    uint i;
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)ma->start;
    ELF_SECTION_HEADER_TYPE *sec_hdr = sec_hdr_start;
    /* The section entries on disk need load_offs.  The rseq entries in memory are
     * relocated and only need the offset if relocations have not yet been applied.
     */
    ssize_t entry_offs = 0;
    if (at_map || (DYNAMO_OPTION(early_inject) && !dr_api_entry && !dynamo_started))
        entry_offs = load_offs;
    for (i = 0; i < elf_hdr->e_shnum; i++) {
#define RSEQ_PTR_ARRAY_SEC_NAME "__rseq_cs_ptr_array"
        if (strcmp(strtab + sec_hdr->sh_name, RSEQ_PTR_ARRAY_SEC_NAME) == 0) {
            found_array = true;
            byte **ptrs = (byte **)(sec_hdr->sh_addr + load_offs);
            int j;
            for (j = 0; j < sec_hdr->sh_size / sizeof(ptrs); ++j) {
                /* We require that the table is loaded.  If not, bail, but unlike
                 * failing to find section headers, make this a fatal error: better
                 * to notify the user than try to run the rseq w/o proper handling.
                 */
                if (ptrs < (byte **)ma->start || ptrs > (byte **)ma->end) {
                    REPORT_FATAL_ERROR_AND_EXIT(
                        RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(),
                        get_application_pid(),
                        RSEQ_PTR_ARRAY_SEC_NAME " is not in a loaded segment");
                    ASSERT_NOT_REACHED();
                }
                /* We assume this is a full mapping and it's safe to read the data
                 * (a partial map shouldn't make it to module list processing).
                 * We do perform a sanity check to handle unusual non-relocated
                 * cases (it's possible this array is not in a loaded segment?).
                 */
                byte *entry = *ptrs + entry_offs;
                if (entry < ma->start || entry > ma->end) {
                    REPORT_FATAL_ERROR_AND_EXIT(
                        RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(),
                        get_application_pid(),
                        RSEQ_PTR_ARRAY_SEC_NAME "'s entries are not in a loaded segment");
                    ASSERT_NOT_REACHED();
                }
                rseq_process_entry((struct rseq_cs *)entry, entry_offs);
                ++ptrs;
            }
            break;
        }
        ++sec_hdr;
    }
    if (!found_array) {
        sec_hdr = sec_hdr_start;
        for (i = 0; i < elf_hdr->e_shnum; i++) {
#define RSEQ_SEC_NAME "__rseq_cs"
#define RSEQ_OLD_SEC_NAME "__rseq_table"
            if (strcmp(strtab + sec_hdr->sh_name, RSEQ_SEC_NAME) == 0 ||
                strcmp(strtab + sec_hdr->sh_name, RSEQ_OLD_SEC_NAME) == 0) {
                /* There may be padding at the start of the section, so ensure we skip
                 * over it.  We're reading the loaded data, not the file, so it will
                 * always be aligned.
                 */
#define RSEQ_CS_ALIGNMENT (4 * sizeof(__u64))
                struct rseq_cs *array = (struct rseq_cs *)ALIGN_FORWARD(
                    sec_hdr->sh_addr + load_offs, RSEQ_CS_ALIGNMENT);
                int j;
                for (j = 0; j < sec_hdr->sh_size / sizeof(*array); ++j) {
                    /* We require that the table is loaded.  If not, bail. */
                    if (array < (struct rseq_cs *)ma->start ||
                        array > (struct rseq_cs *)ma->end) {
                        REPORT_FATAL_ERROR_AND_EXIT(
                            RSEQ_BEHAVIOR_UNSUPPORTED, 3, get_application_name(),
                            get_application_pid(),
                            RSEQ_SEC_NAME " is not in a loaded segment");
                        ASSERT_NOT_REACHED();
                    }
                    rseq_process_entry(array, entry_offs);
                    ++array;
                }
                break;
            }
            ++sec_hdr;
        }
    }
}

/* Returns whether successfully searched for rseq data (not whether found rseq data). */
static bool
rseq_process_module(module_area_t *ma, bool at_map)
{
    bool res = false;
    ASSERT(is_elf_so_header(ma->start, ma->end - ma->start));
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)ma->start;
    ASSERT(elf_hdr->e_shentsize == sizeof(ELF_SECTION_HEADER_TYPE));
    int fd = INVALID_FILE;
    byte *sec_map = NULL, *str_map = NULL;
    size_t sec_size = 0, str_size = 0;
    ELF_SECTION_HEADER_TYPE *sec_hdr = NULL;
    char *strtab;
    ssize_t load_offs = ma->start - ma->os_data.base_address;
    if (at_map && elf_hdr->e_shoff + ma->start < ma->end) {
        sec_map = elf_hdr->e_shoff + ma->start;
        sec_hdr = (ELF_SECTION_HEADER_TYPE *)sec_map;
        /* We assume strtab is there too. */
        strtab = (char *)(ma->start + sec_hdr[elf_hdr->e_shstrndx].sh_offset);
        if (strtab > (char *)ma->end)
            goto rseq_process_module_cleanup;
    } else {
        /* The section headers are not mapped in.  Unfortunately this is the common
         * case: they are typically at the end of the file.  For this reason, we delay
         * calling this function until we see the app use rseq.
         */
        if (ma->full_path == NULL)
            goto rseq_process_module_cleanup;
        fd = os_open(ma->full_path, OS_OPEN_READ);
        if (fd == INVALID_FILE)
            goto rseq_process_module_cleanup;
        off_t offs = ALIGN_BACKWARD(elf_hdr->e_shoff, PAGE_SIZE);
        sec_size =
            ALIGN_FORWARD(elf_hdr->e_shoff + elf_hdr->e_shnum * elf_hdr->e_shentsize,
                          PAGE_SIZE) -
            offs;
        sec_map =
            os_map_file(fd, &sec_size, offs, NULL, MEMPROT_READ, MAP_FILE_COPY_ON_WRITE);
        if (sec_map == NULL)
            goto rseq_process_module_cleanup;
        sec_hdr = (ELF_SECTION_HEADER_TYPE *)(sec_map + elf_hdr->e_shoff - offs);
        /* We also need the section header string table. */
        offs = ALIGN_BACKWARD(sec_hdr[elf_hdr->e_shstrndx].sh_offset, PAGE_SIZE);
        str_size = ALIGN_FORWARD(sec_hdr[elf_hdr->e_shstrndx].sh_offset +
                                     sec_hdr[elf_hdr->e_shstrndx].sh_size,
                                 PAGE_SIZE) -
            offs;
        str_map =
            os_map_file(fd, &str_size, offs, NULL, MEMPROT_READ, MAP_FILE_COPY_ON_WRITE);
        if (str_map == NULL)
            goto rseq_process_module_cleanup;
        strtab = (char *)(str_map + sec_hdr[elf_hdr->e_shstrndx].sh_offset - offs);
    }
    rseq_process_elf_sections(ma, at_map, sec_hdr, strtab, load_offs);
    res = true;
rseq_process_module_cleanup:
    if (str_size != 0)
        os_unmap_file(str_map, str_size);
    if (sec_size != 0)
        os_unmap_file(sec_map, sec_size);
    if (fd != INVALID_FILE)
        os_close(fd);
    DODEBUG({
        if (!res) {
            const char *name = GET_MODULE_NAME(&ma->names);
            if (name == NULL)
                name = "(null)";
            LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
                "%s: error looking for rseq table in %s\n", __FUNCTION__, name);
            if (strstr(name, "linux-vdso.so") == NULL) {
                SYSLOG_INTERNAL_WARNING_ONCE(
                    "Failed to identify whether a module has an rseq table");
            }
        }
    });
    return res;
}

/* Restartable sequence region identification.
 *
 * To avoid extra overhead going to disk to read section headers, we delay looking
 * for rseq data until the app invokes an rseq syscall (or on attach we see a thread
 * that has rseq set up).  We document that we do not handle the app using rseq
 * regions for non-rseq purposes, so we do not need to flush the cache here.
 */
void
rseq_locate_rseq_regions(void)
{
    if (rseq_enabled)
        return;
    /* This is a global operation, but the trigger could be hit by two threads at once,
     * thus requiring synchronization.
     */
    d_r_mutex_lock(&rseq_trigger_lock);
    if (rseq_enabled) {
        d_r_mutex_unlock(&rseq_trigger_lock);
        return;
    }
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    rseq_enabled = true;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    module_iterator_t *iter = module_iterator_start();
    while (module_iterator_hasnext(iter)) {
        module_area_t *ma = module_iterator_next(iter);
        rseq_process_module(ma, false /*!at_map*/);
    }
    module_iterator_stop(iter);
    d_r_mutex_unlock(&rseq_trigger_lock);
}

void
rseq_module_init(module_area_t *ma, bool at_map)
{
    if (rseq_enabled)
        rseq_process_module(ma, at_map);
}
