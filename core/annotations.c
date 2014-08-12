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
#include "annotations.h"

#ifdef ANNOTATIONS /* around whole file */

#ifdef UNIX
# include <string.h>
#endif

#if !(defined (WINDOWS) && defined (X64))
# include "../third_party/valgrind/valgrind.h"
# include "../third_party/valgrind/memcheck.h"
#endif

static strhash_table_t *handlers;

/* locked under the `handlers` table lock */
static dr_annotation_handler_t *vg_handlers[DR_VG_ID__LAST];

/*********************************************************
 * INTERNAL ROUTINE DECLARATIONS
 */

/* Create argument operands for the instrumented clean call */
static void
create_arg_opnds(dr_annotation_handler_t *handler, uint num_args,
                 dr_annotation_calling_convention_t call_type);

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
annotation_match(dcontext_t *dcontext, app_pc *start_pc, instr_t **substitution
                 _IF_WINDOWS_X64(bool hint_is_safe))
{
    /* deliberately empty */
    return false;
}

void
instrument_valgrind_annotation(dcontext_t *dcontext, instrlist_t *bb, instr_t *instr,
                               app_pc xchg_pc, uint bb_instr_count)
{
    /* deliberately empty */
}

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
        handler->id.symbol_name = dr_strdup(annotation_name, ACCT_OTHER);
        handler->num_args = num_args;

        if (num_args == 0) {
            handler->args = NULL;
        } else {
            handler->args = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, opnd_t, num_args,
                                             ACCT_OTHER, UNPROTECTED);
            create_arg_opnds(handler, num_args, call_type);
        }

        strhash_hash_add(GLOBAL_DCONTEXT, handlers, handler->id.symbol_name, handler);
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
        handler->id.vg_request_id = request_id;

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
        handler->id.symbol_name = dr_strdup(annotation_name, ACCT_OTHER);
        strhash_hash_add(GLOBAL_DCONTEXT, handlers, handler->id.symbol_name, handler);
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
 * INTERNAL ROUTINES
 */

#ifdef X64
# ifdef UNIX
#  define FASTCALL_ARG_COUNT 6
# else /* WINDOWS x64 */
#  define FASTCALL_ARG_COUNT 4
# endif
#else /* x86 (all) */
# define FASTCALL_ARG_COUNT 2
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
    for (i = FASTCALL_ARG_COUNT; i < num_args; i++) {
        /* The clean call will appear at the top of the annotation function body, where
         * the stack arguments follow the return address and the caller's saved stack
         * pointer. Rewind `i` to 0 and add 2 to skip over these pointers.
         */
        arg_stack_location = sizeof(ptr_uint_t) * (i-FASTCALL_ARG_COUNT+2);
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
    for (i = FASTCALL_ARG_COUNT; i < num_args; i++) {
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
        for (i = FASTCALL_ARG_COUNT; i < num_args; i++) {
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
        dr_strfree(handler->id.symbol_name, ACCT_OTHER);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, p, dr_annotation_handler_t, ACCT_OTHER, UNPROTECTED);
}

#endif /* ANNOTATIONS */
