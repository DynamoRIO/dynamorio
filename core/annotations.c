/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

#include "../globals.h"
#include "../hashtable.h"
#include "../x86/instr.h"
#include "../x86/instr_create.h"
#include "../x86/decode_fast.h"
#include "utils.h"
#include "annotations.h"

#ifdef ANNOTATIONS /* around whole file */

#if !(defined (WINDOWS) && defined (X64))
# include "../third_party/valgrind/valgrind.h"
# include "../third_party/valgrind/memcheck.h"
#endif

#ifdef UNIX
# include <string.h>
#endif

/* IS_ANNOTATION_LABEL_INSTRUCTION(instr):         evaluates to true for any `instr` that
 *                                                 could be the instruction from which the
 *                                                 label pointer is extracted.
 * IS_ANNOTATION_LABEL_REFERENCE(opnd):            evaluates to true for any `opnd` that
 *                                                 could be the instruction from which the
 *                                                 label pointer is extracted.
 * GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc):  extracts the label pointer
 */
#ifdef WINDOWS
# ifdef X64
#  define IS_ANNOTATION_LABEL_INSTRUCTION(instr) \
        (instr_is_mov(instr) || (instr_get_opcode(instr) == OP_prefetchw))
#  define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_rel_addr(src)
#  define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) opnd_get_addr(src)
# else
#  define IS_ANNOTATION_LABEL_INSTRUCTION(instr) instr_is_mov(instr)
#  define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_base_disp(src)
#  define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) ((app_pc) opnd_get_disp(src))
# endif
#else
# define IS_ANNOTATION_LABEL_INSTRUCTION(instr) instr_is_mov(instr)
# define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_base_disp(src)
# ifdef X64
#  define ANNOTATION_LABEL_REFERENCE_OPERAND_OFFSET 4
# else
#  define ANNOTATION_LABEL_REFERENCE_OPERAND_OFFSET 0
# endif
# define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) \
    ((app_pc) (opnd_get_disp(src) + instr_pc + ANNOTATION_LABEL_REFERENCE_OPERAND_OFFSET))
#endif

/* FASTCALL_REGISTER_ARG_COUNT: Specifies the number of arguments passed in registers. */
#ifdef X64
# ifdef UNIX
#  define FASTCALL_REGISTER_ARG_COUNT 6
# else /* WINDOWS x64 */
#  define FASTCALL_REGISTER_ARG_COUNT 4
# endif
#else /* x86 (all) */
# define FASTCALL_REGISTER_ARG_COUNT 2
#endif

/* Instruction `int 2c` hints that the preceding cbr is probably an annotation head. */
#define WINDOWS_X64_ANNOTATION_HINT_BYTE 0xcd
/* Instruction `int 3` acts as a kind of boundary for compiler optimizations. */
#define WINDOWS_X64_OPTIMIZATION_FENCE 0xcc

#define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO_NAME \
    "dynamorio_annotate_running_on_dynamorio"

#define DYNAMORIO_ANNOTATE_LOG_NAME \
    "dynamorio_annotate_log"

enum {
    VG_ROL_COUNT = 4,
    VG_PATTERN_LENGTH = 5,
};

/* Immediate operands to the special rol instructions.
 * See __SPECIAL_INSTRUCTION_PREAMBLE in valgrind.h.
 */
#ifdef X64
static const int
expected_rol_immeds[VG_PATTERN_LENGTH] = {
    3,
    13,
    61,
    51
};
#else
static const int
expected_rol_immeds[VG_PATTERN_LENGTH] = {
    3,
    13,
    29,
    19
};
#endif

typedef enum _annotation_type_t {
    ANNOTATION_TYPE_NONE,
    ANNOTATION_TYPE_EXPRESSION,
    ANNOTATION_TYPE_STATEMENT,
} annotation_type_t;

typedef struct _annotation_layout_t {
    app_pc start_pc;
    annotation_type_t type;
    bool is_void;
    const char *name;
    app_pc substitution_xl8;
    app_pc resume_pc;
} annotation_layout_t;

static strhash_table_t *handlers;

/* locked under the `handlers` table lock */
static dr_annotation_handler_t *vg_handlers[DR_VG_ID__LAST];

#if !(defined (WINDOWS) && defined (X64))
static dr_annotation_handler_t vg_router;
static dr_annotation_receiver_t vg_receiver;
static opnd_t vg_router_arg;
#endif

extern ssize_t do_file_write(file_t f, const char *fmt, va_list ap);

