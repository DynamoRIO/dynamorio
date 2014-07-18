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
#include "../os_shared.h"
#include "../module_shared.h"
#include "../hashtable.h"
#include "../x86/instr.h"
#include "../x86/instr_create.h"
#include "../x86/disassemble.h"
#include "../x86/decode_fast.h"
#include "../lib/instrument.h"
#include "fragment.h"
#include "vmareas.h"
#include "annotations.h"
#include "utils.h"

#if !(defined (WINDOWS) && defined (X64))
# include "../third_party/valgrind/valgrind.h"
# include "../third_party/valgrind/memcheck.h"
#endif

#define MAX_ANNOTATION_INSTR_COUNT 100

#ifdef UNIX
# include <string.h>
#endif

#ifdef WINDOWS
# ifdef X64
#  define IS_ANNOTATION_LABEL_INSTRUCTION(instr) \
        (instr_is_mov(instr) || (instr_get_opcode(instr) == OP_prefetchw))
#  define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_rel_addr(src)
#  define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) opnd_get_addr(src)
# else
#  define IS_ANNOTATION_LABEL_INSTRUCTION(instr) instr_is_mov(instr)
#  define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_base_disp(src)
#  define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) \
    ((app_pc) opnd_get_disp(src))
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

#define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO_NAME \
    "dynamorio_annotate_running_on_dynamorio"

#define DYNAMORIO_ANNOTATE_LOG_NAME \
    "dynamorio_annotate_log"

#define DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_manage_code_area"

#define DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_unmanage_code_area"

#define DYNAMORIO_ANNOTATE_FLUSH_FRAGMENTS_NAME \
    "dynamorio_annotate_flush_fragments"

static strhash_table_t *handlers;

// locked under the `handlers` table lock
static annotation_handler_t *vg_handlers[VG_ID__LAST];

#if !(defined (WINDOWS) && defined (X64))
static annotation_handler_t vg_router;
static annotation_receiver_t vg_receiver;
static opnd_t vg_router_arg;
#endif

extern uint dr_internal_client_id;
extern file_t main_logfile;
extern ssize_t do_file_write(file_t f, const char *fmt, va_list ap);

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

#define VALGRIND_ANNOTATION_ROL_COUNT 4

typedef enum _annotation_type_t {
    ANNOTATION_TYPE_NONE,
    ANNOTATION_TYPE_EXPRESSION,
    ANNOTATION_TYPE_STATEMENT,
} annotation_type_t;

typedef struct _annotation_instrumentation_t {
    instr_t *head;
    instr_t *tail;
} annotation_instrumentation_t;

typedef struct _annotation_layout_t {
    app_pc start_pc;
    annotation_type_t type;
    bool is_void;
    const char *name;
    app_pc resume_pc;
} annotation_layout_t;

/**** Private Function Declarations ****/

#if !(defined (WINDOWS) && defined (X64))
static void
handle_vg_annotation(app_pc request_args);

static valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request);

static ptr_uint_t
valgrind_discard_translations(vg_client_request_t *request);
#endif

static bool
is_annotation_tag(dcontext_t *dcontext, IN OUT app_pc *start_pc, instr_t *scratch,
                  OUT const char **name);

static void
identify_annotation(dcontext_t *dcontext, IN OUT annotation_layout_t *layout,
                    instr_t *scratch);

static void
specify_args(annotation_handler_t *handler, uint num_args
    _IF_NOT_X64(annotation_calling_convention_t call_type));

#ifdef DEBUG
static void
annot_snprintf(char *buffer, uint length, const char *format, ...);

static ssize_t
annot_printf(const char *format, ...);
#endif

static void
annot_manage_code_area(app_pc start, size_t len);

static void
annot_unmanage_code_area(app_pc start, size_t len);

static void
annot_flush_fragments(app_pc start, size_t len);

static const char *
heap_strcpy(const char *src);

static void
heap_str_free(const char *str);

static void
free_annotation_handler(void *p);

/**** Public Function Definitions ****/

