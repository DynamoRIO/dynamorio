/* ******************************************************
 * Copyright (c) 2014-2023 Google, Inc.  All rights reserved.
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

#include "globals.h"
#include "hashtable.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "decode_fast.h"
#include "utils.h"
#include "annotations.h"

#ifdef ANNOTATIONS /* around whole file */

#    if !(defined(WINDOWS) && defined(X64))
#        include "valgrind.h"
#        include "memcheck.h"
#    endif

/* Macros for identifying an annotation head and extracting the pointer to its name.
 *
 * IS_ANNOTATION_LABEL_INSTR(instr):               Evaluates to true for any `instr`
 * that could be the instruction which encodes the pointer to the annotation name.
 * IS_ANNOTATION_LABEL_REFERENCE(opnd):            Evaluates to true for any `opnd`
 * that could be the operand which encodes the pointer to the annotation name.
 * GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc):  Extracts the annotation name
 * pointer. IS_ANNOTATION_LABEL_GOT_OFFSET_INSTR(instr):    Evaluates to true for any
 * `instr` that could encode the offset of the label's GOT entry within the GOT table.
 * IS_ANNOTATION_LABEL_GOT_OFFSET_REFERENCE(opnd): Evaluates to true for any `opnd`
 * that could encode the offset of the label's GOT entry within the GOT table.
 */
#    ifdef WINDOWS
#        ifdef X64
#            define IS_ANNOTATION_LABEL_INSTR(instr) \
                (instr_is_mov(instr) || (instr_get_opcode(instr) == OP_prefetchw))
#            define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_rel_addr(opnd)
#            define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) opnd_get_addr(src)
#        else
#            define IS_ANNOTATION_LABEL_INSTR(instr) instr_is_mov(instr)
#            define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_base_disp(opnd)
#            define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) \
                ((app_pc)opnd_get_disp(src))
#        endif
#    else
#        define IS_ANNOTATION_LABEL_INSTR(instr) instr_is_mov(instr)
#        define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_base_disp(opnd)
#        ifdef X64
#            define ANNOTATION_LABEL_REFERENCE_OPERAND_OFFSET 4
#        else
#            define ANNOTATION_LABEL_REFERENCE_OPERAND_OFFSET 0
#        endif
#        define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) \
            ((app_pc)(opnd_get_disp(src) + (instr_pc) +       \
                      ANNOTATION_LABEL_REFERENCE_OPERAND_OFFSET))
#        define IS_ANNOTATION_LABEL_GOT_OFFSET_INSTR(instr) \
            (instr_get_opcode(scratch) == OP_bsf || instr_get_opcode(scratch) == OP_bsr)
#        define IS_ANNOTATION_LABEL_GOT_OFFSET_REFERENCE(opnd) opnd_is_base_disp(opnd)
#    endif

/* Annotation label components. */
#    define DYNAMORIO_ANNOTATION_LABEL "dynamorio-annotation"
#    define DYNAMORIO_ANNOTATION_LABEL_LENGTH 20
#    define ANNOTATION_STATEMENT_LABEL "statement"
#    define ANNOTATION_STATEMENT_LABEL_LENGTH 9
#    define ANNOTATION_EXPRESSION_LABEL "expression"
#    define ANNOTATION_EXPRESSION_LABEL_LENGTH 10
#    define ANNOTATION_VOID_LABEL "void"
#    define ANNOTATION_VOID_LABEL_LENGTH 4
#    define IS_ANNOTATION_STATEMENT_LABEL(annotation_name)                    \
        (strncmp((const char *)(annotation_name), ANNOTATION_STATEMENT_LABEL, \
                 ANNOTATION_STATEMENT_LABEL_LENGTH) == 0)
#    define IS_ANNOTATION_VOID(annotation_name)                              \
        (strncmp((const char *)(annotation_name), ANNOTATION_VOID_LABEL ":", \
                 ANNOTATION_VOID_LABEL_LENGTH) == 0)

/* Annotation detection factors exclusive to Windows x64. */
#    if defined(WINDOWS) && defined(X64)
/* Instruction `int 2c` hints that the preceding cbr is probably an annotation
 * head. */
#        define WINDOWS_X64_ANNOTATION_HINT_BYTE 0xcd
#        define X64_WINDOWS_ENCODED_ANNOTATION_HINT 0x2ccd
/* Instruction `int 3` acts as a boundary for compiler optimizations, to prevent
 * the the annotation from being transformed into something unrecognizable.
 */
#        define WINDOWS_X64_OPTIMIZATION_FENCE 0xcc
#        define IS_ANNOTATION_HEADER(scratch, pc) \
            (instr_is_cbr(scratch) &&             \
             (*(ushort *)(pc) == X64_WINDOWS_ENCODED_ANNOTATION_HINT))
#    endif

/* OPND_RETURN_VALUE: create the return value operand for `mov $return_value,%xax`. */
#    ifdef X64
#        define OPND_RETURN_VALUE(return_value) OPND_CREATE_INT64(return_value)
#    else
#        define OPND_RETURN_VALUE(return_value) OPND_CREATE_INT32(return_value)
#    endif

/* FASTCALL_REGISTER_ARG_COUNT: Specifies the number of arguments passed in registers.
 */
#    ifdef X64
#        ifdef UNIX
#            define FASTCALL_REGISTER_ARG_COUNT 6
#        else /* WINDOWS x64 */
#            define FASTCALL_REGISTER_ARG_COUNT 4
#        endif
#    else /* x86 (all) */
#        define FASTCALL_REGISTER_ARG_COUNT 2
#    endif

#    define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO_NAME \
        "dynamorio_annotate_running_on_dynamorio"

#    define DYNAMORIO_ANNOTATE_LOG_NAME "dynamorio_annotate_log"

#    define DYNAMORIO_ANNOTATE_LOG_ARG_COUNT 20