/*********************************************************
 * INTERNAL ROUTINE DECLARATIONS
 */

#if !(defined (WINDOWS) && defined (X64))
static void
handle_vg_annotation(app_pc request_args);

static dr_valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request);

static ptr_uint_t
valgrind_running_on_valgrind(dr_vg_client_request_t *request);
#endif

static bool
is_annotation_tag(dcontext_t *dcontext, IN OUT app_pc *start_pc, instr_t *scratch,
                  OUT const char **name);

static void
identify_annotation(dcontext_t *dcontext, IN OUT annotation_layout_t *layout,
                    instr_t *scratch);

/* Create argument operands for the instrumented clean call */
static void
create_arg_opnds(dr_annotation_handler_t *handler, uint num_args,
                 dr_annotation_calling_convention_t call_type);

#ifdef DEBUG
static ssize_t
annotation_printf(const char *format, ...);
#endif

/* Invoked during hashtable entry removal */
static void
free_annotation_handler(void *p);

/*********************************************************
 * ANNOTATION INTEGRATION FUNCTIONS
 */

void
annotation_init()
{
    handlers = strhash_hash_create(GLOBAL_DCONTEXT, 8, 80, /* favor a small table */
                                   HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                   HASHTABLE_RELAX_CLUSTER_CHECKS | HASHTABLE_PERSISTENT,
                                   free_annotation_handler
                                   _IF_DEBUG("annotation handler hashtable"));

#if !(defined (WINDOWS) && defined (X64))
    vg_router.type = DR_ANNOTATION_HANDLER_CALL;
    vg_router.num_args = 1;
    vg_router_arg = opnd_create_reg(DR_REG_XAX);
    vg_router.args = &vg_router_arg;
    vg_router.symbol_name = NULL;
    vg_router.receiver_list = &vg_receiver;
    vg_receiver.instrumentation.vg_callback = (void *) handle_vg_annotation;
    vg_receiver.save_fpstate = false;
    vg_receiver.next = NULL;
#endif

    dr_annotation_register_return(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO_NAME,
                             (void *) (ptr_uint_t) true);
#ifdef DEBUG
    dr_annotation_register_call(DYNAMORIO_ANNOTATE_LOG_NAME, (void *) annotation_printf,
                                false, 20, DR_ANNOTATION_CALL_TYPE_FASTCALL);
#endif

    dr_annotation_register_valgrind(DR_VG_ID__RUNNING_ON_VALGRIND,
                                    valgrind_running_on_valgrind);
}

void
annotation_exit()
{
    uint i;
    for (i = 0; i < DR_VG_ID__LAST; i++) {
        if (vg_handlers[i] != NULL)
            free_annotation_handler(vg_handlers[i]);
    }

    strhash_hash_destroy(GLOBAL_DCONTEXT, handlers);
}

