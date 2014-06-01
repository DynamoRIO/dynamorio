#include "../globals.h"
#include "../hashtable.h"
#include "../x86/instr.h"
#include "../x86/instr_create.h"
#include "../x86/instrument.h"
#include "../x86/disassemble.h"
#include "annot.h"

#include "../lib/annotation/valgrind.h"
#include "../lib/annotation/memcheck.h"

#define PRINT_SYMBOL_NAME(dst, dst_size, src, num_args) \
    dr_snprintf(dst, dst_size, "@%s@%d", src, sizeof(ptr_uint_t) * num_args);

#define KEY(addr) ((ptr_uint_t) addr)
static generic_table_t *handlers;

// locked under the `handlers` table lock
static annotation_handler_t *vg_handlers[VG_ID__LAST];

static annotation_handler_t vg_router;
static opnd_t vg_router_arg;

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

/**** Private Function Declarations ****/

/* Handles a valgrind client request, if we understand it.
 */
static void
handle_vg_annotation(app_pc request_args);

static valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request);

static void
specify_args(annotation_handler_t *handler, uint num_args
    _IF_NOT_X64(annotation_calling_convention_t type));

static void
free_annotation_handler(void *p);

/**** Public Function Definitions ****/

void
annot_init()
{
    handlers = generic_hash_create(GLOBAL_DCONTEXT, 8, 80,
                                   HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                   HASHTABLE_RELAX_CLUSTER_CHECKS,
                                   free_annotation_handler
                                   _IF_DEBUG("annotation hashtable"));

    vg_router.type = ANNOT_HANDLER_CALL;
    vg_router.instrumentation.callback = (void *) handle_vg_annotation;
    vg_router.num_args = 1;
    vg_router_arg = opnd_create_reg(DR_REG_XAX);
    vg_router.args = &vg_router_arg;
    vg_router.arg_stack_space = 0;
    vg_router.save_fpstate = false;
    vg_router.id.annotation_func = NULL; // identified by magic code sequence
    vg_router.next_handler = NULL;
}

void
annot_exit()
{
    uint i;

    for (i = 0; i < VG_ID__LAST; i++) {
        if (vg_handlers[i] != NULL)
            HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, vg_handlers[i], annotation_handler_t,
                            1, ACCT_OTHER, UNPROTECTED);
    }

    generic_hash_destroy(GLOBAL_DCONTEXT, handlers);
}

void
dr_annot_register_call_varg(void *drcontext, void *annotation_func,
                         void *callback, bool save_fpstate, uint num_args, ...)
{
    annotation_handler_t *handler;
    TABLE_RWLOCK(handlers, write, lock);
    handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           KEY(annotation_func));
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        handler->type = ANNOT_HANDLER_CALL;
        handler->id.annotation_func = (app_pc) annotation_func;
        handler->instrumentation.callback = callback;
        handler->save_fpstate = save_fpstate;
        handler->num_args = num_args;
        handler->next_handler = NULL;
        handler->arg_stack_space = 0;

        if (num_args == 0) {
            handler->args = NULL;
        } else {
            uint i;
            va_list args;
            va_start(args, num_args);
            handler->args = HEAP_ARRAY_ALLOC(drcontext,
                opnd_t, num_args, ACCT_OTHER, UNPROTECTED);
            for (i = 0; i < num_args; i++) {
                handler->args[i] = va_arg(args, opnd_t);
                CLIENT_ASSERT(opnd_is_valid(handler->args[i]),
                              "Call argument: bad operand. Did you create a valid opnd_t?");
#ifndef x64
                if (IS_ANNOTATION_STACK_ARG(handler->args[i]))
                    handler->arg_stack_space += sizeof(ptr_uint_t);
#endif
            }
            va_end(args);
        }

        generic_hash_add(GLOBAL_DCONTEXT, handlers, KEY(annotation_func), handler);
    } // else ignore duplicate registration
    TABLE_RWLOCK(handlers, write, unlock);
}

bool
dr_annot_find_and_register_call(void *drcontext, const module_data_t *module,
                             const char *target_name,
                             void *callback, uint num_args
                             _IF_NOT_X64(annotation_calling_convention_t type))
{
    generic_func_t target;

#if defined(UNIX) || defined(X64)
    char *symbol_name = (char *) target_name;
#else
    char symbol_name[256];
    PRINT_SYMBOL_NAME(symbol_name, 256, target_name, num_args);
#endif
    target = dr_get_proc_address(module->handle, symbol_name);
    if (target != NULL) {
        dr_annot_register_call(drcontext, (void *) target, callback, false,
            num_args _IF_NOT_X64(type));
        return true;
    } else {
        return false;
    }
}