/* Facilitates timestamp substitution in `dynamorio_annotate_log()`. */
#    define LOG_ANNOTATION_TIMESTAMP_TOKEN "${timestamp}"
#    define LOG_ANNOTATION_TIMESTAMP_TOKEN_LENGTH 12

/* Constant factors of the Valgrind annotation, as defined in valgrind.h. */
enum {
    VG_ROL_COUNT = 4,
};

typedef enum _annotation_type_t {
    /* Indicates that the analyzed instruction turned out not to be an annotation head. */
    ANNOTATION_TYPE_NONE,
    /* To invoke an annotation as an expression, the target app calls the annotation as if
     * it were a normal function. The annotation instruction sequence follows the preamble
     * of each annotation function, and instrumentation replaces it with (1) a clean call
     * to each registered handler for that annotation, or (2) a return value substitution,
     * depending on the type of registration.
     */
    ANNOTATION_TYPE_EXPRESSION,
    /* To invoke an annotation as a statement, the target app calls a macro defined in
     * the annotation header (via dr_annotations_asm.h), which places the annotation
     * instruction sequence inline at the invocation site. The sequence includes a normal
     * call to the annotation function, so instrumentation simply involves removing the
     * surrounding components of the annotation to expose the call. The DR client's clean
     * calls will be invoked within the annotation function itself (see above).
     */
    ANNOTATION_TYPE_STATEMENT,
} annotation_type_t;

/* Specifies the exact byte position of the essential components of an annotation. */
typedef struct _annotation_layout_t {
    app_pc start_pc;
    annotation_type_t type;
    /* Indicates whether the annotation function in the target app is of void type. */
    bool is_void;
    /* Points to the annotation name in the target app's data section. */
    const char *name;
    /* Specifies the translation of the annotation instrumentation (e.g., clean call). */
    app_pc substitution_xl8;
    /* Specifies the byte at which app decoding should resume following the annotation. */
    app_pc resume_pc;
} annotation_layout_t;

#    if !(defined(WINDOWS) && defined(X64))
typedef struct _vg_handlers_t {
    dr_annotation_handler_t *handlers[DR_VG_ID__LAST];
} vg_handlers_t;
#    endif

static strhash_table_t *handlers;

#    if !(defined(WINDOWS) && defined(X64))
/* locked under the `handlers` table lock */
static vg_handlers_t *vg_handlers;

/* Dispatching function for Valgrind annotations (required because id of the Valgrind
 * client request object cannot be determined statically).
 */
static dr_annotation_handler_t vg_router;
static dr_annotation_receiver_t vg_receiver; /* The sole receiver for `vg_router`. */
static opnd_t vg_router_arg; /* The sole argument for the clean call to `vg_router`. */
#    endif

/* Required for passing the va_list in `dynamorio_annotate_log()` to the log function. */
extern ssize_t
do_file_write(file_t f, const char *fmt, va_list ap);

/*********************************************************
 * INTERNAL ROUTINE DECLARATIONS
 */

#    if !(defined(WINDOWS) && defined(X64))
/* Valgrind dispatcher, called by the instrumentation of the Valgrind annotations. */
static void
handle_vg_annotation(app_pc request_args);

/* Maps a Valgrind request id into a (sequential) `DR_VG_ID__*` constant. */
static dr_valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request);

static ptr_uint_t
valgrind_running_on_valgrind(dr_vg_client_request_t *request);
#    endif

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

#    ifdef DEBUG
/* Implements `dynamorio_annotate_log()`, including substitution of the string literal
 * token "${timestamp}" with the current system time.
 */
static ssize_t
annotation_printf(const char *format, ...);
#    endif

/* Invoked during hashtable entry removal */
static void
free_annotation_handler(void *p);

/*********************************************************
 * ANNOTATION INTEGRATION FUNCTIONS
 */

void
annotation_init()
{
    handlers = strhash_hash_create(
        GLOBAL_DCONTEXT, 8, 80, /* favor a small table */
        HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED | HASHTABLE_RELAX_CLUSTER_CHECKS |
            HASHTABLE_PERSISTENT,
        free_annotation_handler _IF_DEBUG("annotation handler hashtable"));

#    if !(defined(WINDOWS) && defined(X64))
    vg_handlers =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, vg_handlers_t, ACCT_OTHER, UNPROTECTED);
    memset(vg_handlers, 0, sizeof(vg_handlers_t));

    vg_router.type = DR_ANNOTATION_HANDLER_CALL;
    /* The Valgrind client request object is passed in %xax. */
    vg_router.num_args = 1;
    vg_router_arg = opnd_create_reg(DR_REG_XAX);
    vg_router.args = &vg_router_arg;
    vg_router.symbol_name = NULL; /* No symbols in Valgrind annotations. */
    vg_router.receiver_list = &vg_receiver;
    vg_receiver.instrumentation.callback = (void *)(void (*)())handle_vg_annotation;
    vg_receiver.save_fpstate = false;
    vg_receiver.next = NULL;
#    endif

    dr_annotation_register_return(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO_NAME,
                                  (void *)(ptr_uint_t) true);
#    ifdef DEBUG
    /* The logging annotation requires a debug build of DR. Arbitrarily allows up to
     * 20 arguments, since the clean call must have a fixed number of them.
     */
    dr_annotation_register_call(DYNAMORIO_ANNOTATE_LOG_NAME, (void *)annotation_printf,
                                false, DYNAMORIO_ANNOTATE_LOG_ARG_COUNT,
                                DR_ANNOTATION_CALL_TYPE_VARARG);
#    endif

#    if !(defined(WINDOWS) && defined(X64))
    /* DR pretends to be Valgrind. */
    dr_annotation_register_valgrind(DR_VG_ID__RUNNING_ON_VALGRIND,
                                    valgrind_running_on_valgrind);