bool
instrument_annotation(dcontext_t *dcontext, app_pc *start_pc, instr_t **substitution
                 _IF_WINDOWS_X64(bool hint_is_safe))
{
    annotation_layout_t layout;
    /* This instr_t is used for analytical decoding throughout the detection functions.
     * It is passed on the stack and its contents are considered void on function entry.
     */
    instr_t scratch;
#if defined (WINDOWS) && defined (X64)
    app_pc hint_pc = *start_pc;
    bool hint = true;
    byte hint_byte;
#endif

    memset(&layout, 0, sizeof(annotation_layout_t));

#if defined (WINDOWS) && defined (X64)
    if (hint_is_safe) {
        hint_byte = *hint_pc;
    } else {
        if (!safe_read(hint_pc, 1, &hint_byte))
            return NULL;
    }
    if (hint_byte != WINDOWS_X64_ANNOTATION_HINT_BYTE)
        return NULL;
    layout.start_pc = hint_pc + 2;
    layout.substitution_xl8 = layout.start_pc;
#else
    layout.start_pc = *start_pc;
#endif

    // dr_printf("Look for annotation at "PFX"\n", *start_pc);

    instr_init(dcontext, &scratch);
    TRY_EXCEPT(dcontext, {
        identify_annotation(dcontext, &layout, &scratch);
    }, { /* EXCEPT */
        LOG(THREAD, LOG_ANNOTATIONS, 2, "Failed to instrument annotation at "PFX"\n",
            *start_pc);
    });
    if (layout.type != ANNOTATION_TYPE_NONE) {
        /*
        dr_printf("Decoded %s of %s\n",
                  (layout.type == ANNOTATION_TYPE_EXPRESSION) ? "expression" : "statement",
                  layout.name);
        dr_printf("\tSkip "PFX"-"PFX" and start decoding the annotation\n",
                  *start_pc - 2, layout.resume_pc);
        */

        *start_pc = layout.resume_pc;

        if (layout.type == ANNOTATION_TYPE_EXPRESSION) {
            dr_annotation_handler_t *handler;

            TABLE_RWLOCK(handlers, write, lock);
            handler = strhash_hash_lookup(GLOBAL_DCONTEXT, handlers, layout.name);
            if (handler != NULL) {
                if (handler->type == DR_ANNOTATION_HANDLER_CALL) {
                    instr_t *call = INSTR_CREATE_label(dcontext);
                    dr_instr_label_data_t *label_data = instr_get_label_data_area(call);
                    handler->is_void = layout.is_void;

                    instr_set_note(call, (void *) DR_NOTE_ANNOTATION);
                    label_data->data[0] = (ptr_uint_t) handler;
                    SET_ANNOTATION_APP_PC(label_data, layout.resume_pc);
                    instr_set_ok_to_mangle(call, false);

                    *substitution = call;

                    if (!handler->is_void) {
                        instr_t *return_placeholder =
                            INSTR_XL8(INSTR_CREATE_mov_st(dcontext,
                                                          opnd_create_reg(REG_XAX),
                                                          OPND_CREATE_INT32(0)),
                                      layout.substitution_xl8);
                        instr_set_note(return_placeholder, (void *) DR_NOTE_ANNOTATION);
                        instr_set_next(*substitution, return_placeholder);
                        instr_set_prev(return_placeholder, *substitution);
                    }
                } else {
                    void *return_value =
                        handler->receiver_list->instrumentation.return_value;
                    *substitution =
                        INSTR_XL8(INSTR_CREATE_mov_st(dcontext,
                                                      opnd_create_reg(REG_XAX),
                                                      OPND_CREATE_INT32(return_value)),
                                  layout.substitution_xl8);

                    // instr_set_ok_to_mangle(*substitution, false);
                }
            }
            TABLE_RWLOCK(handlers, write, unlock);
        }
    }

    instr_free(dcontext, &scratch);
    return (layout.type != ANNOTATION_TYPE_NONE);
}

#if !(defined (WINDOWS) && defined (X64))
void
instrument_valgrind_annotation(dcontext_t *dcontext, instrlist_t *bb, instr_t *instr,
                               app_pc xchg_pc, uint bb_instr_count)
{
    int i;
    instr_t *return_placeholder;
    instr_t *instr_walk;
    dr_instr_label_data_t *label_data;

    DOLOG(4, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "Matched valgrind client request pattern at "PFX":\n",
            instr_get_app_pc(instr));
        LOG(THREAD, LOG_INTERP, 4, "\n");
    });

    /* We leave the argument gathering code (typically "lea _zzq_args -> %xax"
     * and "mov _zzq_default -> %xdx") as app instructions, as it writes to app
     * registers (xref i#1423).
     */
    instr_destroy(dcontext, instr);

    /* Delete rol instructions--unless a previous BB contains some of them, in which
     * case they must be executed to avoid messing up %xdi.
     */
    if (bb_instr_count > VG_ROL_COUNT) {
        instr = instrlist_last(bb);
        for (i = 0; (i < VG_ROL_COUNT) && (instr != NULL); i++) {
            instr_walk = instr_get_prev(instr);
            instrlist_remove(bb, instr);
            instr_destroy(dcontext, instr);
            instr = instr_walk;
        }
    }

    // TODO: if nobody is registered, return true here

    instr = INSTR_CREATE_label(dcontext);
    instr_set_note(instr, (void *) DR_NOTE_ANNOTATION);
    label_data = instr_get_label_data_area(instr);
    label_data->data[0] = (ptr_uint_t) &vg_router;
    SET_ANNOTATION_APP_PC(label_data, xchg_pc);
    instr_set_ok_to_mangle(instr, false);
    instrlist_append(bb, instr);

    /* Append a write to %xdx, both to ensure it's marked defined by DrMem
     * and to avoid confusion with register analysis code (%xdx is written
     * by the clean callee).
     */
    return_placeholder = INSTR_XL8(INSTR_CREATE_mov_st(dcontext, opnd_create_reg(REG_XDX),
                                                       OPND_CREATE_INT32(0)),
                                   xchg_pc);
    instr_set_note(return_placeholder, (void *) DR_NOTE_ANNOTATION);
    //instr_set_ok_to_mangle(return_placeholder, false);
    instrlist_append(bb, return_placeholder);
}
#endif

