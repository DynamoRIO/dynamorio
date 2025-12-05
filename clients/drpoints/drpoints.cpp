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
 * frequencies within the interval) of a program execution and outputs them in a .bbv
 * file.
 *
 * The generated .bbv file look like:
 *
 * T:BB_id:count :BB_id:count ... :BB_id:count
 * T:BB_id:count :BB_id:count ... :BB_id:count
 * ...
 *
 * Where "T" is the BBV separator, BB_id is a sequential BB identifier that is an integer
 * and starts from 1, and count = number_of_times_BB_was_executed * instructions_of_BB.
 * This format follows what SimpointToolkit 3.2 expects:
 * https://cseweb.ucsd.edu/~calder/simpoint/releases/SimPoint.3.2.tar.gz
 *
 * TODO i#7685: add proper documentation in a .dox file. Some of the things the doc should
 * touch on: an example of how to run the drpoints tool, explanation of the .bbv output
 * file, difference between compiler definition of BB vs DynamoRIO's definition,
 * limitations about multi-threaded programs.
 */

#include "dr_api.h"
#include "dr_defines.h"
#include "dr_events.h"
#include "drcovlib.h"
#include "drmgr.h"
#include "droption.h"
#include "drreg.h"
#include "drvector.h"
#include "drx.h"
#include "hashtable.h"
#include "../common/utils.h"

#include <cinttypes>
#include <string>