#    endif
}

void
annotation_exit()
{
#    if !(defined(WINDOWS) && defined(X64))
    uint i;
    for (i = 0; i < DR_VG_ID__LAST; i++) {
        if (vg_handlers->handlers[i] != NULL)
            free_annotation_handler(vg_handlers->handlers[i]);
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, vg_handlers, vg_handlers_t, ACCT_OTHER, UNPROTECTED);
#    endif

    strhash_hash_destroy(GLOBAL_DCONTEXT, handlers);
}

bool
instrument_annotation(dcontext_t *dcontext, IN OUT app_pc *start_pc,
                      OUT instr_t **substitution _IF_WINDOWS_X64(IN bool hint_is_safe))
{
    annotation_layout_t layout = { 0 };
    /* This instr_t is used for analytical decoding throughout the detection functions.
     * It is passed on the stack and its contents are considered void on function entry.
     */
    instr_t scratch;
#    if defined(WINDOWS) && defined(X64)
    app_pc hint_pc = *start_pc;
    bool hint = true;
    byte hint_byte;
#    endif
    /* We need to use the passed-in cxt for IR but we need a real one for TRY_EXCEPT. */
    dcontext_t *my_dcontext;
    if (dcontext == GLOBAL_DCONTEXT)
        my_dcontext = get_thread_private_dcontext();
    else
        my_dcontext = dcontext;

#    if defined(WINDOWS) && defined(X64)
    if (hint_is_safe) {
        hint_byte = *hint_pc;
    } else {
        if (!d_r_safe_read(hint_pc, 1, &hint_byte))
            return false;
    }
    if (hint_byte != WINDOWS_X64_ANNOTATION_HINT_BYTE)
        return false;
    /* The hint is the first byte of the 2-byte instruction `int 2c`. Skip both bytes. */
    layout.start_pc = hint_pc + INT_LENGTH;
    layout.substitution_xl8 = layout.start_pc;
#    else
    layout.start_pc = *start_pc;
#    endif

    instr_init(dcontext, &scratch);
    TRY_EXCEPT(
        my_dcontext, { identify_annotation(dcontext, &layout, &scratch); },
        { /* EXCEPT */
          LOG(THREAD, LOG_ANNOTATIONS, 2, "Failed to instrument annotation at " PFX "\n",
              *start_pc);
          /* layout.type is already ANNOTATION_TYPE_NONE */
        });
    if (layout.type != ANNOTATION_TYPE_NONE) {
        LOG(GLOBAL, LOG_ANNOTATIONS, 2,
            "Decoded %s annotation %s. Next pc now " PFX ".\n",
            (layout.type == ANNOTATION_TYPE_EXPRESSION) ? "expression" : "statement",
            layout.name, layout.resume_pc);
        /* Notify the caller where to resume decoding app instructions. */
        *start_pc = layout.resume_pc;

        if (layout.type == ANNOTATION_TYPE_EXPRESSION) {
            dr_annotation_handler_t *handler;

            TABLE_RWLOCK(handlers, write, lock);
            handler = strhash_hash_lookup(GLOBAL_DCONTEXT, handlers, layout.name);
            if (handler != NULL && handler->type == DR_ANNOTATION_HANDLER_CALL) {
                /* Substitute the annotation with a label pointing to the handler. */
                instr_t *call = INSTR_CREATE_label(dcontext);
                dr_instr_label_data_t *label_data = instr_get_label_data_area(call);
                SET_ANNOTATION_HANDLER(label_data, handler);
                SET_ANNOTATION_APP_PC(label_data, layout.resume_pc);
                instr_set_note(call, (void *)DR_NOTE_ANNOTATION);
                instr_set_meta(call);
                *substitution = call;

                handler->is_void = layout.is_void;
                if (!handler->is_void) {
                    /* Append `mov $0x0,%eax` to the annotation substitution, so that
                     * clients and tools recognize that %xax will be written here.
                     * The placeholder is "ok to mangle" because it (partially)
                     * implements the app's annotation. The placeholder will be
                     * removed post-client during mangling.
                     * We only support writing the return value and no other registers
                     * (otherwise we'd need drrg to further special-case
                     * DR_NOTE_ANNOTATION).
                     */
                    instr_t *return_placeholder =
                        INSTR_XL8(INSTR_CREATE_mov_st(dcontext, opnd_create_reg(REG_XAX),
                                                      OPND_CREATE_INT32(0)),
                                  layout.substitution_xl8);
                    instr_set_note(return_placeholder, (void *)DR_NOTE_ANNOTATION);
                    /* Append the placeholder manually, because the caller can call
                     * `instrlist_append()` with a "sublist" of instr_t.
                     */
                    instr_set_next(*substitution, return_placeholder);
                    instr_set_prev(return_placeholder, *substitution);
                }
            } else { /* Substitute the annotation with `mov $return_value,%eax` */
                void *return_value;

                if (handler == NULL)
                    return_value = NULL; /* Return nothing if no handler is registered */
                else
                    return_value = handler->receiver_list->instrumentation.return_value;
                *substitution =
                    INSTR_XL8(INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                                   OPND_RETURN_VALUE(return_value)),
                              layout.substitution_xl8);
            }
            TABLE_RWLOCK(handlers, write, unlock);
        }
        /* else (layout.type == ANNOTATION_TYPE_STATEMENT), in which case the only
         * instrumentation is to remove the jump-over-annotation such that the annotation
         * function gets called like a normal function. Instrumentation of clean calls and
         * return values will then occur within the annotation function (the case above).
         */
    }
    instr_free(dcontext, &scratch);
    return (layout.type != ANNOTATION_TYPE_NONE);
}

