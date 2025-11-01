/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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

/* DrPoints: Basic Block Vector (BBV) Client.
 *
 * Given a user-defined instruction interval, computes the BBVs (histogram of BB
 * frequencies within the interval) of a program execution and outputs them in a .bb file.
 */

#include "dr_api.h"
#include "dr_events.h"
#include "drmgr.h"
#include "droption.h"
#include "drreg.h"
#include "drvector.h"
#include "drx.h"
#include "hashtable.h"

namespace dynamorio {
namespace drpoints {
namespace {

using ::dynamorio::droption::bytesize_t;
using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;
using ::dynamorio::droption::droption_t;

#define FATAL(...)                       \
    do {                                 \
        dr_fprintf(STDERR, __VA_ARGS__); \
        dr_abort();                      \
    } while (0)

#define HASH_BITS 13

#define MINSERT instrlist_meta_preinsert

#ifdef X86_64
#    define INLINE_COUNTER_UPDATE 1
#else
// TODO i#7685: We don't have the inlining implementation yet for AARCH64 or 32-bit
// architectures.
#endif

static droption_t<bytesize_t> instr_interval(
    DROPTION_SCOPE_CLIENT, "instr_interval", 100000000 /*=100M instructions*/,
    "The instruction interval to generate BBVs",
    "Divides the program execution in instruction intervals of the specified size and "
    "generates BBVs as the BB execution frequency within the interval times the number "
    "of instructions in the BB. Default is 100M instructions.");

static droption_t<bool>
    print_to_stdout(DROPTION_SCOPE_CLIENT, "print_to_stdout", false,
                    "Enables printing the Basic Block Vectors to standard output",
                    "Also prints to standard output on top of generating the .bbv file. "
                    "Default is false.");

// Global hash table that maps the PC of a BB's first instruction to a unique,
// increasing ID that comes from unique_bb_count.
static hashtable_t pc_to_id_map;

// Global hash table to keep track of the execution count of BBs.
// Key: unique BB ID, value: execution count in terms of BB instruction size.
static hashtable_t bb_count_table;

// Global unique BB counter used as ID. It must start from 1.
static uint64_t unique_bb_count = 1;

// Global instruction counter to keep track of when we reach the end of the user-defined
// instruction interval.
static uint64_t instr_count = 0;

// List of Basic Block Vectors (BBVs).
// This is a vector of vector pointers. Each element is a vector of pairs
// <BB_ID, execution_count * BB_size> of type bb_id_count_pair_t.
static drvector_t bbvs;

// Keeps track of the number of threads of the application. We abort when we detect a
// multi-threaded application as it's not supported yet.
static int thread_count = 0;

typedef struct _bb_id_count_pair_t {
    uint64_t id;    // Derived from unique_bb_count.
    uint64_t count; // execution_count * BB_size.
} bb_id_count_pair_t;

static void
free_id_count_pair(void *entry)
{
    dr_global_free(entry, sizeof(bb_id_count_pair_t));
}

static void
free_bbv(void *entry)
{
    drvector_t *vector = (drvector_t *)entry;
    if (!drvector_delete(vector))
        FATAL("ERROR: BBV drvector not deleted");
    dr_global_free(vector, sizeof(drvector_t));
}

static void
free_hit_count(void *entry)
{
    uint64_t *hit_count = (uint64_t *)entry;
    dr_global_free(hit_count, sizeof(uint64_t));
}

static void
add_to_bbv(void *key, void *payload, void *user_data)
{
    uint64_t count = *((uint64_t *)payload);
    // Skip BBs that were not executed in the current instruction interval.
    if (count == 0)
        return;
    uint64_t id = (uint64_t)(ptr_int_t)key;

    bb_id_count_pair_t *id_count_pair =
        (bb_id_count_pair_t *)dr_global_alloc(sizeof(bb_id_count_pair_t));
    id_count_pair->id = id;
    id_count_pair->count = count;

    // Add BB frequency to BBV.
    drvector_t *bbv = (drvector_t *)user_data;
    drvector_append(bbv, id_count_pair);
}

// We add execution counters to the table at instrumentation time. We cannot remove them
// from the hit_count_map when we reach the instruction interval at execution time, or the
// next interval won't have a execution counter. So, we set them to zero.
static void
set_count_to_zero(void *payload)
{
    uint64_t *count = ((uint64_t *)payload);
    *count = 0;
}

static void
save_bbv()
{
    // Clear global instruction count.
    // Currently we only support single-threaded applications, so we don't use any locking
    // mechanism to set this global counter.
    instr_count = 0;

    // Save current bb_count_table (i.e., the BBV for the current instruction
    //  interval).
    drvector_t *bbv = (drvector_t *)dr_global_alloc(sizeof(drvector_t));
    // We overshoot the initial size of the BBV vector to avoid resizing it.
    drvector_init(bbv, bb_count_table.entries, /*synch=*/false, free_id_count_pair);
    // Iterate over the non-zero elements of bb_count_table and add them to the BBV.
    hashtable_apply_to_all_key_payload_pairs_user_data(&bb_count_table, add_to_bbv, bbv);
    // Add the newly formed BBV to the list of BBVs.
    drvector_append(&bbvs, bbv);

    // Clear bb_count_table setting all the execution counts to zero.
    hashtable_apply_to_all_payloads(&bb_count_table, set_count_to_zero);
}

#ifndef INLINE_COUNTER_UPDATE
static void
increment_counters_and_save_bbv(uint64_t bb_id, uint64_t bb_size)
{
    // Increase execution count for the BB.
    uint64_t *hit_count =
        (uint64_t *)hashtable_lookup(&bb_count_table, (void *)(ptr_int_t)bb_id);
    (*hit_count) += bb_size;

    // Incrase instruction count of the interval by the BB #instructions.
    instr_count += bb_size;

    // We reached the end of the instruction interval.
    if (instr_count > instr_interval.get_value()) {
        save_bbv();
    }
}
#endif

static void
event_thread_init(void *drcontext)
{
    if (thread_count > 0)
        FATAL("ERROR: Multi-threaded applications are not currently supported");
    ++thread_count;
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    // By default drmgr enables auto-predication, which predicates all instructions
    // with the predicate of the current instruction on ARM. We disable it here
    // because we want to unconditionally execute the following instrumentation.
    // Furthermore, when we compute the representative intervals of the program execution,
    // we want the count of instructions to be the same as perf (which also counts
    // instructions with a false predicate) for interval validation purposes.
    drmgr_disable_auto_predication(drcontext, bb);
    if (!drmgr_is_first_instr(drcontext, inst))
        return DR_EMIT_DEFAULT;

    // Get the BB ID.
    // TODO i#7685: don't rely on absolute PC values. Use drmodtrack library to compute
    // relative offset instead.
    app_pc bb_pc = instr_get_app_pc(instrlist_first_app(bb));
    void *bb_id_ptr = hashtable_lookup(&pc_to_id_map, (void *)bb_pc);
    uint64_t bb_id = (uint64_t)(ptr_int_t)bb_id_ptr;
    if (bb_id_ptr == NULL) {
        bb_id = unique_bb_count;
        hashtable_add(&pc_to_id_map, (void *)bb_pc, (void *)(ptr_int_t)bb_id);
        ++unique_bb_count;
    }

    uint64_t *bb_count_ptr =
        (uint64_t *)hashtable_lookup(&bb_count_table, (void *)(ptr_int_t)bb_id);
    if (bb_count_ptr == NULL) {
        // If execution count is not mapped to this BB, then add a new count to the table.
        // We cannot save the initial value of 0 directly in the (void *)payload because
        // NULL == 0 is used for lookup failure.
        uint64_t *bb_count = (uint64_t *)dr_global_alloc(sizeof(uint64_t));
        *bb_count = 0;
        hashtable_add(&bb_count_table, (void *)(ptr_int_t)bb_id, bb_count);
        bb_count_ptr = bb_count;
    }

    uint64_t bb_size = (uint64_t)drx_instrlist_app_size(bb);

#ifdef INLINE_COUNTER_UPDATE
#    ifdef X86_64
    reg_id_t scratch = DR_REG_NULL;
    uint64_t instr_interval_value = instr_interval.get_value();
    instr_t *skip_call = INSTR_CREATE_label(drcontext);
    // Increment the BB execution count by BB size in #instructions.
    drx_insert_counter_update(drcontext, bb, inst,
                              // We're using drmgr, so these slots
                              // here won't be used: drreg's slots will be.
                              static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1),
                              bb_count_ptr, static_cast<int>(bb_size), DRX_COUNTER_64BIT);