void
annot_init()
{
    handlers = strhash_hash_create(GLOBAL_DCONTEXT, 8, 80,
                                   HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                   HASHTABLE_RELAX_CLUSTER_CHECKS | HASHTABLE_PERSISTENT,
                                   free_annotation_handler
                                   _IF_DEBUG("annotation handler hashtable"));

#if !(defined (WINDOWS) && defined (X64))
    vg_router.type = ANNOT_HANDLER_CALL;
    vg_router.num_args = 1;
    vg_router_arg = opnd_create_reg(DR_REG_XAX);
    vg_router.args = &vg_router_arg;
    vg_router.arg_stack_space = 0;
    vg_router.id.vg_request_id = 0; // routes all requests
    vg_router.receiver_list = &vg_receiver;
    vg_receiver.client_id = dr_internal_client_id;
    vg_receiver.instrumentation.callback = (void *) handle_vg_annotation;
    vg_receiver.save_fpstate = false;
    vg_receiver.next = NULL;
#endif

    dr_annot_register_return(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO_NAME,
                             (void *) (ptr_uint_t) true);
#ifdef DEBUG
    dr_annot_register_call(dr_internal_client_id, DYNAMORIO_ANNOTATE_LOG_NAME,
                           (void *) annot_printf, false, 20
                           _IF_NOT_X64(ANNOT_CALL_TYPE_FASTCALL));
#endif

    if (true) {
        dr_annot_register_call(dr_internal_client_id,
                               DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME,
                               (void *) annot_manage_code_area, false, 2
                               _IF_NOT_X64(ANNOT_CALL_TYPE_FASTCALL));

        dr_annot_register_call(dr_internal_client_id,
                               DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME,
                               (void *) annot_unmanage_code_area, false, 2
                               _IF_NOT_X64(ANNOT_CALL_TYPE_FASTCALL));

        dr_annot_register_call(dr_internal_client_id,
                               DYNAMORIO_ANNOTATE_FLUSH_FRAGMENTS_NAME,
                               (void *) annot_flush_fragments, false, 2
                               _IF_NOT_X64(ANNOT_CALL_TYPE_FASTCALL));
    }

    dr_annot_register_valgrind(dr_internal_client_id, VG_ID__DISCARD_TRANSLATIONS,
                               valgrind_discard_translations);
}

void
annot_exit()
{
    uint i;

    for (i = 0; i < VG_ID__LAST; i++) {
        if (vg_handlers[i] != NULL)
            free_annotation_handler(vg_handlers[i]);
    }

    strhash_hash_destroy(GLOBAL_DCONTEXT, handlers);
}

void
dr_annot_register_call(client_id_t client_id, const char *annotation_name,
                       void *callee, bool save_fpstate, uint num_args
                       _IF_NOT_X64(annotation_calling_convention_t call_type))
{
    annotation_handler_t *handler;
    annotation_receiver_t *receiver;

    TABLE_RWLOCK(handlers, write, lock);

    handler = (annotation_handler_t *) strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           annotation_name);
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(annotation_handler_t));
        handler->type = ANNOT_HANDLER_CALL;
        handler->id.symbol_name = heap_strcpy(annotation_name);
        handler->num_args = num_args;

        if (num_args == 0) {
            handler->args = NULL;
        } else {
            handler->args = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT,
                opnd_t, num_args, ACCT_OTHER, UNPROTECTED);
            specify_args(handler, num_args _IF_NOT_X64(call_type));
        }

        strhash_hash_add(GLOBAL_DCONTEXT, handlers, handler->id.symbol_name, handler);
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
    receiver->client_id = client_id;
    receiver->instrumentation.callback = callee;
    receiver->save_fpstate = save_fpstate;
    receiver->next = handler->receiver_list;
    handler->receiver_list = receiver;

    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_register_valgrind(client_id_t client_id, valgrind_request_id_t request_id,
    ptr_uint_t (*annotation_callback)(vg_client_request_t *request))
{
    annotation_handler_t *handler;
    annotation_receiver_t *receiver;
    if (request_id >= VG_ID__LAST)
        return;

    TABLE_RWLOCK(handlers, write, lock);
    handler = vg_handlers[request_id];
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(annotation_handler_t));
        handler->type = ANNOT_HANDLER_VALGRIND;
        handler->id.vg_request_id = request_id;

        vg_handlers[request_id] = handler;
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
    receiver->client_id = client_id;
    receiver->instrumentation.vg_callback = annotation_callback;
    receiver->save_fpstate = false;
    receiver->next = handler->receiver_list;
    handler->receiver_list = receiver;

    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_register_return(const char *annotation_name, void *return_value)
{
    annotation_handler_t *handler;
    annotation_receiver_t *receiver;

    TABLE_RWLOCK(handlers, write, lock);

    ASSERT_TABLE_SYNCHRONIZED(handlers, WRITE);
    handler = (annotation_handler_t *) strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           annotation_name);
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(annotation_handler_t));
        handler->type = ANNOT_HANDLER_RETURN_VALUE;
        handler->id.symbol_name = heap_strcpy(annotation_name);
        strhash_hash_add(GLOBAL_DCONTEXT, handlers, handler->id.symbol_name, handler);
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
    receiver->client_id = 0; // not used
    receiver->instrumentation.return_value = return_value;
    receiver->save_fpstate = false;
    receiver->next = handler->receiver_list;
    handler->receiver_list = receiver;
    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_unregister_call(client_id_t client_id, const char *annotation_name)
{
    annotation_handler_t *handler;

    TABLE_RWLOCK(handlers, write, lock);

    handler = (annotation_handler_t *) strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           annotation_name);
    if (handler != NULL) {
        annotation_receiver_t *receiver = handler->receiver_list;
        if (receiver->client_id == client_id) {
            if (receiver->next != NULL) {
                handler->receiver_list = receiver->next;
                HEAP_TYPE_FREE(GLOBAL_DCONTEXT, receiver, annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
            }
        } else {
            while (receiver->next != NULL) {
                if (receiver->next->client_id == client_id) {
                    annotation_receiver_t *removal = receiver->next;
                    receiver->next = removal->next;
                    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, removal, annotation_receiver_t,
                                   ACCT_OTHER, UNPROTECTED);
                    break;
                }
                receiver = receiver->next;
            }
        }
    }

    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_unregister_valgrind(client_id_t client_id, valgrind_request_id_t request)
{
    annotation_handler_t *handler;
    TABLE_RWLOCK(handlers, write, lock);
    handler = vg_handlers[request];
    if (handler != NULL) {
        annotation_receiver_t *receiver = handler->receiver_list;
        if (receiver->client_id == client_id) {
            if (receiver->next == NULL) {
                free_annotation_handler(handler);
                vg_handlers[request] = NULL;
            } else {
                handler->receiver_list = receiver->next;
                HEAP_TYPE_FREE(GLOBAL_DCONTEXT, receiver, annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
            }
        } else {
            while (receiver->next != NULL) {
                if (receiver->next->client_id == client_id) {
                    annotation_receiver_t *removal = receiver->next;
                    receiver->next = removal->next;
                    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, removal, annotation_receiver_t,
                                   ACCT_OTHER, UNPROTECTED);
                    break;
                }
                receiver = receiver->next;
            }
        }
    }
    TABLE_RWLOCK(handlers, write, unlock);
}