#    if !(defined(WINDOWS) && defined(X64))
void
instrument_valgrind_annotation(dcontext_t *dcontext, instrlist_t *bb, instr_t *xchg_instr,
                               app_pc xchg_pc, app_pc next_pc, uint bb_instr_count)
{
    int i;
    app_pc instrumentation_pc = NULL;
    instr_t *return_placeholder;
    instr_t *instr, *next_instr;
    dr_instr_label_data_t *label_data;

    LOG(THREAD, LOG_ANNOTATIONS, 2,
        "Matched valgrind client request pattern at " PFX "\n",
        instr_get_app_pc(xchg_instr));

    /* We leave the argument gathering code as app instructions, because it writes to app
     * registers (xref i#1423). Now delete the `xchg` instruction, and the `rol`
     * instructions--unless a previous BB contains some of the `rol`, in which case they
     * must be executed to avoid messing up %xdi (the 4 `rol` compose to form a nop).
     * Note: in the case of a split `rol` sequence, we only instrument the second half.
     */
    instr_destroy(dcontext, xchg_instr);
    if (bb_instr_count > VG_ROL_COUNT) {
        instr = instrlist_last(bb);
        for (i = 0; i < VG_ROL_COUNT; i++) {
            ASSERT(instr != NULL && instr_get_opcode(instr) == OP_rol);
            next_instr = instr_get_prev(instr);
            instrlist_remove(bb, instr);
            instr_destroy(dcontext, instr);
            instr = next_instr;
        }
    }

    /* i#1613: if the client removes the app instruction prior to the annotation, we will
     * skip the annotation instrumentation, so identify the pc of that instruction here.
     */
    if (instrlist_last(bb) != NULL)
        instrumentation_pc = instrlist_last(bb)->translation;

    /* Substitute the annotation tail with a label pointing to the Valgrind handler. */
    instr = INSTR_CREATE_label(dcontext);
    instr_set_note(instr, (void *)DR_NOTE_ANNOTATION);
    label_data = instr_get_label_data_area(instr);
    SET_ANNOTATION_HANDLER(label_data, &vg_router);
    SET_ANNOTATION_APP_PC(label_data, next_pc);
    SET_ANNOTATION_INSTRUMENTATION_PC(label_data, instrumentation_pc);
    instr_set_meta(instr);
    instrlist_append(bb, instr);

    /* Append `mov $0x0,%edx` so that clients and tools recognize that %xdx will be
     * written here. The placeholder is "ok to mangle" because it (partially) implements
     * the app's annotation. The placeholder will be removed post-client during mangling.
     * We only support writing the return value and no other registers (otherwise
     * we'd need drreg to further special-case DR_NOTE_ANNOTATION).
     */
    return_placeholder = INSTR_XL8(
        INSTR_CREATE_mov_st(dcontext, opnd_create_reg(REG_XDX), OPND_CREATE_INT32(0)),
        xchg_pc);
    instr_set_note(return_placeholder, (void *)DR_NOTE_ANNOTATION);
    instrlist_append(bb, return_placeholder);
}
#    endif

/*********************************************************
 * ANNOTATION API FUNCTIONS
 */

bool
dr_annotation_register_call(const char *annotation_name, void *callee, bool save_fpstate,
                            uint num_args, dr_annotation_calling_convention_t call_type)
{
    bool result = true;
    dr_annotation_handler_t *handler;
    dr_annotation_receiver_t *receiver;

    TABLE_RWLOCK(handlers, write, lock);

    handler = (dr_annotation_handler_t *)strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                             annotation_name);
    if (handler == NULL) { /* make a new handler if never registered yet */
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_handler_t, ACCT_OTHER,
                                  UNPROTECTED);
        memset(handler, 0, sizeof(dr_annotation_handler_t));
        handler->type = DR_ANNOTATION_HANDLER_CALL;
        handler->symbol_name = dr_strdup(annotation_name HEAPACCT(ACCT_OTHER));
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
    if (handler->type == DR_ANNOTATION_HANDLER_CALL || handler->receiver_list == NULL) {
        /* If the annotation previously had a return value registration, it can be changed
         * to clean call instrumentation, provided the return value was unregistered.
         */
        handler->type = DR_ANNOTATION_HANDLER_CALL;
        receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_receiver_t, ACCT_OTHER,
                                   UNPROTECTED);
        receiver->instrumentation.callback = callee;
        receiver->save_fpstate = save_fpstate;
        receiver->next = handler->receiver_list; /* push the new receiver onto the list */
        handler->receiver_list = receiver;
    } else {
        result = false; /* A return value is registered, so no call can be added. */
    }

    TABLE_RWLOCK(handlers, write, unlock);
    return result;
}

#    if !(defined(WINDOWS) && defined(X64))
bool
dr_annotation_register_valgrind(
    dr_valgrind_request_id_t request_id,
    ptr_uint_t (*annotation_callback)(dr_vg_client_request_t *request))
{
    dr_annotation_handler_t *handler;
    dr_annotation_receiver_t *receiver;
    if (request_id >= DR_VG_ID__LAST)
        return false;

    TABLE_RWLOCK(handlers, write, lock);
    handler = vg_handlers->handlers[request_id];
    if (handler == NULL) { /* make a new handler if never registered yet */
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_handler_t, ACCT_OTHER,
                                  UNPROTECTED);
        memset(handler, 0, sizeof(dr_annotation_handler_t));
        handler->type = DR_ANNOTATION_HANDLER_VALGRIND;

        vg_handlers->handlers[request_id] = handler;
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_receiver_t, ACCT_OTHER,
                               UNPROTECTED);
    receiver->instrumentation.vg_callback = annotation_callback;
    receiver->save_fpstate = false;
    receiver->next = handler->receiver_list; /* push the new receiver onto the list */
    handler->receiver_list = receiver;

    TABLE_RWLOCK(handlers, write, unlock);
    return true;
}
#    endif

