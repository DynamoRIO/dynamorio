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
#include "drvector.h"
#include "drx.h"
#include "hashtable.h"

namespace dynamorio {
namespace samples {
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

static droption_t<bytesize_t> instr_interval(
    DROPTION_SCOPE_CLIENT, "instr_interval", 100000000 /*=100M instructions*/,
    "The instruction interval to generate BBVs",
    "Divides the program execution in instruction intervals of the specified size and "
    "generates BBVs using the BB hit count frequency within the interval and the number "
    "of instructions in the BB. Default is 100M instructions.");

// Global hash table that maps the PC of a BB's first instruction to a unique,
// increasing ID that comes from unique_bb_count.
static hashtable_t pc_to_id_map;

// Global hash table to keep track of the hit count of BBs.
// Key: unique BB ID, value: hit count.
static hashtable_t hit_count_table;

// Global hash table to save the instruction size of each BB.
// Key: unique BB ID, value: BB #instructions.
static hashtable_t bb_size_table;

// Global unique BB counter used as ID.
static int unique_bb_count = 1;

// Global instruction counter to keep track of when we reach the end of the user-defined
// instruction interval.
static int instr_count = 0;

// List of Basic Block Vectors (BBVs).
// This is a vector of vector pointers. Each element is a vector of pairs
// <BB_ID, hit_count * BB_size> of type bb_id_count_pair_t.
static drvector_t bbvs;

typedef struct _bb_id_count_pair_t {
    int id;    // Derived from unique_bb_count.
    int count; // hit_count * BB_size.
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
    int *hit_count = (int *)entry;
    dr_global_free(hit_count, sizeof(int));
}

static void
add_to_bbv(void *key, void *payload, void *user_data)
{
    int count = *((int *)payload);
    // Skip BBs that were not executed in the current instruction interval.
    if (count == 0)
        return;
    int id = (int)(ptr_int_t)key;

    int bb_size = (int)(ptr_int_t)hashtable_lookup(&bb_size_table, (void *)(ptr_int_t)id);

    bb_id_count_pair_t *id_count_pair =
        (bb_id_count_pair_t *)dr_global_alloc(sizeof(bb_id_count_pair_t));
    id_count_pair->id = id;
    id_count_pair->count = count * bb_size;

    // Add BB frequency to BBV.
    drvector_t *bbv = (drvector_t *)user_data;
    drvector_append(bbv, id_count_pair);
}

// We add hit counters to the table at instrumentation time. We cannot remove them from
// the hit_count_map when we reach the instruction interval at execution time, or the next
// interval won't have a hit counter. So, we set them to zero.
static void
set_count_to_zero(void *payload)
{
    int *count = ((int *)payload);
    *count = 0;
}

static void
save_bbv(int bb_id, int bb_size)
{
    // TODO i#7685: inline the hit_count and instr_count counter increments and the check
    // for when we reach the instruction interval that should then jump to the code inside
    // the if statement.

    // Increase hit count for the BB.
    int *hit_count = (int *)hashtable_lookup(&hit_count_table, (void *)(ptr_int_t)bb_id);
    ++(*hit_count);

    // Incrase instruction count of the interval by the BB #instructions.
    instr_count += bb_size;

    // We reached the end of the instruction interval.
    if (instr_count > (int)instr_interval.get_value()) {
        // Clear global instruction count.
        instr_count = 0;

        // Save current hit_count_table (i.e., the BBV for the current instruction
        //  interval).
        drvector_t *bbv = (drvector_t *)dr_global_alloc(sizeof(drvector_t));
        // We overshoot the initial size of the BBV vector to avoid resizing it.
        drvector_init(bbv, hit_count_table.entries, /*synch=*/false, free_id_count_pair);
        // Iterate over the non-zero elements of hit_count_table and add them to the BBV.
        hashtable_apply_to_all_key_payload_pairs_user_data(&hit_count_table, add_to_bbv,
                                                           bbv);
        // Add the newly formed BBV to the list of BBVs.
        drvector_append(&bbvs, bbv);

        // Clear hit_count_table setting all the hit counts to zero.
        hashtable_apply_to_all_payloads(&hit_count_table, set_count_to_zero);
    }
}

static void
event_exit(void)
{
    // Get the current working directory where drrun is executing. We save the .bb file
    // there.
    char cwd[MAXIMUM_PATH];
    if (!dr_get_current_directory(cwd, sizeof(cwd)))
        FATAL("ERROR: dr_get_current_directory() failed");

    // Create and open the .bb file.
    char buf[MAXIMUM_PATH];
    file_t bbvs_file =
        drx_open_unique_appid_file(cwd, dr_get_process_id(), "proc", "bb",
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
            // Only one BB and related hit count for this BBV.
            if (j == 0 && j == bbv->entries - 1) {
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

            dr_write_file(bbvs_file, msg, (size_t)len);
        }
    }

    dr_close_file(bbvs_file);

    // Free DR memory.
    hashtable_delete(&pc_to_id_map);
    hashtable_delete(&bb_size_table);
    hashtable_delete(&hit_count_table);
    if (!drvector_delete(&bbvs))
        FATAL("ERROR: BBVs drvector not deleted");

    drx_exit();
    drmgr_exit();

    // Avoid accumulation of option values on static-link re-attach.
    droption::droption_parser_t::clear_values();
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    // By default drmgr enables auto-predication, which predicates all instructions
    // with the predicate of the current instruction on ARM. We disable it here
    // because we want to unconditionally execute the following instrumentation.
    drmgr_disable_auto_predication(drcontext, bb);
    if (!drmgr_is_first_instr(drcontext, inst))
        return DR_EMIT_DEFAULT;

    // TODO i#7685: don't rely on absolute PC values. Use drmodtrack library to compute
    // relative offset instead.

    // Get the BB ID.
    app_pc bb_pc = instr_get_app_pc(instrlist_first_app(bb));
    void *bb_id_ptr = hashtable_lookup(&pc_to_id_map, (void *)bb_pc);
    int bb_id = (int)(ptr_int_t)bb_id_ptr;
    if (bb_id_ptr == NULL) {
        bb_id = unique_bb_count;
        hashtable_add(&pc_to_id_map, (void *)bb_pc, (void *)(ptr_int_t)bb_id);
        ++unique_bb_count;
    }

    // TODO i#7685: use hit_count_ptr when we inline the counter increments.
    int *hit_count_ptr =
        (int *)hashtable_lookup(&hit_count_table, (void *)(ptr_int_t)bb_id);
    if (hit_count_ptr == NULL) {
        // If hit count is not mapped to this BB, then add a new count to the table.
        int *hit_count = (int *)dr_global_alloc(sizeof(int));
        *hit_count = 0;
        hashtable_add(&hit_count_table, (void *)(ptr_int_t)bb_id, hit_count);
        hit_count_ptr = hit_count;
    }

    // Get the number of instructions in the BB.
    int *bb_size_ptr = (int *)hashtable_lookup(&bb_size_table, (void *)(ptr_int_t)bb_id);
    int bb_size = (int)(ptr_int_t)bb_size_ptr;
    if (bb_size_ptr == NULL) {
        bb_size = (int)drx_instrlist_app_size(bb);
        hashtable_add(&bb_size_table, (void *)(ptr_int_t)bb_id,
                      (void *)(ptr_int_t)bb_size);
    }

    // Insert call to instrumentation function.
    dr_insert_clean_call(drcontext, bb, inst, (void *)save_bbv, /*save_fpstate=*/false, 2,
                         OPND_CREATE_INT32(bb_id), OPND_CREATE_INT32(bb_size));

    return DR_EMIT_DEFAULT;
}

} // namespace
} // namespace samples
} // namespace dynamorio

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    // Parse command-line options.
    if (!dynamorio::droption::droption_parser_t::parse_argv(
            dynamorio::droption::DROPTION_SCOPE_CLIENT, argc, argv, NULL, NULL))
        DR_ASSERT(false);

    dr_set_client_name("DynamoRIO Basic Block Vector Client (aka DrPoints)",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || !drx_init())
        DR_ASSERT(false);

    // Register events.
    drmgr_register_exit_event(dynamorio::samples::event_exit);
    if (!drmgr_register_bb_instrumentation_event(
            NULL, dynamorio::samples::event_app_instruction, NULL)) {
        DR_ASSERT(false);
    }

    hashtable_init_ex(&dynamorio::samples::bb_size_table, HASH_BITS, HASH_INTPTR,
                      /*str_dup=*/false, /*synch=*/false, NULL, NULL, NULL);
    hashtable_init_ex(&dynamorio::samples::hit_count_table, HASH_BITS, HASH_INTPTR,
                      /*str_dup=*/false, /*synch=*/false,
                      dynamorio::samples::free_hit_count, NULL, NULL);
    hashtable_init_ex(&dynamorio::samples::pc_to_id_map, HASH_BITS, HASH_INTPTR,
                      /*str_dup=*/false, /*synch=*/false, NULL, NULL, NULL);
    drvector_init(&dynamorio::samples::bbvs, 0, /*synch=*/false,
                  dynamorio::samples::free_bbv);

    // Make it easy to tell, by looking at log file, which client executed.
    dr_log(NULL, DR_LOG_ALL, 1, "DrPoints initializing\n");
}
