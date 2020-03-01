/* ******************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
 * ******************************************************/

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

/* This client provides handlers for a set of hypothetical annotations that the app uses
 * to (a) report app activity and (b) control the client by setting various mode states.
 * The client supports three execution modes, selected via command line argument:
 *
 *   - default (fast decoding): no argument
 *   - full decoding: "full-decode"
 *   - truncation: "truncate@#", where "#" is a single digit 1-9 indicating the maximum
 *                 number of app instructions that remain in each bb after truncation.
 */

#include "dr_api.h"
#include "client_tools.h"
#include "dr_ir_opnd.h"
#include "dr_annotation.h"
#include <string.h> /* memset */

#define MAX_MODE_HISTORY 100
#define UNKNOWN_MODE 0xffffffffU

#ifdef X64
#    define MIN_MEM_DEFINES 10000000U
#else
#    define MIN_MEM_DEFINES 1000000U
#endif

/* This macro wraps all messages printed from the client in "< >", e.g. "<message>".
 * This makes it easier to understand the verbose output from this test when something
 * has gone wrong. The macro additionally acquires a lock for thread safety (see i#1647).
 */
#define PRINTF(...)                      \
    do {                                 \
        dr_mutex_lock(write_lock);       \
        dr_fprintf(STDERR, "      <");   \
        dr_fprintf(STDERR, __VA_ARGS__); \
        dr_fprintf(STDERR, ">\n");       \
        dr_mutex_unlock(write_lock);     \
    } while (0)

/* Defines a hypothetical "analysis context", which is associated with an app thread */
typedef struct _context_t {
    uint id;
    char *label;
    uint mode;               /* hypothetical "analysis mode" of the associated thread */
    uint *mode_history;      /* for recording mode changes to evaluate the test */
    uint mode_history_index; /* index into `mode_history` */
    struct _context_t *next;
} context_t;

typedef struct _context_list_t {
    context_t *head;
    context_t *tail;
} context_list_t;

typedef struct _mem_defines_t {
    unsigned long v1;
    unsigned long v2;
    unsigned long v3;
    unsigned long v4;
} mem_defines_t;

static void *context_lock, *write_lock;
static context_list_t *context_list;
#if !(defined(WINDOWS) && defined(X64))
static mem_defines_t *mem_defines;
#endif

static client_id_t client_id;
static uint bb_truncation_length;

static void
test_eight_args_v1(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h);
static void
test_eight_args_v2(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h);
static void
test_nine_args_v1(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i);
static void
test_nine_args_v2(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i);
static void
test_ten_args_v1(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i,
                 uint j);
static void
test_ten_args_v2(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i,
                 uint j);

static inline context_t *
get_context(uint id)
{
    context_t *context = context_list->head;
    for (; context != NULL; context = context->next) {
        if (context->id == id)
            return context;
    }
    return NULL;
}