bool
dr_annotation_register_return(const char *annotation_name, void *return_value)
{
    bool result = true;
    dr_annotation_handler_t *handler;
    dr_annotation_receiver_t *receiver;

    TABLE_RWLOCK(handlers, write, lock);
    ASSERT_TABLE_SYNCHRONIZED(handlers, WRITE);
    handler = (dr_annotation_handler_t *)strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                             annotation_name);
    if (handler == NULL) { /* make a new handler if never registered yet */
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_handler_t, ACCT_OTHER,
                                  UNPROTECTED);
        memset(handler, 0, sizeof(dr_annotation_handler_t));
        handler->type = DR_ANNOTATION_HANDLER_RETURN_VALUE;
        handler->symbol_name = dr_strdup(annotation_name HEAPACCT(ACCT_OTHER));
        strhash_hash_add(GLOBAL_DCONTEXT, handlers, handler->symbol_name, handler);
    }
    if (handler->receiver_list == NULL) {
        /* If the annotation previously had clean call registration, it can be changed to
         * return value instrumentation, provided the calls have been unregistered.
         */
        handler->type = DR_ANNOTATION_HANDLER_RETURN_VALUE;
        receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dr_annotation_receiver_t, ACCT_OTHER,
                                   UNPROTECTED);
        receiver->instrumentation.return_value = return_value;
        receiver->save_fpstate = false;
        receiver->next = NULL; /* Return value can only have one implementation. */
        handler->receiver_list = receiver;
    } else {
        result = false; /* Existing handler prevents the new return value. */
    }
    TABLE_RWLOCK(handlers, write, unlock);
    return result;
}

bool
dr_annotation_pass_pc(const char *annotation_name)
{
    bool result = true;
    dr_annotation_handler_t *handler;

    TABLE_RWLOCK(handlers, write, lock);
    ASSERT_TABLE_SYNCHRONIZED(handlers, WRITE);
    handler = (dr_annotation_handler_t *)strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                             annotation_name);
    if (handler == NULL) {
        result = false;
    } else {
        handler->pass_pc_in_slot = true;
    }
    TABLE_RWLOCK(handlers, write, unlock);
    return result;
}

bool
dr_annotation_unregister_call(const char *annotation_name, void *callee)
{
    bool found = false;
    dr_annotation_handler_t *handler;

    TABLE_RWLOCK(handlers, write, lock);
    handler = (dr_annotation_handler_t *)strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
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
    handler = (dr_annotation_handler_t *)strhash_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                             annotation_name);
    if ((handler != NULL) && (handler->receiver_list != NULL)) {
        ASSERT(handler->receiver_list->next == NULL);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, handler->receiver_list, dr_annotation_receiver_t,
                       ACCT_OTHER, UNPROTECTED);
        handler->receiver_list = NULL;
        found = true;
    } /* Leave the handler for the next registration (free it on exit) */
    TABLE_RWLOCK(handlers, write, unlock);
    return found;
}

#    if !(defined(WINDOWS) && defined(X64))
bool
dr_annotation_unregister_valgrind(
    dr_valgrind_request_id_t request_id,
    ptr_uint_t (*annotation_callback)(dr_vg_client_request_t *request))
{
    bool found = false;
    dr_annotation_handler_t *handler;
    TABLE_RWLOCK(handlers, write, lock);
    handler = vg_handlers->handlers[request_id];
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

static void
handle_vg_annotation(app_pc request_args)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    dr_valgrind_request_id_t request_id;
    dr_annotation_receiver_t *receiver;
    dr_vg_client_request_t request;
    ptr_uint_t result;

    if (!d_r_safe_read(request_args, sizeof(dr_vg_client_request_t), &request)) {
        LOG(THREAD, LOG_ANNOTATIONS, 2,
            "Failed to read Valgrind client request args at " PFX
            ". Skipping the annotation.\n",
            request_args);
        return;
    }

    result = request.default_result;
    request_id = lookup_valgrind_request(request.request);
    if (request_id < DR_VG_ID__LAST) {
        TABLE_RWLOCK(handlers, read, lock);
        if (vg_handlers->handlers[request_id] != NULL) {
            receiver = vg_handlers->handlers[request_id]->receiver_list;
            while (receiver != NULL) {
                result = receiver->instrumentation.vg_callback(&request);
                receiver = receiver->next;
            }
        }
        TABLE_RWLOCK(handlers, read, unlock);
    } else {
        LOG(THREAD, LOG_ANNOTATIONS, 2,
            "Skipping unrecognized Valgrind client request id %d\n", request.request);
        return;
    }

    /* Put the result in %xdx where the target app expects to find it. */
    if (dcontext->client_data->mcontext_in_dcontext) {
        get_mcontext(dcontext)->xdx = result;
    } else {
        priv_mcontext_t *state = get_priv_mcontext_from_dstack(dcontext);
        state->xdx = result;
    }
}

static dr_valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request)
{
    switch (request) {
    case VG_USERREQ__RUNNING_ON_VALGRIND: return DR_VG_ID__RUNNING_ON_VALGRIND;
    case VG_USERREQ__DO_LEAK_CHECK: return DR_VG_ID__DO_LEAK_CHECK;
    case VG_USERREQ__MAKE_MEM_DEFINED_IF_ADDRESSABLE:
        return DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE;
    case VG_USERREQ__DISCARD_TRANSLATIONS: return DR_VG_ID__DISCARD_TRANSLATIONS;
    }
    return DR_VG_ID__LAST;
}

static ptr_uint_t
valgrind_running_on_valgrind(dr_vg_client_request_t *request)
{
    return 1; /* Pretend to be Valgrind. */
}
#    endif

/*********************************************************
 * INTERNAL ROUTINES
 */