void
dr_annot_register_call(void *drcontext, void *annotation_func, void *callback,
                    bool save_fpstate, uint num_args
                    _IF_NOT_X64(annotation_calling_convention_t type))
{
    annotation_handler_t *handler;
    TABLE_RWLOCK(handlers, write, lock);
    handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                          KEY(annotation_func));
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        handler->type = ANNOT_HANDLER_CALL;
        handler->id.annotation_func = (app_pc) annotation_func;
        handler->instrumentation.callback = callback;
        handler->save_fpstate = save_fpstate;
        handler->num_args = num_args;
        handler->next_handler = NULL;
        handler->arg_stack_space = 0;

        if (num_args == 0) {
            handler->args = NULL;
        } else {
            handler->args = HEAP_ARRAY_ALLOC(drcontext,
                opnd_t, num_args, ACCT_OTHER, UNPROTECTED);
            specify_args(handler, num_args _IF_NOT_X64(type));
        }

        generic_hash_add(GLOBAL_DCONTEXT, handlers, KEY(annotation_func), handler);
    } // else ignore duplicate registration
    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_register_return(void *drcontext, void *annotation_func, void *return_value)
{
    annotation_handler_t *handler;
    TABLE_RWLOCK(handlers, write, lock);
    handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           KEY(annotation_func));
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(annotation_handler_t));
        handler->type = ANNOT_HANDLER_RETURN_VALUE;
        handler->id.annotation_func = (app_pc) annotation_func;
        handler->instrumentation.return_value = return_value;
        handler->save_fpstate = false;
        handler->num_args = 0;
        handler->next_handler = NULL;
        handler->arg_stack_space = 0;

        generic_hash_add(GLOBAL_DCONTEXT, handlers, KEY(annotation_func), handler);
    } // else ignore duplicate registration
    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_register_valgrind(valgrind_request_id_t request_id,
    ptr_uint_t (*annotation_callback)(vg_client_request_t *request))
{
    annotation_handler_t *handler;
    if (request_id >= VG_ID__LAST)
        return;

    TABLE_RWLOCK(handlers, write, lock);
    handler = vg_handlers[request_id];
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        handler->type = ANNOT_HANDLER_VALGRIND;
        handler->id.vg_request_id = request_id;
        handler->instrumentation.vg_callback = annotation_callback;
        handler->save_fpstate = false;
        handler->num_args = 0;
        handler->next_handler = NULL;

        vg_handlers[request_id] = handler;
    }
    TABLE_RWLOCK(handlers, write, unlock);
}

instr_t *
annot_match(dcontext_t *dcontext, instr_t *instr)
{
    instr_t *first_call = NULL, *last_added_instr = NULL;

    if (instr_is_call_direct(instr) || instr_is_ubr(instr)) { // ubr: tail call `gcc -O3`
        app_pc target = instr_get_branch_target_pc(instr);
        annotation_handler_t *handler = NULL;

        TABLE_RWLOCK(handlers, read, lock);
        handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                               (ptr_uint_t)target);
        if (handler != NULL) {
            while (true) {
                instr_t *call = INSTR_CREATE_label(dcontext);
                dr_instr_label_data_t *label_data = instr_get_label_data_area(call);

                instr_set_note(call, (void *) DR_NOTE_ANNOTATION);
                label_data->data[0] = (ptr_uint_t) handler;
                label_data->data[1] = instr_is_ubr(instr) ?
                    ANNOT_TAIL_CALL : ANNOT_NORMAL_CALL;
                label_data->data[2] = (ptr_uint_t) instr_get_translation(instr);
                instr_set_ok_to_mangle(call, false);

                if (first_call == NULL) {
                    first_call =  call;
                } else {
                    instr_set_next(last_added_instr, call);
                    instr_set_prev(call, last_added_instr);
                }
                last_added_instr = call;

                if (handler->next_handler == NULL) {
                    if (handler->arg_stack_space > 0) {
                        instr_t *stack_scrub = INSTR_CREATE_lea(dcontext,
                            opnd_create_reg(REG_XSP),
                            opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                                  handler->arg_stack_space, OPSZ_0));
                        instr_set_ok_to_mangle(stack_scrub, false);
                        instr_set_next(call, stack_scrub);
                        instr_set_prev(stack_scrub, call);
                        last_added_instr = stack_scrub;
                    }
                    break;
                }
                handler = handler->next_handler;
            }

            if (instr_is_ubr(instr)) {
                instr_t *tail_call_return = INSTR_CREATE_ret(dcontext);
                instr_set_ok_to_mangle(tail_call_return, false);
                instr_set_next(last_added_instr, tail_call_return);
                instr_set_prev(tail_call_return, last_added_instr);
            }
        }

        TABLE_RWLOCK(handlers, read, unlock);
    }

    return first_call;
}

bool
match_valgrind_pattern(dcontext_t *dcontext, instrlist_t *bb, instr_t *instr,
                       app_pc xchg_pc, uint bb_instr_count)
{
    int i;
    app_pc xchg_xl8;
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
    xchg_xl8 = instr_get_app_pc(instr);
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

    /* Append a write to %xbx, both to ensure it's marked defined by DrMem
     * and to avoid confusion with register analysis code (%xbx is written
     * by the clean callee).
     */
    instrlist_append(bb, INSTR_XL8(INSTR_CREATE_xor(dcontext, opnd_create_reg(DR_REG_XBX),
                                                    opnd_create_reg(DR_REG_XBX)),
                                   xchg_xl8));

    instr = INSTR_CREATE_label(dcontext);
    instr_set_note(instr, (void *) DR_NOTE_ANNOTATION);
    label_data = instr_get_label_data_area(instr);
    label_data->data[0] = (ptr_uint_t) &vg_router;
    label_data->data[1] = ANNOT_NORMAL_CALL;
    label_data->data[2] = (ptr_uint_t) xchg_pc;
    instr_set_ok_to_mangle(instr, false);
    instrlist_append(bb, instr);

    return true;
}

