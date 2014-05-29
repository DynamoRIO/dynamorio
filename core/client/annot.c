#include "../globals.h"
#include "../hashtable.h"
#include "../x86/instr.h"
#include "../x86/instr_create.h"
#include "../x86/instrument.h"
#include "../x86/disassemble.h"
#include "annot.h"

#include "../lib/annotation/valgrind.h"
#include "../lib/annotation/memcheck.h"

#ifdef CLIENT_INTERFACE

#define PRINT_SYMBOL_NAME(dst, dst_size, src, num_args) \
    dr_snprintf(dst, dst_size, "@%s@%d", src, sizeof(ptr_uint_t) * num_args);

#define KEY(addr) ((ptr_uint_t) addr)
static generic_table_t *handlers;

// locked under the `handlers` table lock
static annotation_handler_t *vg_handlers[VG_ID__LAST];

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

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded);

static void
event_module_unload(void *drcontext, const module_data_t *info);

static void
convert_va_list_to_opnd(opnd_t *args, uint num_args, va_list ap);

static valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request);

static void
specify_args(annotation_handler_t *handler, uint num_args
    _IF_NOT_X64(annotation_call_type_t type));

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

    dr_register_module_load_event(event_module_load);
    dr_register_module_unload_event(event_module_unload);
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
annot_register_call_varg(void *drcontext, void *annotation_func,
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
                if (IS_ANNOTATION_STACK_ARG(handler->args[i]))
                    handler->arg_stack_space += sizeof(ptr_uint_t);
            }
            va_end(args);
        }

        generic_hash_add(GLOBAL_DCONTEXT, handlers, KEY(annotation_func), handler);
    } // else ignore duplicate registration
    TABLE_RWLOCK(handlers, write, unlock);
}

bool
annot_find_and_register_call(void *drcontext, const module_data_t *module,
                             const char *target_name,
                             void *callback, uint num_args
                             _IF_NOT_X64(annotation_call_type_t type))
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
        annot_register_call(drcontext, (void *) target, callback, false,
            num_args _IF_NOT_X64(type));
        return true;
    } else {
        return false;
    }
}

void
annot_register_call(void *drcontext, void *annotation_func, void *callback,
                    bool save_fpstate, uint num_args
                    _IF_NOT_X64(annotation_call_type_t type))
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
annot_register_return(void *drcontext, void *annotation_func, void *return_value)
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
annot_register_valgrind(valgrind_request_id_t request_id,
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
    instr_t *first_call = NULL, *prev_call = NULL;

    if (instr_is_call_direct(instr) || instr_is_ubr(instr)) { // ubr: tail call `gcc -O3`
        app_pc target = instr_get_branch_target_pc(instr);
        annotation_handler_t *handler = NULL;

        TABLE_RWLOCK(handlers, read, lock);
        handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                               (ptr_uint_t)target);
        if (handler != NULL) {
            while (true) {
                instr_t *call = INSTR_CREATE_label(dcontext);

                call->flags |= INSTR_ANNOTATION;
                if (instr_is_ubr(instr))
                    call->flags |= INSTR_ANNOTATION_TAIL_CALL;
                instr_set_note(call, (void *) handler); // Collision with other notes?
                instr_set_ok_to_mangle(call, false);

                if (first_call == NULL) {
                    first_call = prev_call = call;
                } else {
                    instr_set_next(prev_call, call);
                    instr_set_prev(call, prev_call);
                    prev_call = call;
                }

                if (handler->next_handler == NULL) {
                    if (handler->arg_stack_space > 0) {
                        instr_t *stack_scrub = INSTR_CREATE_lea(dcontext,
                            opnd_create_reg(REG_XSP),
                            opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                                  handler->arg_stack_space, OPSZ_0));
                        instr_set_ok_to_mangle(stack_scrub, false);
                        instr_set_next(call, stack_scrub);
                        instr_set_prev(stack_scrub, call);
                    }
                    break;
                }
                handler = handler->next_handler;
            }
        }

        TABLE_RWLOCK(handlers, read, unlock);
    }

    return first_call;
}