namespace dynamorio {
namespace drpoints {
namespace {

using ::dynamorio::droption::bytesize_t;
using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;
using ::dynamorio::droption::droption_t;

#define FATAL(...)                       \
    do {                                 \
        dr_fprintf(STDERR, __VA_ARGS__); \
        dr_abort();                      \
    } while (0)

#define HASH_BITS_BB_ID 13
#define HASH_BITS_BB_COUNT 13

#define MINSERT instrlist_meta_preinsert

#if defined(X86_64) || defined(AARCH64)
#    define INLINE_COUNTER_UPDATE 1
#else
// TODO i#7685: We don't have the inlining implementation yet for 32-bit architectures.
#endif

static droption_t<bytesize_t> instr_interval(
    DROPTION_SCOPE_CLIENT, "instr_interval", 100000000 /*=100M instructions*/,
    "The instruction interval for which to generate BBVs",
    "Divides the program execution into instruction intervals of the specified size and "
    "generates BBVs as the BB execution frequency within the interval times the number "
    "of instructions in the BB. Default is 100M instructions.");

static droption_t<bool>
    print_to_stdout(DROPTION_SCOPE_CLIENT, "print_to_stdout", false,
                    "Enables printing the Basic Block Vectors to standard output",
                    "Also prints to standard output on top of generating the .bbv file. "
                    "Default is false.");

static droption_t<bool> no_out_bbv_file(
    DROPTION_SCOPE_CLIENT, "no_out_bbv_file", false,
    "Disables the generation of the output .bbv file",
    "Disables the generation of the output .bbv file, but still runs the client. Useful "
    "for unit tests or paired with -print_to_stdout. Default is false.");

static droption_t<std::string>
    out_bbv_file(DROPTION_SCOPE_CLIENT, "out_bbv_file", "",
                 "The path to the output .bbv file",
                 "Specifies a different path to the .bbv file. Default is "
                 "${PWD}/drpoints.BINARY_NAME.PID.UNIQUE_ID.bbv.");

// Global hash table that maps the pair module-index and PC-offset to the module's base
// address (which uniquely identify a BB) to a unique, 1-indexed, increasing ID that comes
// from unique_bb_count.
static hashtable_t bb_id_table;

// Global hash table to keep track of the execution count of BBs.
// Key: unique BB ID, value: execution count times BB instruction size.
static hashtable_t bb_count_table;

// Global unique BB counter used as ID. It must start from 1.
static uint64_t unique_bb_count = 1;

// Global instruction counter to keep track of when we reach the end of the user-defined
// instruction interval. Starts at instr_interval and is decremented until <= 0.
static int64_t instr_count;

// List of Basic Block Vectors (BBVs).
// This is a vector of vector pointers. Each vector pointer represents the BBV for an
// instruction interval. They follow the target program execution order and containt
// <BB_ID, execution_count * BB_size> pairs of type bb_id_count_pair_t.
static drvector_t bbvs;

// Keeps track of the number of threads of the application. We abort when we detect a
// multi-threaded application as it's not supported yet.
// TODO i#7685: add support for multi-threaded applications.
static int thread_count = 0;

struct bb_id_count_pair_t {
    uint64_t id;             // Derived from unique_bb_count.
    uint64_t weighted_count; // execution_count * BB_size.
};

// We use this structure as key for bb_id_table to uniquely identify a BB.
struct modidx_offset_t {
    uint modidx;     // Module index.
    uint64_t offset; // BB_PC - modbase_address.
};

// We use Szudzik elegant pairing as hash function (xref:
// http://szudzik.com/ElegantPairing.pdf). This pairing function uniquely and
// deterministically maps two dimensions (i.e., the pair <modidx,offset> that uniquely
// identifies a BB) into one (i.e., a single scalar, the hash). This way we avoid a nested
// hashtable_t from modidx to offset and then to the BB id.
uint
bb_id_hash(void *val)
{
    modidx_offset_t *key = static_cast<modidx_offset_t *>(val);
    uint modidx = key->modidx;
    uint64_t offset = key->offset;
    // We don't care about wrapping behavior as collisions will be handled by bb_id_cmp()
    // anyway.
    return offset >= modidx ? static_cast<uint>(offset * offset + offset) + modidx
                            : modidx * modidx + static_cast<uint>(offset);
}

bool
bb_id_cmp(void *val1, void *val2)
{
    modidx_offset_t *key1 = static_cast<modidx_offset_t *>(val1);
    modidx_offset_t *key2 = static_cast<modidx_offset_t *>(val2);
    return (key1->modidx == key2->modidx) && (key1->offset == key2->offset);
}

static void
free_id_count_pair(void *entry)
{
    bb_id_count_pair_t *pair = static_cast<bb_id_count_pair_t *>(entry);
    dr_global_free(entry, sizeof(*pair));
}

static void
free_bbv(void *entry)
{
    drvector_t *vector = static_cast<drvector_t *>(entry);
    if (!drvector_delete(vector))
        FATAL("ERROR: BBV drvector not deleted");
    dr_global_free(vector, sizeof(*vector));
}

static void
free_count(void *entry)
{
    uint64_t *count = static_cast<uint64_t *>(entry);
    dr_global_free(count, sizeof(*count));
}

static void
add_to_bbv(void *key, void *payload, void *user_data)
{
    uint64_t count = *(static_cast<uint64_t *>(payload));
    // Skip BBs that were not executed in the current instruction interval.
    if (count == 0)
        return;
    uint64_t id = reinterpret_cast<uint64_t>(key);

    bb_id_count_pair_t *id_count_pair =
        static_cast<bb_id_count_pair_t *>(dr_global_alloc(sizeof(*id_count_pair)));
    id_count_pair->id = id;
    id_count_pair->weighted_count = count;

    // Add BB frequency to BBV.
    drvector_t *bbv = static_cast<drvector_t *>(user_data);
    drvector_append(bbv, id_count_pair);
}

// We add execution counters to the table at instrumentation time. We cannot remove them
// from the bb_count_table when we reach the instruction interval at execution time, or
// the next interval won't have an execution counter. So, we set them to zero.
static void
set_count_to_zero(void *payload)
{
    uint64_t *count = static_cast<uint64_t *>(payload);
    *count = 0;
}

static void
save_bbv()
{
    // Clear global instruction count setting it to instr_interval, since we decrement it.
    // Currently we only support single-threaded applications, so we don't use any locking
    // mechanism to set this global counter.
    instr_count = instr_interval.get_value();
#if defined(INLINE_COUNTER_UPDATE) && defined(AARCH64)
    // The counter inline optimization for AARCH64 uses OP_tbz (test bit and branch if 0),
    // which in this case tests the sign bit and does not branch here (save_bbv()) when
    // instr_count reaches 0, it branches only when instr_count < 0, so we decrement the
    // initial count by 1 here to keep the same "branch when instr_count <= 0" behavior.
    --instr_count;
#endif

    // Save current bb_count_table (i.e., the BBV for the current instruction interval).
    drvector_t *bbv = static_cast<drvector_t *>(dr_global_alloc(sizeof(*bbv)));
    // We overshoot the initial size of the BBV vector to avoid resizing it
    // (bb_count_table.entries also counts BB with count 0).
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
update_counters_and_save_bbv(uint64_t *execution_count, size_t bb_size)
{
    // Increase execution count for the BB.
    (*execution_count) += bb_size;

    // Decrease instruction count of the interval by the BB #instructions.
    instr_count -= bb_size;

    // We reached the end of the instruction interval.
    if (instr_count <= 0)
        save_bbv();
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
    // We can't rely on absolute PC values, as a program can unload a library and then
    // reload it at a different address, hence the PC of that code would be different.
    // Instead we use the drmodtrack library to get the module index (which doesn't
    // change) and compute the relative offset of the BB PC to the base address of its
    // corresponding module. The <modidx,offset> pair uniquely identifies a BB and can be
    // used as a key in bb_id_table to get the 1-indexed BB id.
    app_pc bb_pc = instr_get_app_pc(instrlist_first_app(bb));
    uint modidx;
    app_pc modbase;
    drcovlib_status_t res = drmodtrack_lookup(drcontext, bb_pc, &modidx, &modbase);
    DR_ASSERT(res == DRCOVLIB_SUCCESS);
    uint64_t offset = static_cast<uint64_t>(bb_pc - modbase);
    modidx_offset_t bb_id_key = { modidx, offset };
    void *bb_id_ptr = hashtable_lookup(&bb_id_table, &bb_id_key);
    uint64_t bb_id = reinterpret_cast<uint64_t>(bb_id_ptr);
    if (bb_id_ptr == nullptr) {
        bb_id = unique_bb_count;
        hashtable_add(&bb_id_table, &bb_id_key, reinterpret_cast<void *>(bb_id));
        ++unique_bb_count;
    }

    // Get the BB execution counter.
    uint64_t *bb_count_ptr = static_cast<uint64_t *>(
        hashtable_lookup(&bb_count_table, reinterpret_cast<void *>(bb_id)));
    if (bb_count_ptr == nullptr) {
        // If execution count is not mapped to this BB, then add a new count to the table.
        // We cannot save the initial value of 0 directly in the (void *)payload because
        // NULL == 0 is used for lookup failure. Also, we need the counter address.
        uint64_t *bb_count = static_cast<uint64_t *>(dr_global_alloc(sizeof(*bb_count)));
        *bb_count = 0;
        hashtable_add(&bb_count_table, reinterpret_cast<void *>(bb_id), bb_count);
        bb_count_ptr = bb_count;
    }

    size_t bb_size = drx_instrlist_app_size(bb);

#ifdef INLINE_COUNTER_UPDATE
#    if defined(X86_64) || defined(AARCH64)
    instr_t *skip_call = INSTR_CREATE_label(drcontext);
#        if defined(X86_64)
    // Increment the BB execution count by BB size in #instructions.
    if (!drx_insert_counter_update(drcontext, bb, inst,
                                   // We're using drmgr, so these slots
                                   // here won't be used: drreg's slots will be.
                                   static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1),
                                   bb_count_ptr, static_cast<int>(bb_size),
                                   DRX_COUNTER_64BIT)) {
        DR_ASSERT(false);
    }

    // Decrement the instruction count by BB size in #instructions.
    if (!drx_insert_counter_update(drcontext, bb, inst,
                                   // We're using drmgr, so these slots
                                   // here won't be used: drreg's slots will be.
                                   static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1),
                                   &instr_count, -(static_cast<int>(bb_size)),
                                   DRX_COUNTER_64BIT)) {
        DR_ASSERT(false);
    }