/*********************************************************
 * ANNOTATION API FUNCTIONS
 */

bool
dr_annotation_register_call(const char *annotation_name, void *callee, bool save_fpstate,
                            uint num_args, dr_annotation_calling_convention_t call_type)
{
    dr_annotation_handler_t *handler;
    dr_annotation_receiver_t *receiver;

    TABLE_RWLOCK(handlers, write, lock);

    handler = (dr_annotation_handler_t *) strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                              annotation_name);
    if (handler == NULL) { /* make a new handler if never registered yet */
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(dr_annotation_handler_t));
        handler->type = DR_ANNOTATION_HANDLER_CALL;
        handler->symbol_name = dr_strdup(annotation_name, ACCT_OTHER);
        handler->num_args = num_args;

        if (num_args == 0) {
            handler->args = NULL;
        } else {
            handler->args = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, opnd_t, num_args,
                                             ACCT_OTHER, UNPROTECTED);
            create_arg_opnds(handler, num_args, call_type);
        }

        strhash_hash_add(GLOBAL_DCONTEXT, handlers, handler->symbol_name, handler);
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
    receiver->instrumentation.callback = callee;
    receiver->save_fpstate = save_fpstate;
    receiver->next = handler->receiver_list; /* push the new receiver onto the list */
    handler->receiver_list = receiver;

    TABLE_RWLOCK(handlers, write, unlock);
    return true;
}

bool
dr_annotation_register_valgrind(dr_valgrind_request_id_t request_id,
                                ptr_uint_t (*annotation_callback)
                                (dr_vg_client_request_t *request))
{
    dr_annotation_handler_t *handler;
    dr_annotation_receiver_t *receiver;
    if (request_id >= DR_VG_ID__LAST)
        return false;

    TABLE_RWLOCK(handlers, write, lock);
    handler = vg_handlers[request_id];
    if (handler == NULL) { /* make a new handler if never registered yet */
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(dr_annotation_handler_t));
        handler->type = DR_ANNOTATION_HANDLER_VALGRIND;

        vg_handlers[request_id] = handler;
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
    receiver->instrumentation.vg_callback = annotation_callback;
    receiver->save_fpstate = false;
    receiver->next = handler->receiver_list; /* push the new receiver onto the list */
    handler->receiver_list = receiver;

    TABLE_RWLOCK(handlers, write, unlock);
    return true;
}

bool
dr_annotation_register_return(const char *annotation_name, void *return_value)
{
    dr_annotation_handler_t *handler;
    dr_annotation_receiver_t *receiver;

    TABLE_RWLOCK(handlers, write, lock);
    ASSERT_TABLE_SYNCHRONIZED(handlers, WRITE);
    handler = (dr_annotation_handler_t *) strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                              annotation_name);
    if (handler == NULL) { /* make a new handler if never registered yet */
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(dr_annotation_handler_t));
        handler->type = DR_ANNOTATION_HANDLER_RETURN_VALUE;
        handler->symbol_name = dr_strdup(annotation_name, ACCT_OTHER);
        strhash_hash_add(GLOBAL_DCONTEXT, handlers, handler->symbol_name, handler);
    } else {
        ASSERT(handler->receiver_list == NULL);
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
    receiver->instrumentation.return_value = return_value;
    receiver->save_fpstate = false;
    receiver->next = NULL; /* return value can only have one implementation at a time */
    handler->receiver_list = receiver;
    TABLE_RWLOCK(handlers, write, unlock);
    return true;
}

bool
dr_annotation_unregister_call(const char *annotation_name, void *callee)
{
    bool found = false;
    dr_annotation_handler_t *handler;

    TABLE_RWLOCK(handlers, write, lock);
    handler = (dr_annotation_handler_t *) strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                              annotation_name);
    if (handler != NULL && handler->receiver_list != NULL) {
        dr_annotation_receiver_t *receiver = handler->receiver_list;
        if (receiver->instrumentation.callback == callee) { /* case 1: remove the head */
            handler->receiver_list = receiver->next;
            HEAP_TYPE_FREE(GLOBAL_DCONTEXT, receiver, dr_annotation_receiver_t,
                           ACCT_OTHER, UNPROTECTED);
            found = true;
        } else { /* case 2: remove from within the list */
            while (receiver->next != NULL) {
                if (receiver->next->instrumentation.callback == callee) {
                    dr_annotation_receiver_t *removal = receiver->next;
                    receiver->next = removal->next;
                    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, removal, dr_annotation_receiver_t,
                                   ACCT_OTHER, UNPROTECTED);
                    found = true;
                    break;
                }
                receiver = receiver->next;
            }
        } /* Leave the handler for the next registration (free it on exit) */
    }
    TABLE_RWLOCK(handlers, write, unlock);
    return found;
}