    // Increment the instruction count by BB size in #instructions.
    drx_insert_counter_update(drcontext, bb, inst,
                              // We're using drmgr, so these slots
                              // here won't be used: drreg's slots will be.
                              static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1),
                              &instr_count, static_cast<int>(bb_size), DRX_COUNTER_64BIT);
    if (drreg_reserve_aflags(drcontext, bb, inst) != DRREG_SUCCESS)
        FATAL("ERROR: failed to reserve aflags");
    if (instr_interval_value < INT_MAX) {
        // The user defined instruction interval is small enough to be an immediate.
        MINSERT(bb, inst,
                XINST_CREATE_cmp(
                    drcontext, OPND_CREATE_ABSMEM(&instr_count, OPSZ_8),
                    OPND_CREATE_INT32(static_cast<int32_t>(instr_interval_value))));
    } else {
        if (drreg_reserve_register(drcontext, bb, inst, NULL, &scratch) != DRREG_SUCCESS)
            FATAL("ERROR: failed to reserve scratch register");
        instrlist_insert_mov_immed_ptrsz(drcontext, instr_interval_value,
                                         opnd_create_reg(scratch), bb, inst, NULL, NULL);
        MINSERT(bb, inst,
                XINST_CREATE_cmp(drcontext, OPND_CREATE_ABSMEM(&instr_count, OPSZ_8),
                                 opnd_create_reg(scratch)));
    }
    // If the user defined instruction interval is reached, jump to a clean call of the
    // instrumentation function that saves the current BBV, otherwise jump to the rest of
    // the BB and continue.
    MINSERT(bb, inst, INSTR_CREATE_jcc(drcontext, OP_jle, opnd_create_instr(skip_call)));
    // Insert call to instrumentation function that saves the current BBV.
    dr_insert_clean_call(drcontext, bb, inst, (void *)save_bbv, /*save_fpstate=*/false,
                         0);
    MINSERT(bb, inst, skip_call);
    if (drreg_unreserve_aflags(drcontext, bb, inst) != DRREG_SUCCESS)
        DR_ASSERT(false);
    if (scratch != DR_REG_NULL) {
        if (drreg_unreserve_register(drcontext, bb, inst, scratch) != DRREG_SUCCESS)
            DR_ASSERT(false);
    }
#    else
#        error NYI
#    endif
#else
    // Default to a clean call to the instrumentation function that increments the
    // counter, checks if the user defined instruction interval is reached, and if so
    // saves the current BBV.
    dr_insert_clean_call(drcontext, bb, inst, (void *)increment_counters_and_save_bbv,
                         /*save_fpstate=*/false, 2, OPND_CREATE_INTPTR(bb_id),
                         OPND_CREATE_INTPTR(bb_size));
#endif
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    // Get the current working directory where drrun is executing. We save the .bbv file
    // there.
    char cwd[MAXIMUM_PATH];
    if (!dr_get_current_directory(cwd, sizeof(cwd)))
        FATAL("ERROR: dr_get_current_directory() failed");