    if (drreg_reserve_aflags(drcontext, bb, inst) != DRREG_SUCCESS)
        DR_ASSERT(false);

    // If the user defined instruction interval is reached, jump to a clean call of the
    // instrumentation function that saves the current BBV, otherwise jump to the rest of
    // the BB and continue.
    MINSERT(bb, inst, INSTR_CREATE_jcc(drcontext, OP_jg, opnd_create_instr(skip_call)));
#        elif defined(AARCH64)
    reg_id_t scratch1, scratch2 = DR_REG_NULL;

    // Increment the BB execution count by BB size in #instructions.
    if (!drx_insert_counter_update(
            drcontext, bb, inst, static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1),
            static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1), bb_count_ptr,
            static_cast<int>(bb_size), DRX_COUNTER_64BIT | DRX_COUNTER_REL_ACQ)) {
        DR_ASSERT(false);
    }

    // Decrement the instruction count by BB size in #instructions.
    if (!drx_insert_counter_update(
            drcontext, bb, inst, static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1),
            static_cast<dr_spill_slot_t>(SPILL_SLOT_MAX + 1), &instr_count,
            -(static_cast<int>(bb_size)), DRX_COUNTER_64BIT | DRX_COUNTER_REL_ACQ)) {
        DR_ASSERT(false);
    }

    // Reserve two scratch registers.
    if (drreg_reserve_register(drcontext, bb, inst, NULL, &scratch1) != DRREG_SUCCESS)
        FATAL("ERROR: failed to reserve scratch register 1");
    if (drreg_reserve_register(drcontext, bb, inst, NULL, &scratch2) != DRREG_SUCCESS)
        FATAL("ERROR: failed to reserve scratch register 2");

    // XXX i#7685: drx_insert_counter_update() above already loads instr_count in a
    // register, so we could avoid the following two instructions, which are redundant.
    // We can achieve this using some extra flag in drx_insert_counter_update() or
    // performing the sub instruction directly here instead of using
    // drx_insert_counter_update().
    // Move the address of instr_count in a scratch register.
    instrlist_insert_mov_immed_ptrsz(drcontext, reinterpret_cast<ptr_int_t>(&instr_count),
                                     opnd_create_reg(scratch1), bb, inst, NULL, NULL);

    // Load the value of instr_count into another scratch register.
    MINSERT(bb, inst,
            XINST_CREATE_load(drcontext, opnd_create_reg(scratch2),
                              OPND_CREATE_MEMPTR(scratch1, 0)));

    // If the top bit is still zero, then we have not reached the instr_interval yet, so
    // skip the save_bbv() call.
    MINSERT(bb, inst,
            INSTR_CREATE_tbz(drcontext, opnd_create_instr(skip_call),
                             opnd_create_reg(scratch2), OPND_CREATE_INTPTR(63)));