bool
annot_match(dcontext_t *dcontext, app_pc *start_pc, instr_t **substitution
            _IF_WINDOWS_X64(bool hint_is_safe))
{
    annotation_layout_t layout;
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
    if (hint_byte != 0xcd)
        return NULL;
    layout.start_pc = hint_pc + 2;
#else
    layout.start_pc = *start_pc;
#endif

    // dr_printf("Look for annotation at "PFX"\n", *start_pc);

    instr_init(dcontext, &scratch);
    TRY_EXCEPT(dcontext, {
        identify_annotation(dcontext, &layout, &scratch);
    }, { /* EXCEPT */
        // log it
        //dr_printf("Failed to instrument annotation at "PFX"\n", *start_pc);
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
            annotation_handler_t *handler;

            TABLE_RWLOCK(handlers, write, lock);
            handler = strhash_hash_lookup(GLOBAL_DCONTEXT, handlers, layout.name);
            if (handler != NULL) {
                if (handler->type == ANNOT_HANDLER_CALL) {
                    instr_t *call = INSTR_CREATE_label(dcontext);
                    dr_instr_label_data_t *label_data = instr_get_label_data_area(call);
                    handler->is_void = layout.is_void;

                    instr_set_note(call, (void *) DR_NOTE_ANNOTATION);
                    label_data->data[0] = (ptr_uint_t) handler;
                    label_data->data[1] = ANNOT_NORMAL_CALL;
                    SET_ANNOTATION_APP_PC(label_data, layout.resume_pc);
                    instr_set_ok_to_mangle(call, false);

                    *substitution = call;

                    if (!handler->is_void) {
                        instr_t *return_placeholder =
                            INSTR_XL8(INSTR_CREATE_mov_st(dcontext,
                                                          opnd_create_reg(REG_XAX),
                                                          OPND_CREATE_INT32(0)),
                                      layout.start_pc);
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
                                  layout.start_pc);

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
bool
match_valgrind_pattern(dcontext_t *dcontext, instrlist_t *bb, instr_t *instr,
                       app_pc xchg_pc, uint bb_instr_count)
{
    int i;
    instr_t *return_placeholder;
    instr_t *instr_walk;
    dr_instr_label_data_t *label_data;

    if (!IS_ENCODED_VALGRIND_ANNOTATION(xchg_pc))
        return false;

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
    if (bb_instr_count > VALGRIND_ANNOTATION_ROL_COUNT) {
        instr = instrlist_last(bb);
        for (i = 0; (i < VALGRIND_ANNOTATION_ROL_COUNT) && (instr != NULL); i++) {
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
    label_data->data[1] = ANNOT_NORMAL_CALL;
    label_data->data[2] = (ptr_uint_t) xchg_pc;
    instr_set_ok_to_mangle(instr, false);
    instrlist_append(bb, instr);

    /* Append a write to %xdx, both to ensure it's marked defined by DrMem
     * and to avoid confusion with register analysis code (%xdx is written
     * by the clean callee).
     */
    return_placeholder = INSTR_CREATE_mov_st(dcontext, opnd_create_reg(REG_XDX),
                                             OPND_CREATE_INT32(0));
    instr_set_ok_to_mangle(return_placeholder, false);
    instrlist_append(bb, return_placeholder);

    return true;
}
#endif

/**** Private Function Definitions ****/

#if !(defined (WINDOWS) && defined (X64))
static void
handle_vg_annotation(app_pc request_args)
{
    dcontext_t *dcontext;
    valgrind_request_id_t request_id;
    annotation_receiver_t *receiver;
    vg_client_request_t request;
    ptr_uint_t result;

    if (!safe_read(request_args, sizeof(vg_client_request_t), &request))
        return;

    result = request.default_result;

    request_id = lookup_valgrind_request(request.request);

    if (request_id < VG_ID__LAST) {
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
#ifdef CLIENT_INTERFACE
    if (dcontext->client_data->mcontext_in_dcontext) {
        get_mcontext(dcontext)->xdx = result;
    } else
#endif
    {
        char *state = (char *)dcontext->dstack - sizeof(priv_mcontext_t);
        ((priv_mcontext_t *)state)->xdx = result;
    }
}

static valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request)
{
    switch (request) {
        case VG_USERREQ__RUNNING_ON_VALGRIND:
            return VG_ID__RUNNING_ON_VALGRIND;
        case VG_USERREQ__MAKE_MEM_DEFINED_IF_ADDRESSABLE:
            return VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE;
        case VG_USERREQ__DISCARD_TRANSLATIONS:
            return VG_ID__DISCARD_TRANSLATIONS;
    }
    return VG_ID__LAST;
}

static ptr_uint_t
valgrind_discard_translations(vg_client_request_t *request)
{
    annot_flush_fragments((app_pc) request->args[0], request->args[1]);
    return 0;
}
#endif

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

    // debug assert: next byte is 0xcc
    cur_pc++;
    layout->resume_pc = cur_pc;

    if (strncmp((const char *) layout->name, "statement:", 10) == 0) {
        layout->type = ANNOTATION_TYPE_STATEMENT;
        layout->name += 10;
        layout->is_void = (strncmp((const char *) layout->name, "void:", 5) == 0);
        layout->name = strchr(layout->name, ':') + 1;
        while (true) { // skip fused headers caused by inlining identical annotations
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

                // debug assert: next byte is 0xcc
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
#ifdef WINDOWS
        if (*(cur_pc++) == 0x58) { // skip padding byte (`pop eax` indicates statement)
#else
        if (instr_get_opcode(scratch) == OP_bsf) {
#endif
            layout->type = ANNOTATION_TYPE_STATEMENT;
        } else {
            layout->type = ANNOTATION_TYPE_EXPRESSION;
        }
        instr_reset(dcontext, scratch);
        cur_pc = decode_cti(dcontext, cur_pc, scratch); // assert is `jmp`
        layout->resume_pc = cur_pc;
        layout->is_void = (strncmp((const char *) layout->name, "void:", 5) == 0);
        layout->name = strchr(layout->name, ':') + 1;
    }
}
#endif

#ifdef X64
# ifdef UNIX
static inline void // UNIX x64
specify_args(annotation_handler_t *handler, uint num_args)
{
    uint i;
    for (i = 6; i < num_args; i++) {
        handler->args[i] = OPND_CREATE_MEMPTR(
            DR_REG_XSP, sizeof(ptr_uint_t) * (i-4));
    }
    switch (num_args) {
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
}
# else // WINDOWS x64
static inline void
specify_args(annotation_handler_t *handler, uint num_args)
{
    uint i;
    for (i = 4; i < num_args; i++) {
        handler->args[i] = OPND_CREATE_MEMPTR(
            DR_REG_XSP, sizeof(ptr_uint_t) * (i+1));
    }
    switch (num_args) {
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
}
# endif
#else // x86 (all)
static inline void
specify_args(annotation_handler_t *handler, uint num_args,
             annotation_calling_convention_t call_type)
{
    uint i;
    if (call_type == ANNOT_CALL_TYPE_FASTCALL) {
        for (i = 2; i < num_args; i++) {
            handler->args[i] = OPND_CREATE_MEMPTR(
                DR_REG_XBP, sizeof(ptr_uint_t) * i);
        }
        switch (num_args) {
            default:
                handler->arg_stack_space = (sizeof(ptr_uint_t) * (num_args - 2));
            case 2:
                handler->args[1] = opnd_create_reg(DR_REG_XDX);
            case 1:
                handler->args[0] = opnd_create_reg(DR_REG_XCX);
        }
    } else { // ANNOT_CALL_TYPE_STDCALL
        for (i = 0; i < num_args; i++) {
            handler->args[i] = OPND_CREATE_MEMPTR(
                DR_REG_XBP, sizeof(ptr_uint_t) * i);
        }
        handler->arg_stack_space = (sizeof(ptr_uint_t) * num_args);
    }
}
#endif

#ifdef DEBUG
static void
annot_snprintf(char *buffer, uint length, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, length, format, ap);
    va_end(ap);
}

static ssize_t
annot_printf(const char *format, ...)
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

            annot_snprintf(timestamped, length_before_token, "%s", format);
            sec = (uint) (timestamp / 1000);
            msec = (uint) (timestamp % 1000);
            min = sec / 60;
            sec = sec % 60;
            annot_snprintf(timestamp_buffer, 32, "(%ld:%02ld.%03ld)", min, sec, msec);

            annot_snprintf(timestamped + length_before_token,
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
annot_manage_code_area(app_pc start, size_t len)
{
    LOG(GLOBAL, LOG_ANNOTATIONS, 1, "Manage code area "PFX"-"PFX"\n",
        start, start+len);
    set_region_app_managed(start, len);
}

static void
annot_unmanage_code_area(app_pc start, size_t len)
{
    LOG(GLOBAL, LOG_ANNOTATIONS, 1, "Unmanage code area "PFX"-"PFX"\n",
        start, start+len);
}

static void
annot_flush_fragments(app_pc start, size_t len)
{
    dcontext_t *dcontext = (dcontext_t *) dr_get_current_drcontext();

    LOG(THREAD, LOG_ANNOTATIONS, 2, "Flush fragments "PFX"-"PFX"\n",
        start, start+len);

    if (len == 0 || is_couldbelinking(dcontext))
        return;
    if (!executable_vm_area_executed_from(start, start+len))
        return;

    flush_fragments_in_region_start(dcontext, start, len, false /*don't own initexit*/,
                                    false/*don't free futures*/, false/*exec valid*/,
                                    false/*don't force synchall*/ _IF_DGCDIAG(NULL));

    vm_area_isolate_region(dcontext, start, start+len);

    flush_fragments_in_region_finish(dcontext, false /*don't keep initexit*/);
}

static inline const char *
heap_strcpy(const char *src)
{
    size_t len = strlen(src) + 1;
    char *dst = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, char, len,
                                 ACCT_OTHER, UNPROTECTED);
    memcpy(dst, src, len);
    return (const char *) dst;
}

static inline void
heap_str_free(const char *str)
{
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, str, char, strlen(str) + 1,
                    ACCT_OTHER, UNPROTECTED);
}

static void
free_annotation_handler(void *p)
{
    annotation_handler_t *handler = (annotation_handler_t *) p;
    annotation_receiver_t *next, *receiver = handler->receiver_list;
    while (receiver != NULL) {
        next = receiver->next;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, receiver, annotation_receiver_t,
                       ACCT_OTHER, UNPROTECTED);
        receiver = next;
    }
    if (handler->num_args > 0) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, handler->args, opnd_t, handler->num_args,
                        ACCT_OTHER, UNPROTECTED);
    }
    if ((handler->type == ANNOT_HANDLER_CALL) ||
        (handler->type == ANNOT_HANDLER_RETURN_VALUE))
        heap_str_free(handler->id.symbol_name);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, p, annotation_handler_t, ACCT_OTHER, UNPROTECTED);
}