    // Create and open the drpoints.PROC_BIN_NAME.PID.UNIQUE_ID.bbv file.
    char buf[MAXIMUM_PATH];
    file_t bbvs_file =
        drx_open_unique_appid_file(cwd, dr_get_process_id(), "drpoints", "bbv",
                                   DR_FILE_WRITE_REQUIRE_NEW, buf, sizeof(buf));
    if (bbvs_file == INVALID_FILE)
        FATAL("ERROR: unable to create BBVs file");

    // Define the format strings we use to write the file.
    const char *one_pair_only = "T:%d:%d \n";
    const char *first_pair = "T:%d:%d ";
    const char *middle_pair = ":%d:%d ";
    const char *last_pair = ":%d:%d \n";

    for (uint i = 0; i < bbvs.entries; ++i) {
        drvector_t *bbv = (drvector_t *)drvector_get_entry(&bbvs, i);
        for (uint j = 0; j < bbv->entries; ++j) {
            bb_id_count_pair_t *pair = (bb_id_count_pair_t *)drvector_get_entry(bbv, j);

            const char *format_string;
            if (j == 0 && j == bbv->entries - 1) {
                // Only one BB and related execution count for this BBV.
                format_string = one_pair_only;
            } else if (j == 0) {
                format_string = first_pair;
            } else if (j == bbv->entries - 1) {
                format_string = last_pair;
            } else {
                format_string = middle_pair;
            }

            char msg[64];
            int len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]), format_string,
                                  pair->id, pair->count);
            DR_ASSERT(len > 0);