bool
dr_annotation_unregister_return(const char *annotation_name)
{
    bool found = false;
    dr_annotation_handler_t *handler;

    TABLE_RWLOCK(handlers, write, lock);
    handler = (dr_annotation_handler_t *) strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           annotation_name);
    if ((handler != NULL) && (handler->receiver_list != NULL)) {
        ASSERT(handler->receiver_list->next != NULL);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, handler->receiver_list, dr_annotation_receiver_t,
                       ACCT_OTHER, UNPROTECTED);
        handler->receiver_list = NULL;
        found = true;
    } /* Leave the handler for the next registration (free it on exit) */
    TABLE_RWLOCK(handlers, write, unlock);
    return found;
}

bool
dr_annotation_unregister_valgrind(dr_valgrind_request_id_t request_id,
                                  ptr_uint_t (*annotation_callback)
                                  (dr_vg_client_request_t *request))
{
    bool found = false;
    dr_annotation_handler_t *handler;
    TABLE_RWLOCK(handlers, write, lock);
    handler = vg_handlers[request_id];
    if ((handler != NULL) && (handler->receiver_list != NULL)) {
        dr_annotation_receiver_t *receiver = handler->receiver_list;
        if (receiver->instrumentation.vg_callback == annotation_callback) {
            handler->receiver_list = receiver->next; /* case 1: remove the head */
            HEAP_TYPE_FREE(GLOBAL_DCONTEXT, receiver, dr_annotation_receiver_t,
                           ACCT_OTHER, UNPROTECTED);
            found = true;
        } else { /* case 2: remove from within the list */
            while (receiver->next != NULL) {
                if (receiver->next->instrumentation.vg_callback == annotation_callback) {
                    dr_annotation_receiver_t *removal = receiver->next;
                    receiver->next = removal->next;
                    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, removal, dr_annotation_receiver_t,
                                   ACCT_OTHER, UNPROTECTED);
                    found = true;
                    break;
                }
                receiver = receiver->next;
            }
        } /* Leave the handler for the next registration (free it on exit) */
    }
    TABLE_RWLOCK(handlers, write, unlock);
    return found;
}

/*********************************************************
 * ANNOTATION IMPLEMENTATIONS
 */

#if !(defined (WINDOWS) && defined (X64))
static void
handle_vg_annotation(app_pc request_args)
{
    dcontext_t *dcontext;
    dr_valgrind_request_id_t request_id;
    dr_annotation_receiver_t *receiver;
    dr_vg_client_request_t request;
    ptr_uint_t result;

    if (!safe_read(request_args, sizeof(dr_vg_client_request_t), &request))
        return;

    result = request.default_result;

    request_id = lookup_valgrind_request(request.request);

    if (request_id < DR_VG_ID__LAST) {
        TABLE_RWLOCK(handlers, read, lock);
        receiver = vg_handlers[request_id]->receiver_list;
        while (receiver != NULL) { // last receiver wins the result value
            result = receiver->instrumentation.vg_callback(&request);
            receiver = receiver->next;
        }
        TABLE_RWLOCK(handlers, read, unlock);
    }

    /* The result code goes in xdx. */
    dcontext = get_thread_private_dcontext();
# ifdef CLIENT_INTERFACE
    if (dcontext->client_data->mcontext_in_dcontext) {
        get_mcontext(dcontext)->xdx = result;
    } else
# endif
    {
        char *state = (char *)dcontext->dstack - sizeof(priv_mcontext_t);
        ((priv_mcontext_t *)state)->xdx = result;
    }
}

static dr_valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request)
{
    switch (request) {
        case VG_USERREQ__RUNNING_ON_VALGRIND:
            return DR_VG_ID__RUNNING_ON_VALGRIND;
        case VG_USERREQ__MAKE_MEM_DEFINED_IF_ADDRESSABLE:
            return DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE;
        case VG_USERREQ__DISCARD_TRANSLATIONS:
            return DR_VG_ID__DISCARD_TRANSLATIONS;
    }
    return DR_VG_ID__LAST;
}