#        endif
    // Insert call to the instrumentation function that saves the current BBV.
    dr_insert_clean_call(drcontext, bb, inst, reinterpret_cast<void *>(save_bbv),
                         /*save_fpstate=*/false, 0);
    MINSERT(bb, inst, skip_call);
#        if defined(X86_64)
    if (drreg_unreserve_aflags(drcontext, bb, inst) != DRREG_SUCCESS)
        DR_ASSERT(false);
#        elif defined(AARCH64)
    if (drreg_unreserve_register(drcontext, bb, inst, scratch1) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, bb, inst, scratch2) != DRREG_SUCCESS) {
        DR_ASSERT(false);
    }
#        endif
#    else
#        error NYI
#    endif
#else
    // Default to a clean call to the instrumentation function that increments the
    // counter, checks if the user defined instruction interval is reached, and if so
    // saves the current BBV.
    dr_insert_clean_call(drcontext, bb, inst,
                         reinterpret_cast<void *>(update_counters_and_save_bbv),
                         /*save_fpstate=*/false, 2, OPND_CREATE_INTPTR(bb_count_ptr),
                         OPND_CREATE_INTPTR(bb_size));
#endif
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    bool generate_file = !no_out_bbv_file.get_value();
    file_t bbvs_file = INVALID_FILE;
    if (generate_file) {
        std::string path_to_bbv_file = out_bbv_file.get_value();
        if (!path_to_bbv_file.empty()) {
            bbvs_file = dr_open_file(path_to_bbv_file.c_str(), DR_FILE_WRITE_REQUIRE_NEW);
        } else {
            // Get the current working directory where drrun is executing. We save the
            // .bbv file there.
            char cwd[MAXIMUM_PATH];
            if (!dr_get_current_directory(cwd, sizeof(cwd)))
                FATAL("ERROR: dr_get_current_directory() failed");

            // Create and open the drpoints.PROC_BIN_NAME.PID.UNIQUE_ID.bbv file.
            char buf[MAXIMUM_PATH];
            bbvs_file =
                drx_open_unique_appid_file(cwd, dr_get_process_id(), "drpoints", "bbv",
                                           DR_FILE_WRITE_REQUIRE_NEW, buf, sizeof(buf));
        }

        if (bbvs_file == INVALID_FILE)
            FATAL("ERROR: unable to create BBVs file");
    }

    // Define the format strings to write the file.
    const char *one_pair_only = "T:%" PRIu64 ":%" PRIu64 " \n";
    const char *first_pair = "T:%" PRIu64 ":%" PRIu64 " ";
    const char *middle_pair = ":%" PRIu64 ":%" PRIu64 " ";
    const char *last_pair = ":%" PRIu64 ":%" PRIu64 " \n";

    for (uint i = 0; i < bbvs.entries; ++i) {
        drvector_t *bbv = static_cast<drvector_t *>(drvector_get_entry(&bbvs, i));
        for (uint j = 0; j < bbv->entries; ++j) {
            bb_id_count_pair_t *pair =
                static_cast<bb_id_count_pair_t *>(drvector_get_entry(bbv, j));

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
            int len = dr_snprintf(msg, BUFFER_SIZE_ELEMENTS(msg), format_string, pair->id,
                                  pair->weighted_count);
            NULL_TERMINATE_BUFFER(msg);
            DR_ASSERT(len > 0);

            if (print_to_stdout.get_value())
                dr_fprintf(STDOUT, "%s", msg);

            if (generate_file)
                dr_write_file(bbvs_file, msg, static_cast<size_t>(len));
        }
    }

    if (generate_file)
        dr_close_file(bbvs_file);

    // Free DR memory.
    hashtable_delete(&bb_id_table);
    hashtable_delete(&bb_count_table);
    if (!drvector_delete(&bbvs))
        FATAL("ERROR: BBVs drvector not deleted");

    bool res = drmodtrack_exit();
    DR_ASSERT(res == DRCOVLIB_SUCCESS);

    drmgr_unregister_thread_init_event(event_thread_init);
    drmgr_unregister_exit_event(event_exit);
    drx_exit();
    drreg_exit();
    drmgr_exit();

    // Avoid accumulation of option values on static-link re-attach.
    droption_parser_t::clear_values();
}

} // namespace
} // namespace drpoints
} // namespace dynamorio

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    // Parse command-line options.
    std::string parse_err;
    if (!dynamorio::droption::droption_parser_t::parse_argv(
            dynamorio::droption::DROPTION_SCOPE_CLIENT, argc, argv, &parse_err,
            nullptr)) {
        FATAL("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
              dynamorio::droption::droption_parser_t::usage_short(
                  dynamorio::droption::DROPTION_SCOPE_CLIENT)
                  .c_str());
    }

    dr_set_client_name("DrPoints", "http://dynamorio.org/issues");

    drcovlib_status_t res = drmodtrack_init();
    DR_ASSERT(res == DRCOVLIB_SUCCESS);

    drreg_options_t ops = { sizeof(ops), 1 /*max slots needed: aflags*/, false };
    if (!drmgr_init() || !drx_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    // Register events.
    drmgr_register_exit_event(dynamorio::drpoints::event_exit);
    if (!drmgr_register_bb_instrumentation_event(
            nullptr, dynamorio::drpoints::event_app_instruction, nullptr) ||
        (!drmgr_register_thread_init_event(dynamorio::drpoints::event_thread_init))) {
        DR_ASSERT(false);
    }

    // Currently we only support single-threaded applications, so we don't use any locking
    // mechanism for the following global data structures.
    hashtable_init_ex(&dynamorio::drpoints::bb_count_table, HASH_BITS_BB_COUNT,
                      HASH_INTPTR, /*str_dup=*/false, /*synch=*/false,
                      dynamorio::drpoints::free_count, nullptr, nullptr);
    hashtable_init_ex(&dynamorio::drpoints::bb_id_table, HASH_BITS_BB_ID, HASH_INTPTR,
                      /*str_dup=*/false, /*synch=*/false, nullptr,
                      dynamorio::drpoints::bb_id_hash, dynamorio::drpoints::bb_id_cmp);
    drvector_init(&dynamorio::drpoints::bbvs, 0, /*synch=*/false,
                  dynamorio::drpoints::free_bbv);

    // Make it easy to tell, by looking at log file, which client executed.
    dr_log(nullptr, DR_LOG_ALL, 1, "DrPoints initializing\n");

    // We count backward until 0, so we set the initial instr_count to be instr_interval.
    dynamorio::drpoints::instr_count = dynamorio::drpoints::instr_interval.get_value();
#if defined(INLINE_COUNTER_UPDATE) && defined(AARCH64)
    // The counter inline optimization for AARCH64 uses OP_tbz (test bit and branch if 0),
    // which in this case tests the sign bit and does not branch to save_bbv() when
    // instr_count reaches 0, it branches only when instr_count < 0, so we decrement the
    // initial count by 1 here to keep the same "branch when instr_count <= 0" behavior.
    --dynamorio::drpoints::instr_count;
#endif
}