bool
match_valgrind_pattern(dcontext_t *dcontext, instrlist_t *bb, instr_t *instr)
{
    int i;
    app_pc xchg_xl8;
    instr_t *instr_walk;

    /* Already know that `instr` is `OP_xchg`, per `IS_VALGRIND_ANNOTATION_SHAPE`.
     * Check the operands of the xchg for the Valgrind signature: both xbx. */
    opnd_t src = instr_get_src(instr, 0);
    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t xbx = opnd_create_reg(DR_REG_XBX);
    if (!opnd_same(src, xbx) || !opnd_same(dst, xbx))
        return false;

    /* If it's a Valgrind annotation, the preceding `VALGRIND_ANNOTATION_ROL_COUNT`
     * instructions will be `OP_rol` having `expected_rol_immeds` operands. */
    instr_walk = instrlist_last(bb);
    for (i = (VALGRIND_ANNOTATION_ROL_COUNT - 1); i >= 0; i--) {
        if (instr_get_opcode(instr_walk) != OP_rol) {
            return false;
        } else {
            opnd_t src = instr_get_src(instr_walk, 0);
            opnd_t dst = instr_get_dst(instr_walk, 0);
            if (!opnd_is_immed(src) ||
                opnd_get_immed_int(src) != expected_rol_immeds[i])
                return false;
            if (!opnd_same(dst, opnd_create_reg(DR_REG_XDI)))
                return false;
        }
        instr_walk = instr_get_prev(instr_walk);
    }

    /* We have matched the pattern. */
    DOLOG(4, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "Matched valgrind client request pattern at "PFX":\n",
            instr_get_app_pc(instr));
        /*
        for (i = 0; i < BUFFER_SIZE_ELEMENTS(instrs); i++) {
            instr_disassemble(dcontext, instr, THREAD);
            LOG(THREAD, LOG_INTERP, 4, "\n");
        }
        */
        LOG(THREAD, LOG_INTERP, 4, "\n");
    });

    /* We leave the argument gathering code (typically "lea _zzq_args -> %xax"
     * and "mov _zzq_default -> %xdx") as app instructions, as it writes to app
     * registers (xref i#1423).
     */
    xchg_xl8 = instr_get_app_pc(instr);
    instr_destroy(dcontext, instr);

    /* Delete rol and xchg instructions. Note: reusing parameter `instr`. */
    instr = instrlist_last(bb);
    for (i = 0; i < VALGRIND_ANNOTATION_ROL_COUNT; i++) {
        instr_walk = instr_get_prev(instr);
        instrlist_remove(bb, instr);
        instr_destroy(dcontext, instr);
        instr = instr_walk;
    }

    // TODO: check request id, and ignore if we don't support that one?

    /* Append a write to %xbx, both to ensure it's marked defined by DrMem
     * and to avoid confusion with register analysis code (%xbx is written
     * by the clean callee).
     */
    instrlist_append(bb, INSTR_XL8(INSTR_CREATE_xor(dcontext, opnd_create_reg(DR_REG_XBX),
                                                    opnd_create_reg(DR_REG_XBX)),
                                   xchg_xl8));

    dr_insert_clean_call(dcontext, bb, NULL, (void*)handle_vg_annotation,
                         /*fpstate=*/false, 1, opnd_create_reg(DR_REG_XAX));

    return true;
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

    if (!safe_read(request_args, sizeof(request), &request))
        return;

    result = request.default_result;

    request_id = lookup_valgrind_request(request.request);
    if (request_id < VG_ID__LAST) {
        TABLE_RWLOCK(handlers, read, lock);
        handler = vg_handlers[request_id];
        if (handler != NULL) // TODO: multiple handlers? Then what result?
            result = handler->instrumentation.vg_callback(&request);
        TABLE_RWLOCK(handlers, read, unlock);
    }

    /* The result code goes in xbx. */
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_INTEGER;
    dr_get_mcontext(dcontext, &mcontext);
    mcontext.xbx = result;
    dr_set_mcontext(dcontext, &mcontext);
}

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    generic_func_t target;
#if defined(UNIX) || defined(X64)
    const char *symbol_name = "dynamorio_annotate_running_on_dynamorio";
#else
    char symbol_name[256];
    PRINT_SYMBOL_NAME(symbol_name, 256, "dynamorio_annotate_running_on_dynamorio", 0);
#endif
    target = dr_get_proc_address(info->handle, symbol_name);
    if (target != NULL)
        annot_register_return(drcontext, (void *) target, (void *) (ptr_uint_t) true);
}

static void
event_module_unload(void *drcontext, const module_data_t *info)
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
        if ((key > (ptr_uint_t) info->start) && (key < (ptr_uint_t) info->end))
            iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, handlers,
                                               iter, key);
    } while (true);
    TABLE_RWLOCK(handlers, write, unlock);
}

/* Stolen from instrument.c--should it become a utility? */
static void
convert_va_list_to_opnd(opnd_t *args, uint num_args, va_list ap)
{
    uint i;
    /* There's no way to check num_args vs actual args passed in */
    for (i = 0; i < num_args; i++) {
        args[i] = va_arg(ap, opnd_t);
        CLIENT_ASSERT(opnd_is_valid(args[i]),
                      "Call argument: bad operand. Did you create a valid opnd_t?");
    }
}

static valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request)
{
    switch (request) {
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
            handler->arg_stack_space = (sizeof(ptr_uint_t) * (num_args - 6));
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
#else // X86
static inline void
specify_args(annotation_handler_t *handler, uint num_args,
                       annotation_call_type_t type)
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

#endif /* CLIENT_INTERFACE */