static ptr_uint_t
valgrind_running_on_valgrind(dr_vg_client_request_t *request)
{
    return 1;
}
#endif

/*********************************************************
 * INTERNAL ROUTINES
 */

static inline bool
is_annotation_tag(dcontext_t *dcontext, IN OUT app_pc *cur_pc, instr_t *scratch,
                  OUT const char **name)
{
    app_pc start_pc = *cur_pc;

    instr_reset(dcontext, scratch);
    *cur_pc = decode(dcontext, *cur_pc, scratch);
    if (IS_ANNOTATION_LABEL_INSTRUCTION(scratch)) {
        opnd_t src = instr_get_src(scratch, 0);
        if (IS_ANNOTATION_LABEL_REFERENCE(src)) {
            char buf[24];
            app_pc buf_ptr;
            app_pc src_ptr = GET_ANNOTATION_LABEL_REFERENCE(src, start_pc);
#ifdef UNIX
            app_pc got_ptr;
            instr_reset(dcontext, scratch);
            *cur_pc = decode(dcontext, *cur_pc, scratch);
            if ((instr_get_opcode(scratch) != OP_bsf) &&
                (instr_get_opcode(scratch) != OP_bsr))
                return false;
            src = instr_get_src(scratch, 0);
            if (!opnd_is_base_disp(src))
                return false;
            src_ptr += opnd_get_disp(src);
            if (!safe_read(src_ptr, sizeof(app_pc), &got_ptr))
                return false;
            src_ptr = got_ptr;
#endif
#if defined (WINDOWS) && defined (X64)
            if (instr_get_opcode(scratch) == OP_prefetchw) {
                if (!safe_read(src_ptr, 20, buf))
                    return false;
                buf[20] = '\0';
                if (strcmp(buf, "dynamorio-annotation") == 0) {
                    *name = (const char *) (src_ptr + 21);
                    return true;
                }
            }
#endif
            if (!safe_read(src_ptr, sizeof(app_pc), &buf_ptr))
                return false;
            if (!safe_read(buf_ptr, 20, buf))
                return false;
            buf[20] = '\0';
            if (strcmp(buf, "dynamorio-annotation") != 0)
                return false;
            *name = (const char *) (buf_ptr + 21);
            return true;
        }
    }
    return false;
}