            if (print_to_stdout.get_value())
                dr_fprintf(STDOUT, "%s", msg);

            dr_write_file(bbvs_file, msg, (size_t)len);
        }
    }

    dr_close_file(bbvs_file);

    // Free DR memory.
    hashtable_delete(&pc_to_id_map);
    hashtable_delete(&bb_count_table);
    if (!drvector_delete(&bbvs))
        FATAL("ERROR: BBVs drvector not deleted");

    drmgr_unregister_thread_init_event(event_thread_init);
    drmgr_unregister_exit_event(event_exit);
    drx_exit();
    drreg_exit();
    drmgr_exit();

    // Avoid accumulation of option values on static-link re-attach.
    droption::droption_parser_t::clear_values();
}

} // namespace
} // namespace drpoints
} // namespace dynamorio

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    // Parse command-line options.
    if (!dynamorio::droption::droption_parser_t::parse_argv(
            dynamorio::droption::DROPTION_SCOPE_CLIENT, argc, argv, NULL, NULL))
        DR_ASSERT(false);

    dr_set_client_name("DrPoints", "http://dynamorio.org/issues");

    drreg_options_t ops = { sizeof(ops), 1 /*max slots needed: aflags*/, false };
    if (!drmgr_init() || !drx_init() || drreg_init(&ops))
        DR_ASSERT(false);

    // Register events.
    drmgr_register_exit_event(dynamorio::drpoints::event_exit);
    if (!drmgr_register_bb_instrumentation_event(
            NULL, dynamorio::drpoints::event_app_instruction, NULL) ||
        (!drmgr_register_thread_init_event(dynamorio::drpoints::event_thread_init))) {
        DR_ASSERT(false);
    }

    // Currently we only support single-threaded applications, so we don't use any locking
    // mechanism for the following global data structures.
    hashtable_init_ex(&dynamorio::drpoints::bb_count_table, HASH_BITS, HASH_INTPTR,
                      /*str_dup=*/false, /*synch=*/false,
                      dynamorio::drpoints::free_hit_count, NULL, NULL);
    hashtable_init_ex(&dynamorio::drpoints::pc_to_id_map, HASH_BITS, HASH_INTPTR,
                      /*str_dup=*/false, /*synch=*/false, NULL, NULL, NULL);
    drvector_init(&dynamorio::drpoints::bbvs, 0, /*synch=*/false,
                  dynamorio::drpoints::free_bbv);

    // Make it easy to tell, by looking at log file, which client executed.
    dr_log(NULL, DR_LOG_ALL, 1, "DrPoints initializing\n");
}