/* If the app code at `*cur_pc` can be read as an encoded annotation label, then:
 *   (1) advance `*cur_pc` beyond the last label instruction,
 *   (2) point `**name` to the start of the label within the app image data section, and
 *   (3) return true.
 * If there is no annotation label at `*cur_pc`, or failure occurs while trying to read
 * it, then return false with undefined values for `*cur_pc` and `**name`.
 *
 * On Unix and Windows x86, the label has the form
 * "<annotation-label>:<return-type>:<annotation-name>, e.g.
 *
 *     "dynamorio-annotation:void:dynamorio_annotate_log"
 *     "dynamorio-annotation:const char *:some_custom_client_annotation"
 *
 * On Windows x64, the label has an additional token for the annotation type:
 * "<annotation-label>:<annotation-type>:<return-type>:<annotation-name>, e.g.
 *
 *     "dynamorio-annotation:statement:void:dynamorio_annotate_log"
 *     "dynamorio-annotation:expression:const char *:some_custom_client_annotation"
 *
 * The encoding of the pointer to the label varies by platform:
 *
 *   Unix expression annotation label (two instructions form <GOT-base> + <GOT-offset>):
 *
 *     mov $<GOT-base>,%xax
 *     bsr $<GOT-offset>,%xax
 *
 *   Windows x86 expression annotation label (direct pointer to the <label>):
 *
 *     mov $<label>,%eax
 *
 *   Windows x64 expression annotation label (same, in 2 variations):
 *
 *     mov $<label>,%rax      Or      prefetch $<label>
 *     prefetch %rax
 *
 *   Decoding the label pointer proceeds as follows:
 *
 *     (step 1) check `*cur_pc` for the opcode of the first label-encoding instruction
 *     (step 2) check the operand of that instruction for a label-encoding operand type
 *     Unix only--dereference the GOT entry:
 *         (step 3) check the next instruction for the label offset opcode (bsr or bsf)
 *         (step 4) check its operand for the offset-encoding operand type (base disp)
 *         (step 5) add the two operands and dereference as the label's GOT entry
 *     (step 6) attempt to read the decoded pointer and compare to "dynamorio-annotation"
 *              (note there is a special case for Windows x64, which is not inline asm)
 *     (step 7) if it matches, point `**name` to the character beyond the separator ':'
 *
 * See https://dynamorio.org/page_annotations.html for complete examples.
 */
static inline bool
is_annotation_tag(dcontext_t *dcontext, IN OUT app_pc *cur_pc, instr_t *scratch,
                  OUT const char **name)
{
    app_pc start_pc = *cur_pc;

    instr_reset(dcontext, scratch);
    *cur_pc = decode(dcontext, *cur_pc, scratch);
    if (IS_ANNOTATION_LABEL_INSTR(scratch)) { /* step 1 */
        opnd_t src = instr_get_src(scratch, 0);
        if (IS_ANNOTATION_LABEL_REFERENCE(src)) { /* step 2 */
            char buf[DYNAMORIO_ANNOTATION_LABEL_LENGTH + 1 /*nul*/];
            app_pc buf_ptr;
            app_pc opnd_ptr = GET_ANNOTATION_LABEL_REFERENCE(src, start_pc);
#    ifdef UNIX
            app_pc got_ptr;
            instr_reset(dcontext, scratch);
            *cur_pc = decode(dcontext, *cur_pc, scratch);
            if (!IS_ANNOTATION_LABEL_GOT_OFFSET_INSTR(scratch)) /* step 3 */
                return false;
            src = instr_get_src(scratch, 0);
            if (!IS_ANNOTATION_LABEL_GOT_OFFSET_REFERENCE(src)) /* step 4 */
                return false;
            opnd_ptr += opnd_get_disp(src); /* step 5 */
            if (!d_r_safe_read(opnd_ptr, sizeof(app_pc), &got_ptr))
                return false;
            opnd_ptr = got_ptr;
#    endif
#    if defined(WINDOWS) && defined(X64)
            /* In Windows x64, if the `prefetch` instruction was found at
             * `*cur_pc` with no intervening `mov` instruction, the label
             * pointer must be an immediate operand to that `prefetch`.
             */
            if (instr_get_opcode(scratch) == OP_prefetchw) { /* step 6 */
                if (!d_r_safe_read(opnd_ptr, DYNAMORIO_ANNOTATION_LABEL_LENGTH, buf))
                    return false;
                buf[DYNAMORIO_ANNOTATION_LABEL_LENGTH] = '\0';
                if (strcmp(buf, DYNAMORIO_ANNOTATION_LABEL) == 0) {
                    *name = (const char *)(opnd_ptr + /* step 7 */
                                           DYNAMORIO_ANNOTATION_LABEL_LENGTH +
                                           1); /* skip the separator ':' */
                    return true;
                }
            } /* else the label pointer is the usual `mov` operand: */
#    endif
            if (!d_r_safe_read(opnd_ptr, sizeof(app_pc), &buf_ptr)) /* step 6 */
                return false;
            if (!d_r_safe_read(buf_ptr, DYNAMORIO_ANNOTATION_LABEL_LENGTH, buf))
                return false;
            buf[DYNAMORIO_ANNOTATION_LABEL_LENGTH] = '\0';
            if (strcmp(buf, DYNAMORIO_ANNOTATION_LABEL) != 0)
                return false;
            *name = (const char *)(buf_ptr + /* step 7 */
                                   DYNAMORIO_ANNOTATION_LABEL_LENGTH +
                                   1); /* skip the separator ':' */
            return true;
        }
    }
    return false;
}