#if defined (WINDOWS) && defined (X64)
static inline void
identify_annotation(dcontext_t *dcontext, IN OUT annotation_layout_t *layout,
                    instr_t *scratch)
{
    app_pc cur_pc = layout->start_pc, last_call = NULL;

    if (!is_annotation_tag(dcontext, &cur_pc, scratch, &layout->name))
        return;

    while (instr_get_opcode(scratch) != OP_prefetchw) {
        if (instr_is_ubr(scratch))
            cur_pc = instr_get_branch_target_pc(scratch);
        instr_reset(dcontext, scratch);
        cur_pc = decode(dcontext, cur_pc, scratch);
    }

    ASSERT(*cur_pc == WINDOWS_X64_OPTIMIZATION_FENCE);
    cur_pc++;
    layout->resume_pc = cur_pc;

    if (strncmp((const char *) layout->name, "statement:", 10) == 0) {
        layout->type = ANNOTATION_TYPE_STATEMENT;
        layout->name += 10;
        layout->is_void = (strncmp((const char *) layout->name, "void:", 5) == 0);
        layout->name = strchr(layout->name, ':') + 1;
        /* If the target app contains an annotation whose argument is a function call that
         * gets inlined, and that function contains the same annotation, the compiler will
         * fuse the headers. See https://code.google.com/p/dynamorio/wiki/Annotations for
         * a complete example. This loop identifies fused headers and skips them.
         */
        while (true) {
            instr_reset(dcontext, scratch);
            cur_pc = decode(dcontext, cur_pc, scratch);
            if (instr_is_cbr(scratch) && (*(ushort *) cur_pc == 0x2ccd)) {
                cur_pc += 2;
                while (instr_get_opcode(scratch) != OP_prefetchw) {
                    if (instr_is_ubr(scratch))
                        cur_pc = instr_get_branch_target_pc(scratch);
                    instr_reset(dcontext, scratch);
                    cur_pc = decode(dcontext, cur_pc, scratch);
                }

                ASSERT(*cur_pc == WINDOWS_X64_OPTIMIZATION_FENCE);
                cur_pc++;
                layout->resume_pc = cur_pc;
            } else if (instr_is_cti(scratch))
                break;
        }
    } else {
        layout->type = ANNOTATION_TYPE_EXPRESSION;
        layout->name += 11;
        layout->is_void = (strncmp((const char *) layout->name, "void:", 5) == 0);
        layout->name = strchr(layout->name, ':') + 1;
    }
}
#else
static inline void
identify_annotation(dcontext_t *dcontext, IN OUT annotation_layout_t *layout,
                    instr_t *scratch)
{
    app_pc cur_pc = layout->start_pc;
    if (is_annotation_tag(dcontext, &cur_pc, scratch, &layout->name)) {
# ifdef WINDOWS
        if (*(cur_pc++) == 0x58) { // skip padding byte (`pop eax` indicates statement)
# else
        if (instr_get_opcode(scratch) == OP_bsf) {
# endif
            layout->type = ANNOTATION_TYPE_STATEMENT;
        } else {
            layout->type = ANNOTATION_TYPE_EXPRESSION;
        }
        instr_reset(dcontext, scratch);
        layout->substitution_xl8 = cur_pc;
        cur_pc = decode_cti(dcontext, cur_pc, scratch); // assert is `jmp`
        layout->resume_pc = cur_pc;
        layout->is_void = (strncmp((const char *) layout->name, "void:", 5) == 0);
        layout->name = strchr(layout->name, ':') + 1;
    }
}
#endif

#ifdef X64
# ifdef UNIX
static inline void /* UNIX x64 */
create_arg_opnds(dr_annotation_handler_t *handler, uint num_args,
                 dr_annotation_calling_convention_t call_type)
{
    uint i, arg_stack_location;
    ASSERT(call_type == DR_ANNOTATION_CALL_TYPE_FASTCALL); /* architecture constraint */
    switch (num_args) { /* Create up to six register args */
    default:
    case 6:
        handler->args[5] = opnd_create_reg(DR_REG_R9);
    case 5:
        handler->args[4] = opnd_create_reg(DR_REG_R8);
    case 4:
        handler->args[3] = opnd_create_reg(DR_REG_XCX);
    case 3:
        handler->args[2] = opnd_create_reg(DR_REG_XDX);
    case 2:
        handler->args[1] = opnd_create_reg(DR_REG_XSI);
    case 1:
        handler->args[0] = opnd_create_reg(DR_REG_XDI);
    }
    /* Create the remaining args on the stack */
    for (i = FASTCALL_REGISTER_ARG_COUNT; i < num_args; i++) {
        /* The clean call will appear at the top of the annotation function body, where
         * the stack arguments follow the return address and the caller's saved stack
         * pointer. Rewind `i` to 0 and add 2 to skip over these pointers.
         */
        arg_stack_location = sizeof(ptr_uint_t) * (i-FASTCALL_REGISTER_ARG_COUNT+2);
        /* Use the stack pointer because the base pointer is a general register in x64 */
        handler->args[i] = OPND_CREATE_MEMPTR(DR_REG_XSP, arg_stack_location);
    }
}
# else /* WINDOWS x64 */
static inline void
create_arg_opnds(dr_annotation_handler_t *handler, uint num_args,
                 dr_annotation_calling_convention_t call_type)
{
    uint i, arg_stack_location;
    ASSERT(call_type == DR_ANNOTATION_CALL_TYPE_FASTCALL); /* architecture constraint */
    switch (num_args) { /* Create up to four register args */
    default:
    case 4:
        handler->args[3] = opnd_create_reg(DR_REG_R9);
    case 3:
        handler->args[2] = opnd_create_reg(DR_REG_R8);
    case 2:
        handler->args[1] = opnd_create_reg(DR_REG_XDX);
    case 1:
        handler->args[0] = opnd_create_reg(DR_REG_XCX);
    }
    /* Create the remaining args on the stack */
    for (i = FASTCALL_REGISTER_ARG_COUNT; i < num_args; i++) {
        /* The clean call will appear at the top of the annotation function body, where
         * the stack arguments follow the return address and 32 bytes of empty space.
         * Since `i` is already starting at 4, just add one more to reach the args.
         */
        arg_stack_location = sizeof(ptr_uint_t) * (i+1);
        /* Use the stack pointer because the base pointer is a general register in x64 */
        handler->args[i] = OPND_CREATE_MEMPTR(DR_REG_XSP, arg_stack_location);
    }
}
# endif
#else /* x86 (all) */
static inline void
create_arg_opnds(dr_annotation_handler_t *handler, uint num_args,
                 dr_annotation_calling_convention_t call_type)
{
    uint i, arg_stack_location;
    if (call_type == DR_ANNOTATION_CALL_TYPE_FASTCALL) {
        switch (num_args) { /* Create 1 or 2 register args */
        default:
        case 2:
            handler->args[1] = opnd_create_reg(DR_REG_XDX);
        case 1:
            handler->args[0] = opnd_create_reg(DR_REG_XCX);
        }
        /* Create the remaining args on the stack */
        for (i = FASTCALL_REGISTER_ARG_COUNT; i < num_args; i++) {
            /* The clean call will appear at the top of the annotation function body,
             * where the stack args follow the return address and the caller's saved base
             * pointer. Since `i` already starts at 2, use it to skip those pointers.
             */
            arg_stack_location = sizeof(ptr_uint_t) * i;
            handler->args[i] = OPND_CREATE_MEMPTR(DR_REG_XBP, arg_stack_location);
        }
    } else { /* DR_ANNOTATION_CALL_TYPE_STDCALL: Create all args on the stack */
        for (i = 0; i < num_args; i++) {
            /* The clean call will appear at the top of the annotation function body,
             * where the stack args follow the return address and the caller's saved base
             * pointer. Since `i` already starts at 2, use it to skip those pointers.
             */
            arg_stack_location = sizeof(ptr_uint_t) * i;
            handler->args[i] = OPND_CREATE_MEMPTR(DR_REG_XBP, arg_stack_location);
        }
    }
}
#endif

#ifdef DEBUG
static ssize_t
annotation_printf(const char *format, ...)
{
    va_list ap;
    ssize_t count;
    const char *timestamp_token_start;
    uint format_length = 0;

    // TODO: sensitive to `#if defined(DEBUG) && !defined(STANDALONE_DECODER)`?
    if (stats == NULL || stats->loglevel == 0 || (stats->logmask & LOG_ANNOTATIONS) == 0)
        return 0;

    timestamp_token_start = strstr(format, "${timestamp}");
    if (timestamp_token_start != NULL) {
        uint min, sec, msec, timestamp = query_time_seconds();
        format_length = (uint) (strlen(format) + 32);
        if (timestamp > 0) {
            uint length_before_token =
                (uint)((ptr_uint_t) timestamp_token_start - (ptr_uint_t) format);
            char timestamp_buffer[32];
            char *timestamped = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, char, format_length,
                                                 ACCT_OTHER, UNPROTECTED);

            our_snprintf(timestamped, length_before_token, "%s", format);
            sec = (uint) (timestamp / 1000);
            msec = (uint) (timestamp % 1000);
            min = sec / 60;
            sec = sec % 60;
            our_snprintf(timestamp_buffer, 32, "(%ld:%02ld.%03ld)", min, sec, msec);

            our_snprintf(timestamped + length_before_token,
                           (format_length - length_before_token), "%s%s",
                           timestamp_buffer, timestamp_token_start + strlen("${timestamp}"));
            format = (const char *) timestamped;
        }
    }

    va_start(ap, format);
    count = do_file_write(GLOBAL, format, ap);
    va_end(ap);

    if (timestamp_token_start != NULL) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, format, char, format_length,
                        ACCT_OTHER, UNPROTECTED);
    }

    return count;
}
#endif

static void
free_annotation_handler(void *p)
{
    dr_annotation_handler_t *handler = (dr_annotation_handler_t *) p;
    dr_annotation_receiver_t *next, *receiver = handler->receiver_list;
    while (receiver != NULL) {
        next = receiver->next;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, receiver, dr_annotation_receiver_t,
                       ACCT_OTHER, UNPROTECTED);
        receiver = next;
    }
    if (handler->num_args > 0) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, handler->args, opnd_t, handler->num_args,
                        ACCT_OTHER, UNPROTECTED);
    }
    if ((handler->type == DR_ANNOTATION_HANDLER_CALL) ||
        (handler->type == DR_ANNOTATION_HANDLER_RETURN_VALUE))
        dr_strfree(handler->symbol_name, ACCT_OTHER);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, p, dr_annotation_handler_t, ACCT_OTHER, UNPROTECTED);
}

#endif /* ANNOTATIONS */