/* Convenience function to register a call handler */
static void
register_call(const char *annotation, void *target, uint num_args)
{
    dr_annotation_register_call(annotation, target, false, num_args,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
}

/* Annotation handler to initialize a hypothetical "analysis mode" with integer id */
static void
init_mode(uint mode)
{
    PRINTF("Initialize mode %d", mode);
}

/* Annotation handler to initialize a client context (associated with an app thread) */
static void
init_context(uint id, const char *label, uint initial_mode)
{
    context_t *context;
    dr_mutex_lock(context_lock);

    PRINTF("Initialize context %d '%s' in mode %d", id, label, initial_mode);

    context = get_context(id);
    if (context == NULL) {
        uint label_length = (uint)(sizeof(char) * strlen(label)) + 1;
        context = dr_global_alloc(sizeof(context_t));
        context->id = id;
        context->label = dr_global_alloc(label_length);
        context->mode = initial_mode;
        context->mode_history = dr_global_alloc(MAX_MODE_HISTORY * sizeof(uint));
        context->mode_history[0] = initial_mode;
        context->mode_history_index = 1;
        memcpy(context->label, label, label_length);
        context->next = NULL;

        if (context_list->head == NULL) {
            context_list->head = context_list->tail = context;
        } else {
            context_list->tail->next = context;
            context_list->tail = context;
        }
    }

    dr_mutex_unlock(context_lock);
}

/* Annotation accessor for the hypothetical "analysis mode" of the specified context */
static void
get_mode(uint context_id)
{
    context_t *context;

    dr_mutex_lock(context_lock);
    context = get_context(context_id);
    if (context != NULL)
        dr_annotation_set_return_value(context->mode);
    else
        dr_annotation_set_return_value(UNKNOWN_MODE);
    dr_mutex_unlock(context_lock);
}

/* Annotation handler to set a hypothetical "analysis mode" for the specified context */
static void
set_mode(uint context_id, uint new_mode)
{
    uint original_mode = 0xffffffff;
    context_t *context;

    dr_mutex_lock(context_lock);
    context = get_context(context_id);
    if (context != NULL) {
        original_mode = context->mode;
        context->mode = new_mode;
        if (context->mode_history_index < MAX_MODE_HISTORY)
            context->mode_history[context->mode_history_index++] = new_mode;
    }
    dr_mutex_unlock(context_lock);
}

static void
get_pc(void)
{
    app_pc pc = (app_pc)dr_read_saved_reg(dr_get_current_drcontext(), SPILL_SLOT_2);
    module_data_t *exe = dr_get_main_module();
    ASSERT(pc >= exe->start && pc <= exe->end);
    dr_free_module_data(exe);
}

#if !(defined(WINDOWS) && defined(X64))
/* Identical Valgrind annotation handlers for concurrent rotation and invocation */
static ptr_uint_t
handle_make_mem_defined_if_addressable_v1(dr_vg_client_request_t *request)
{
    mem_defines->v1 += request->args[1];
    return 0;
}

static ptr_uint_t
handle_make_mem_defined_if_addressable_v2(dr_vg_client_request_t *request)
{
    mem_defines->v2 += request->args[1];
    return 0;
}

static ptr_uint_t
handle_make_mem_defined_if_addressable_v3(dr_vg_client_request_t *request)
{
    mem_defines->v3 += request->args[1];
    return 0;
}

static ptr_uint_t
handle_make_mem_defined_if_addressable_v4(dr_vg_client_request_t *request)
{
    mem_defines->v4 += request->args[1];
    return 0;
}

/* Annotation handler to rotate among registered Valgrind handlers. Exercises concurrent
 * un/registration and invocation of valgrind annotation handlers.
 */
static void
rotate_valgrind_handler(int phase)
{
    switch (phase) {
    case 0:
        dr_annotation_register_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                        handle_make_mem_defined_if_addressable_v1);
        break;
    case 1:
        dr_annotation_register_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                        handle_make_mem_defined_if_addressable_v2);
        break;
    case 2:
        dr_annotation_unregister_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                          handle_make_mem_defined_if_addressable_v1);
        break;
    case 3:
        dr_annotation_unregister_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                          handle_make_mem_defined_if_addressable_v2);
        break;
    case 4:
        dr_annotation_register_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                        handle_make_mem_defined_if_addressable_v3);
        break;
    case 5:
        dr_annotation_register_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                        handle_make_mem_defined_if_addressable_v4);
        break;
    case 6:
        dr_annotation_unregister_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                          handle_make_mem_defined_if_addressable_v3);
        break;
    case 7:
        dr_annotation_unregister_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                          handle_make_mem_defined_if_addressable_v4);
        break;
    }
}
#endif

/* First handler for an annotation with 8 arguments */
static void
test_eight_args_v1(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h)
{
    PRINTF("Test many args (handler #1): "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d",
           a, b, c, d, e, f, g, h);
}

/* Second handler for an annotation with 8 arguments */
static void
test_eight_args_v2(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h)
{
    PRINTF("Test many args (handler #2): "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d",
           a, b, c, d, e, f, g, h);

    /* Verify that a reloaded module gets instrumented with the current handlers. This
     * registration executes only on the first iteration (`a` is the iteration count),
     * and the modules are unloaded and reloaded within each iteration.
     */
    if (h == 18) {
        if (a == 1)
            register_call("test_annotation_nine_args", (void *)test_nine_args_v2, 9);
        else if (a == 3)
            dr_annotation_unregister_call("test_annotation_nine_args", test_nine_args_v1);
    }
}

/* First handler for an annotation with 9 arguments */
static void
test_nine_args_v1(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i)
{
    /* omit handler number to allow non-deterministic ordering */
    PRINTF("Test many args (concurrent handler): "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d",
           a, b, c, d, e, f, g, h, i);
}

/* Second handler for an annotation with 9 arguments */
static void
test_nine_args_v2(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i)
{
    /* omit handler number to allow non-deterministic ordering */
    PRINTF("Test many args (concurrent handler): "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d",
           a, b, c, d, e, f, g, h, i);
}

/* First handler for an annotation with 10 arguments */
static void
test_ten_args_v1(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i,
                 uint j)
{
    PRINTF("Test many args (handler #1): "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d, j=%d",
           a, b, c, d, e, f, g, h, i, j);
}

/* Second handler for an annotation with 10 arguments */
static void
test_ten_args_v2(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i,
                 uint j)
{
    PRINTF("Test many args (handler #2): "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d, j=%d",
           a, b, c, d, e, f, g, h, i, j);
}

/* Enables full decoding */
dr_emit_flags_t
empty_bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
               bool translating)
{
    return DR_EMIT_DEFAULT;
}