void
annot_event_module_load(dcontext_t *dcontext, const module_data_t *data,
                        bool already_loaded)
{
    generic_func_t target;

#if defined(UNIX) || defined(X64)
    const char *symbol_name = "dynamorio_annotate_running_on_dynamorio";
#else
    char symbol_name[256];
    PRINT_SYMBOL_NAME(symbol_name, 256, "dynamorio_annotate_running_on_dynamorio", 0);
#endif
    target = dr_get_proc_address(data->handle, symbol_name);
    if (target != NULL)
        dr_annot_register_return(dcontext, (void *) target, (void *) (ptr_uint_t) true);
}

void
annot_event_module_unload(dcontext_t *dcontext, const module_data_t *data)
{
    int iter = 0;
    ptr_uint_t key;
    void *handler;
    TABLE_RWLOCK(handlers, write, lock);
    do {
        iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, handlers,
                                         iter, &key, &handler);
        if (iter < 0)
            break;
        if ((key > (ptr_uint_t) data->start) && (key < (ptr_uint_t) data->end))
            iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, handlers,
                                               iter, key);
    } while (true);
    TABLE_RWLOCK(handlers, write, unlock);
}


/**** Private Function Definitions ****/

static void
handle_vg_annotation(app_pc request_args)
{
    dcontext_t *dcontext = (dcontext_t *) dr_get_current_drcontext();
    valgrind_request_id_t request_id;
    annotation_handler_t *handler;
    vg_client_request_t request;
    dr_mcontext_t mcontext;
    ptr_uint_t result;

    if (!safe_read(request_args, sizeof(vg_client_request_t), &request))
        return;

    result = request.default_result;

    request_id = lookup_valgrind_request(request.request);

    // dr_printf("Core handles Valgrind annotation %d.\n", request_id);

    if (request_id < VG_ID__LAST) {
        TABLE_RWLOCK(handlers, read, lock);
        handler = vg_handlers[request_id];
        if (handler != NULL) // TODO: multiple handlers? Then what result?
            result = handler->instrumentation.vg_callback(&request);
        //else
            //dr_printf("Valgrind handler returns default result 0x%x\n", result);
        TABLE_RWLOCK(handlers, read, unlock);
    }

    /* The result code goes in xdx. */
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_INTEGER;
    dr_get_mcontext(dcontext, &mcontext);
    mcontext.xdx = result;
    dr_set_mcontext(dcontext, &mcontext);
}

static valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request)
{
    switch (request) {
        case VG_USERREQ__RUNNING_ON_VALGRIND:
            return VG_ID__RUNNING_ON_VALGRIND;
        case VG_USERREQ__MAKE_MEM_DEFINED_IF_ADDRESSABLE:
            return VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE;
    }

    return VG_ID__LAST;
}

#ifdef X64
# ifdef UNIX
static inline void // UNIX x64
specify_args(annotation_handler_t *handler, uint num_args)
{
    uint i;
    for (i = 6; i < num_args; i++) {
        handler->args[i] = OPND_CREATE_MEMPTR(
            DR_REG_XSP, sizeof(ptr_uint_t) * (i-6));
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
            DR_REG_XSP, sizeof(ptr_uint_t) * i);
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
                       annotation_calling_convention_t type)
{
    uint i;
    if (type == ANNOT_FASTCALL) {
        for (i = 2; i < num_args; i++) {
            handler->args[i] = OPND_CREATE_MEMPTR(
                DR_REG_XSP, sizeof(ptr_uint_t) * (i-2));
        }
        switch (num_args) {
            default:
                handler->arg_stack_space = (sizeof(ptr_uint_t) * (num_args - 2));
            case 2:
                handler->args[1] = opnd_create_reg(DR_REG_XDX);
            case 1:
                handler->args[0] = opnd_create_reg(DR_REG_XCX);
        }
    } else { // ANNOT_STDCALL
        for (i = 0; i < num_args; i++) {
            handler->args[i] = OPND_CREATE_MEMPTR(
                DR_REG_XSP, sizeof(ptr_uint_t) * i);
        }
        handler->arg_stack_space = (sizeof(ptr_uint_t) * num_args);
    }
}
#endif

static void
free_annotation_handler(void *p)
{
    annotation_handler_t *handler = (annotation_handler_t *) p;
    if (handler->num_args > 0) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, handler->args, opnd_t, handler->num_args,
                        ACCT_OTHER, UNPROTECTED);
    }
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, p, annotation_handler_t,
                    1, ACCT_OTHER, UNPROTECTED);
}