#    if defined(WINDOWS) && defined(X64)
/* Identify the annotation at layout->start_pc, if any. On Windows x64, some flexibility
 * is required to recognize the annotation sequence because it is compiled instead of
 * explicitly inserted with inline asm (which is unavailable in MSVC for this platform).
 *   (step 1) check if the instruction at layout->start_pc encodes an annotation label.
 *   (step 2) decode arbitrary instructions up to a `prefetch`,
 *            following any direct branches decoded along the way.
 *   (step 3) skip the `int 3` following the `prefetch`.
 *   (step 4) set the substitution_xl8 to precede the resume_pc, so we'll resume in the
 *            right place if the client removes all instrs following the substitution.
 *   (step 5) set the resume pc, so execution resumes within the annotation body.
 *   (step 6) compare the next label token to determine the annotation type.
 *     Expression annotations only:
 *       (step 7) compare the return type to determine whether the annotation is void.
 *   (step 8) advance the label pointer to the name token
 *            (note that the Statement annotation concludes with a special case).
 */
static inline void
identify_annotation(dcontext_t *dcontext, IN OUT annotation_layout_t *layout,
                    instr_t *scratch)
{
    app_pc cur_pc = layout->start_pc, last_call = NULL;

    if (!is_annotation_tag(dcontext, &cur_pc, scratch, &layout->name)) /* step 1 */
        return;

    while (instr_get_opcode(scratch) != OP_prefetchw) { /* step 2 */
        if (instr_is_ubr(scratch))
            cur_pc = instr_get_branch_target_pc(scratch);
        instr_reset(dcontext, scratch);
        cur_pc = decode(dcontext, cur_pc, scratch);
    }

    ASSERT(*cur_pc == WINDOWS_X64_OPTIMIZATION_FENCE); /* step 3 */
    layout->substitution_xl8 = cur_pc;                 /* step 4 */
    cur_pc++;
    layout->resume_pc = cur_pc; /* step 5 */

    if (IS_ANNOTATION_STATEMENT_LABEL(layout->name)) { /* step 6 */
        layout->type = ANNOTATION_TYPE_STATEMENT;
        layout->name += (ANNOTATION_STATEMENT_LABEL_LENGTH + 1);
        layout->name = strchr(layout->name, ':') + 1; /* step 8 */
        /* If the target app contains an annotation whose argument is a function call
         * that gets inlined, and that function contains the same annotation, the
         * compiler will fuse the headers. See
         * https://dynamorio.org/page_annotations.html for a sample of
         * fused headers. This loop identifies and skips any fused headers.
         */
        while (true) {
            instr_reset(dcontext, scratch);
            cur_pc = decode(dcontext, cur_pc, scratch);
            if (IS_ANNOTATION_HEADER(scratch, cur_pc)) {
                cur_pc += INT_LENGTH;
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
        layout->name += (ANNOTATION_EXPRESSION_LABEL_LENGTH + 1);
        layout->is_void = IS_ANNOTATION_VOID(layout->name); /* step 7 */
        layout->name = strchr(layout->name, ':') + 1;       /* step 8 */
    }
}
#    else /* Windows x86 and all Unix  */
/* Identify the annotation at layout->start_pc, if any. In summary:
 *   (step 1) check if the instruction at layout->start_pc encodes an annotation label.
 *   (step 2) determine the annotation type based on instruction opcodes
 *     Expression annotations only:
 *       (step 3) compare the return type to determine whether the annotation is void.
 *   (step 4) adjust the substitution xl8 to the current pc.
 *   (step 5) decode past the jump over the annotation body.
 *   (step 6) set the resume pc to the current instruction, which is a jump over the
 *            native version of the annotation body (specified by the target app).
 *   (step 7) advance the label pointer to the name token.
 */
static inline void
identify_annotation(dcontext_t *dcontext, IN OUT annotation_layout_t *layout,
                    instr_t *scratch)
{
    app_pc cur_pc = layout->start_pc;
    if (is_annotation_tag(dcontext, &cur_pc, scratch, &layout->name)) { /* step 1 */
#        ifdef WINDOWS
        if (*(cur_pc++) == RAW_OPCODE_pop_eax) {                        /* step 2 */
#        else
        if (instr_get_opcode(scratch) == OP_bsf) { /* step 2 */
#        endif
            layout->type = ANNOTATION_TYPE_STATEMENT;
        } else {
            layout->type = ANNOTATION_TYPE_EXPRESSION;
            layout->is_void = IS_ANNOTATION_VOID(layout->name); /* step 3 */
        }
        layout->substitution_xl8 = cur_pc; /* step 4 */
        instr_reset(dcontext, scratch);
        cur_pc = decode_cti(dcontext, cur_pc, scratch); /* step 5 */
        ASSERT(instr_is_ubr(scratch));
        layout->resume_pc = cur_pc;                   /* step 6 */
        layout->name = strchr(layout->name, ':') + 1; /* step 7 */
    }
}
#    endif

#    ifdef X64
#        ifdef UNIX
static inline void /* UNIX x64 */
create_arg_opnds(dr_annotation_handler_t *handler, uint num_args,
                 dr_annotation_calling_convention_t call_type)
{
    uint i, arg_stack_location;
    ASSERT(call_type == DR_ANNOTATION_CALL_TYPE_FASTCALL); /* architecture constraint */
    switch (num_args) { /* Create up to six register args */
    default:
    case 6: handler->args[5] = opnd_create_reg(DR_REG_R9);
    case 5: handler->args[4] = opnd_create_reg(DR_REG_R8);
    case 4: handler->args[3] = opnd_create_reg(DR_REG_XCX);
    case 3: handler->args[2] = opnd_create_reg(DR_REG_XDX);
    case 2: handler->args[1] = opnd_create_reg(DR_REG_XSI);
    case 1: handler->args[0] = opnd_create_reg(DR_REG_XDI);
    }
    /* Create the remaining args on the stack */
    for (i = FASTCALL_REGISTER_ARG_COUNT; i < num_args; i++) {
        /* The clean call will appear at the top of the annotation function body, where
         * the stack arguments follow the return address and the caller's saved stack
         * pointer. Rewind `i` to 0 and add 2 to skip over these pointers.
         */
        arg_stack_location = sizeof(ptr_uint_t) * (i - FASTCALL_REGISTER_ARG_COUNT + 2);
        /* Use the stack pointer because the base pointer is a general register in x64 */
        handler->args[i] = OPND_CREATE_MEMPTR(DR_REG_XSP, arg_stack_location);
    }
}
#        else /* WINDOWS x64 */
static inline void
create_arg_opnds(dr_annotation_handler_t *handler, uint num_args,
                 dr_annotation_calling_convention_t call_type)
{
    uint i, arg_stack_location;
    ASSERT(call_type == DR_ANNOTATION_CALL_TYPE_FASTCALL); /* architecture constraint */
    switch (num_args) { /* Create up to four register args */
    default:
    case 4: handler->args[3] = opnd_create_reg(DR_REG_R9);
    case 3: handler->args[2] = opnd_create_reg(DR_REG_R8);
    case 2: handler->args[1] = opnd_create_reg(DR_REG_XDX);
    case 1: handler->args[0] = opnd_create_reg(DR_REG_XCX);
    }
    /* Create the remaining args on the stack */
    for (i = FASTCALL_REGISTER_ARG_COUNT; i < num_args; i++) {
        /* The clean call will appear at the top of the annotation function body, where
         * the stack arguments follow the return address and 32 bytes of empty space.
         * Since `i` is already starting at 4, just add one more to reach the args.
         */
        arg_stack_location = sizeof(ptr_uint_t) * (i + 1);
        /* Use the stack pointer because the base pointer is a general register in x64 */
        handler->args[i] = OPND_CREATE_MEMPTR(DR_REG_XSP, arg_stack_location);
    }
}
#        endif
#    else /* x86 (all) */
static inline void
create_arg_opnds(dr_annotation_handler_t *handler, uint num_args,
                 dr_annotation_calling_convention_t call_type)
{
    uint i, arg_stack_location;
    if (call_type == DR_ANNOTATION_CALL_TYPE_FASTCALL) {
        switch (num_args) { /* Create 1 or 2 register args */
        default:
        case 2: handler->args[1] = opnd_create_reg(DR_REG_XDX);
        case 1: handler->args[0] = opnd_create_reg(DR_REG_XCX);
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
             * pointer. Since `i` starts at 0, add 2 to skip those pointers.
             */
            arg_stack_location = sizeof(ptr_uint_t) * (i + 2);
            handler->args[i] = OPND_CREATE_MEMPTR(DR_REG_XBP, arg_stack_location);
        }
    }
}
#    endif

#    ifdef DEBUG
static ssize_t
annotation_printf(const char *format, ...)
{
    va_list ap;
    ssize_t count;
    const char *timestamp_token_start;
    char *timestamped_format = NULL;
    uint buffer_length = 0;

    if (d_r_stats == NULL || d_r_stats->loglevel == 0)
        return 0; /* No log is available for writing. */
    if ((d_r_stats->logmask & LOG_VIA_ANNOTATIONS) == 0)
        return 0; /* Filtered out by the user. */

    /* Substitute the first instance of the timestamp token with a timestamp string.
     * Additional timestamp tokens will be ignored, because it would be pointless.
     */
    timestamp_token_start = strstr(format, LOG_ANNOTATION_TIMESTAMP_TOKEN);
    if (timestamp_token_start != NULL) {
        char timestamp_buffer[PRINT_TIMESTAMP_MAX_LENGTH];
        buffer_length = (uint)(strlen(format) + PRINT_TIMESTAMP_MAX_LENGTH);
        if (print_timestamp_to_buffer(timestamp_buffer, PRINT_TIMESTAMP_MAX_LENGTH) > 0) {
            uint length_before_token =
                (uint)((ptr_uint_t)timestamp_token_start - (ptr_uint_t)format);
            /* print the timestamped format string into this heap buffer */
            timestamped_format = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, char, buffer_length,
                                                  ACCT_OTHER, UNPROTECTED);

            /* copy the original format string up to the timestamp token */
            d_r_snprintf(timestamped_format, length_before_token, "%s", format);
            /* copy the timestamp and the remainder of the original format string */
            d_r_snprintf(timestamped_format + length_before_token,
                         (buffer_length - length_before_token), "%s%s", timestamp_buffer,
                         timestamp_token_start + LOG_ANNOTATION_TIMESTAMP_TOKEN_LENGTH);
            /* use the timestamped format string */
            format = (const char *)timestamped_format;
        } else {
            LOG(GLOBAL, LOG_ANNOTATIONS, 2,
                "Failed to obtain a system timestamp for "
                "substitution in annotation log statements '%s'\n",
                format);
        }
    }

    va_start(ap, format);
    count = do_file_write(GLOBAL, format, ap);
    va_end(ap);
    if (timestamped_format != NULL) { /* free the timestamp heap buffer, if any */
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, format, char, buffer_length, ACCT_OTHER,
                        UNPROTECTED);
    }
    return count;
}
#    endif

static void
free_annotation_handler(void *p)
{
    dr_annotation_handler_t *handler = (dr_annotation_handler_t *)p;
    dr_annotation_receiver_t *next, *receiver = handler->receiver_list;
    while (receiver != NULL) {
        next = receiver->next;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, receiver, dr_annotation_receiver_t, ACCT_OTHER,
                       UNPROTECTED);
        receiver = next;
    }
    if (handler->num_args > 0) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, handler->args, opnd_t, handler->num_args,
                        ACCT_OTHER, UNPROTECTED);
    }
    if ((handler->type == DR_ANNOTATION_HANDLER_CALL) ||
        (handler->type == DR_ANNOTATION_HANDLER_RETURN_VALUE))
        dr_strfree(handler->symbol_name HEAPACCT(ACCT_OTHER));
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, p, dr_annotation_handler_t, ACCT_OTHER, UNPROTECTED);
}

#endif /* ANNOTATIONS */