/* Truncates every basic block to the length specified in the CL option (see dr_init) */
dr_emit_flags_t
bb_event_truncate(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating)
{
    uint app_instruction_count = 0;
    instr_t *next, *instr = instrlist_first(bb);

    while (instr != NULL) {
        next = instr_get_next(instr);
        if (!instr_is_meta(instr)) {
            if (app_instruction_count == bb_truncation_length) {
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            } else {
                app_instruction_count++;
            }
        }
        instr = next;
    }

    return DR_EMIT_DEFAULT;
}

/* Report the history of "analysis mode" changes and clean up local allocations */
static void
event_exit(void)
{
    uint i;
    context_t *context, *next;

    for (context = context_list->head; context != NULL; context = next) {
        next = context->next;

        for (i = 1; i < context->mode_history_index; i++) {
            PRINTF("In context %d at event %d, the mode changed from %d to %d",
                   context->id, i, context->mode_history[i - 1],
                   context->mode_history[i]);
        }
        PRINTF("Context '%s' terminates in mode %d", context->label, context->mode);
    }

#if !(defined(WINDOWS) && defined(X64))
    ASSERT(mem_defines->v1 > MIN_MEM_DEFINES);
    ASSERT(mem_defines->v2 > MIN_MEM_DEFINES);
    ASSERT(mem_defines->v3 > MIN_MEM_DEFINES);
    ASSERT(mem_defines->v4 > MIN_MEM_DEFINES);
#endif

    dr_mutex_destroy(context_lock);
    dr_mutex_destroy(write_lock);
    for (context = context_list->head; context != NULL; context = next) {
        next = context->next;

        dr_global_free(context->label, (sizeof(char) * strlen(context->label)) + 1);
        dr_global_free(context->mode_history, MAX_MODE_HISTORY * sizeof(uint));
        dr_global_free(context, sizeof(context_t));
    }
    dr_global_free(context_list, sizeof(context_list_t));
#if !(defined(WINDOWS) && defined(X64))
    dr_global_free(mem_defines, sizeof(mem_defines_t));
#endif
}

/* Parse CL options and register DR event handlers and annotation handlers */
DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    context_lock = dr_mutex_create();
    write_lock = dr_mutex_create();

#ifdef WINDOWS
    dr_enable_console_printing();
#endif

    client_id = id;

    /* XXX: should use droption */
    if (argc > 1 && strcmp(argv[1], "full-decode") == 0) {
        PRINTF("Init annotation test client with full decoding");
        dr_register_bb_event(empty_bb_event);
    } else if (argc > 1 && strlen(argv[1]) >= 8 && strncmp(argv[1], "truncate", 8) == 0) {
        bb_truncation_length = (argv[1][9] - '0'); /* format is "truncate@n" (0<n<10) */
        ASSERT(bb_truncation_length < 10 && bb_truncation_length > 0);
        PRINTF("Init annotation test client with bb truncation");
        dr_register_bb_event(bb_event_truncate);
    } else {
        PRINTF("Init annotation test client with fast decoding");
    }

    context_list = dr_global_alloc(sizeof(context_list_t));
    memset(context_list, 0, sizeof(context_list_t));

#if !(defined(WINDOWS) && defined(X64))
    mem_defines = dr_global_alloc(sizeof(mem_defines_t));
    memset(mem_defines, 0, sizeof(mem_defines_t));
#endif

    dr_register_exit_event(event_exit);

    register_call("test_annotation_init_mode", (void *)init_mode, 1);
    register_call("test_annotation_init_context", (void *)init_context, 3);
    register_call("test_annotation_get_mode", (void *)get_mode, 1);
    register_call("test_annotation_set_mode", (void *)set_mode, 2);
#if !(defined(WINDOWS) && defined(X64))
    register_call("test_annotation_rotate_valgrind_handler",
                  (void *)rotate_valgrind_handler, 1);
#endif

    register_call("test_annotation_get_pc", (void *)get_pc, 0);
    dr_annotation_pass_pc("test_annotation_get_pc");

    register_call("test_annotation_eight_args", (void *)test_eight_args_v1, 8);
    register_call("test_annotation_eight_args", (void *)test_eight_args_v2, 8);
    /* Test removing the last handler */
    dr_annotation_unregister_call("test_annotation_eight_args", test_eight_args_v1);

    register_call("test_annotation_nine_args", (void *)test_nine_args_v1, 9);
    register_call("test_annotation_nine_args", (void *)test_nine_args_v2, 9);
    /* Test removing the first handler */
    dr_annotation_unregister_call("test_annotation_nine_args", test_nine_args_v2);

    /* Test multiple handlers */
    register_call("test_annotation_ten_args", (void *)test_ten_args_v1, 10);
    register_call("test_annotation_ten_args", (void *)test_ten_args_v2, 10);

    dr_annotation_register_return("test_annotation_get_client_version", (void *)"2.2.8");
}
