#include "../globals.h"
#include "../os_shared.h"
#include "../module_shared.h"
#include "../hashtable.h"
#include "../x86/instr.h"
#include "../x86/instr_create.h"
#include "../x86/disassemble.h"
#include "../x86/decode_fast.h"
#include "../lib/instrument.h"
#include "annotations.h"

#include "../third_party/valgrind/valgrind.h"
#include "../third_party/valgrind/memcheck.h"

#define MAX_ANNOTATION_INSTR_COUNT 100

#ifdef UNIX
# include <string.h>
#endif

#if defined(WINDOWS)
# define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_rel_addr(src)
# define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) opnd_get_addr(src)
#else
# define IS_ANNOTATION_LABEL_REFERENCE(opnd) opnd_is_base_disp(src)
// +4 for offset of the operand in the instruction
# define GET_ANNOTATION_LABEL_REFERENCE(src, instr_pc) ((app_pc) (opnd_get_disp(src) + instr_pc + 4))
#endif

typedef struct _annotation_registration_by_name_t {
    handler_type_t type;
    client_id_t client_id;
    const char *target_name;
    const char *symbol_name;
    union { // per type
        void *callback;
        void *return_value;
        ptr_uint_t (*vg_callback)(vg_client_request_t *request);
    } instrumentation;
    bool save_fpstate;
    uint num_args;
#ifndef X64
    annotation_calling_convention_t call_type;
#endif
    struct _annotation_registration_by_name_t *next;
} annotation_registration_by_name_t;

typedef struct _annotation_registration_by_name_list_t {
    uint size;
    annotation_registration_by_name_t *head;
} annotation_registration_by_name_list_t;

#define MATCH_REGISTRATION(by_name, client_id, target_name) \
    ((by_name != NULL) && (by_name->client_id == client_id) && \
    (strcmp(by_name->symbol_name, target_name) == 0))

#define REMOVE_REGISTRATION(link) \
do { \
    annotation_registration_by_name_t *removal = link; \
    link = link->next; \
    free_annotation_registration_by_name(removal); \
    by_name_list->size--; \
} while (0)

#if defined(WINDOWS) && !defined(X64)
# define PRINT_SYMBOL_NAME(dst, dst_size, src, num_args) \
    annot_vsnprintf(dst, dst_size, "@%s@%d", src, sizeof(ptr_uint_t) * num_args);
#endif

#define IS_HANDLER_NAME(h, name) \
    ((h->symbol_name != NULL) && strcmp(h->symbol_name, name) == 0)

#define KEY(annotation_id) ((ptr_uint_t) annotation_id)
static generic_table_t *handlers;

// locked under the `handlers` table lock
static annotation_handler_t *vg_handlers[VG_ID__LAST];

static annotation_handler_t vg_router;
static annotation_receiver_t vg_receiver;
static opnd_t vg_router_arg;

// locked under the `handlers` table lock
static annotation_registration_by_name_list_t *by_name_list;

extern uint dr_internal_client_id;

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

static annotation_handler_t *
annot_register_call(client_id_t client_id, void *annotation_func, void *callee,
                    bool save_fpstate, uint num_args
                    _IF_NOT_X64(annotation_calling_convention_t call_type));

static annotation_handler_t *
annot_register_return(void *annotation_func, void *return_value);

static void
annot_bind_registration(generic_func_t target, annotation_registration_by_name_t *by_name);

static void
handle_vg_annotation(app_pc request_args);

static valgrind_request_id_t
lookup_valgrind_request(ptr_uint_t request);

static inline bool
is_annotation_tag(dcontext_t *dcontext, app_pc start_pc, instr_t *scratch,
                  OUT const char **name);

static void
identify_annotation(dcontext_t *dcontext, instr_t *cti_instr, OUT const char **name,
                    OUT bool *is_expression, OUT app_pc *annotation_pc);

static void
specify_args(annotation_handler_t *handler, uint num_args
    _IF_NOT_X64(annotation_calling_convention_t call_type));

#if defined(WINDOWS) && !defined(X64)
static int
annot_vsnprintf(char *s, size_t max, const char *fmt, ...);
#endif

static void
free_annotation_registration_by_name(annotation_registration_by_name_t *by_name);

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
#ifdef WINDOWS
    module_iterator_t *mi;
    module_area_t *area;
#endif

    handlers = generic_hash_create(GLOBAL_DCONTEXT, 8, 80,
                                   HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                   HASHTABLE_RELAX_CLUSTER_CHECKS | HASHTABLE_PERSISTENT,
                                   free_annotation_handler
                                   _IF_DEBUG("annotation hashtable"));

    vg_router.type = ANNOT_HANDLER_CALL;
    vg_router.num_args = 1;
    vg_router_arg = opnd_create_reg(DR_REG_XAX);
    vg_router.args = &vg_router_arg;
    vg_router.arg_stack_space = 0;
    vg_router.id.annotation_func = NULL; // identified by magic code sequence
    vg_router.receiver_list = &vg_receiver;
    vg_receiver.client_id = dr_internal_client_id;
    vg_receiver.instrumentation.callback = (void *) handle_vg_annotation;
    vg_receiver.save_fpstate = false;
    vg_receiver.next = NULL;

    by_name_list = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT,
                                   annotation_registration_by_name_list_t,
                                   ACCT_OTHER, UNPROTECTED);
    memset(by_name_list, 0, sizeof(annotation_registration_by_name_list_t));

    dr_annot_register_return_by_name(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO_NAME,
                                     (void *) (ptr_uint_t) true);

#ifdef WINDOWS
    mi = module_iterator_start();
    while (module_iterator_hasnext(mi)) {
        area = module_iterator_next(mi);
        annot_module_load((module_handle_t) area->start);
    }
    module_iterator_stop(mi);
#endif
}

void
annot_exit()
{
    uint i;
    annotation_registration_by_name_t *next_by_name, *by_name = by_name_list->head;

    while (by_name != NULL) {
        next_by_name = by_name->next;
        free_annotation_registration_by_name(by_name);
        by_name = next_by_name;
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, by_name_list, annotation_registration_by_name_list_t,
                   ACCT_OTHER, UNPROTECTED);

    for (i = 0; i < VG_ID__LAST; i++) {
        if (vg_handlers[i] != NULL)
            free_annotation_handler(vg_handlers[i]);
    }

    generic_hash_destroy(GLOBAL_DCONTEXT, handlers);
}

void
dr_annot_register_call_by_name(client_id_t client_id, const char *target_name,
                               void *callee, bool save_fpstate, uint num_args
                               _IF_NOT_X64(annotation_calling_convention_t call_type))
{
    annotation_registration_by_name_t *by_name;

#if defined(UNIX) || defined(X64)
    char *symbol_name = (char *) target_name;
#else
    char symbol_name[256];
    PRINT_SYMBOL_NAME(symbol_name, 256, target_name, num_args);
#endif

    //dr_printf("Client %d registers for %s()\n", client_id, target_name);

    TABLE_RWLOCK(handlers, write, lock);

    by_name = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_registration_by_name_t,
                              ACCT_OTHER, UNPROTECTED);
    by_name->type = ANNOT_HANDLER_CALL;
    by_name->client_id = client_id;
    by_name->target_name = heap_strcpy(target_name);
    by_name->symbol_name = heap_strcpy(symbol_name);
    by_name->instrumentation.callback = callee;
    by_name->save_fpstate = save_fpstate;
    by_name->num_args = num_args;
#ifndef X64
    by_name->call_type = call_type;
#endif
    by_name->next = by_name_list->head;
    by_name_list->head = by_name;
    by_name_list->size++;

#ifdef WINDOWS
    /* Bind to all existing modules */
#endif

    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_register_call(client_id_t client_id, void *annotation_func, void *callee,
                       bool save_fpstate, uint num_args
                       _IF_NOT_X64(annotation_calling_convention_t call_type))
{
    TABLE_RWLOCK(handlers, write, lock);
    annot_register_call(client_id, annotation_func, callee, save_fpstate, num_args
                        _IF_NOT_X64(call_type));
    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_register_call_ex(client_id_t client_id, void *annotation_func,
                          void *callee, bool save_fpstate, uint num_args, ...)
{
    annotation_handler_t *handler;
    annotation_receiver_t *receiver;
    TABLE_RWLOCK(handlers, write, lock);
    handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           KEY(annotation_func));
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(annotation_handler_t));
        handler->type = ANNOT_HANDLER_CALL;
        handler->id.annotation_func = (app_pc) annotation_func;
        handler->num_args = num_args;

        if (num_args == 0) {
            handler->args = NULL;
        } else {
            uint i;
            va_list args;
            va_start(args, num_args);
            handler->args = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, opnd_t, num_args,
                                             ACCT_OTHER, UNPROTECTED);
            for (i = 0; i < num_args; i++) {
                handler->args[i] = va_arg(args, opnd_t);
                CLIENT_ASSERT(opnd_is_valid(handler->args[i]),
                              "Bad operand to annotation registration. "
                              "Did you create a valid opnd_t?");
#ifndef x64
                if (IS_ANNOTATION_STACK_ARG(handler->args[i]))
                    handler->arg_stack_space += sizeof(ptr_uint_t);
#endif
            }
            va_end(args);
        }

        generic_hash_add(GLOBAL_DCONTEXT, handlers, KEY(annotation_func), handler);
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
dr_annot_register_return(void *annotation_func, void *return_value)
{
    TABLE_RWLOCK(handlers, write, lock);
    annot_register_return(annotation_func, return_value);
    TABLE_RWLOCK(handlers, write, unlock);
}

// no client_id b/c there can only be one return value
// dangerous on unregister()?
void
dr_annot_register_return_by_name(const char *target_name, void *return_value)
{
    annotation_registration_by_name_t *by_name;

#if defined(UNIX) || defined(X64)
    char *symbol_name = (char *) target_name;
#else
    char symbol_name[256]; // TODO: alloc heap directly?
    PRINT_SYMBOL_NAME(symbol_name, 256, target_name, 0);
#endif

    TABLE_RWLOCK(handlers, write, lock);
    by_name = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_registration_by_name_t,
                              ACCT_OTHER, UNPROTECTED);
    by_name->type = ANNOT_HANDLER_RETURN_VALUE;
    by_name->client_id = 0;
    by_name->target_name = heap_strcpy(target_name);
    by_name->symbol_name = heap_strcpy(symbol_name);
    by_name->instrumentation.return_value = return_value;
    by_name->save_fpstate = false;
    by_name->num_args = 0;
#ifndef X64
    by_name->call_type = ANNOT_CALL_TYPE_NONE;
#endif
    by_name->next = by_name_list->head;
    by_name_list->head = by_name;
    by_name_list->size++;

#ifdef WINDOWS
    /* Bind to all existing modules */
#endif
    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_unregister_call_by_name(client_id_t client_id, const char *target_name)
{
    int iter = 0;
    ptr_uint_t key;
    annotation_handler_t *handler;
    annotation_registration_by_name_t *by_name;

    TABLE_RWLOCK(handlers, write, lock);
    by_name = by_name_list->head;
    do {
        iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, handlers,
                                         iter, &key, (void *) &handler);
        if (iter < 0)
            break;
        if (IS_HANDLER_NAME(handler, target_name)) {
            annotation_receiver_t *receiver = handler->receiver_list;
            if (receiver->client_id == client_id) {
                if (receiver->next == NULL) {
                    iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, handlers,
                                                       iter, key);
                } else {
                    handler->receiver_list = receiver->next;
                    // delete `receiver`
                }
            } else {
                while (receiver->next != NULL) {
                    if (receiver->next->client_id == client_id) {
                        annotation_receiver_t *removal = receiver->next;
                        receiver->next = removal->next;
                        // delete `removal`
                        break;
                    }
                    receiver = receiver->next;
                }
            }
        }
    } while (true);

    while (MATCH_REGISTRATION(by_name_list->head, client_id, target_name))
        REMOVE_REGISTRATION(by_name_list->head);
    by_name = by_name_list->head;
    while (by_name != NULL) {
        if (MATCH_REGISTRATION(by_name->next, client_id, target_name))
            REMOVE_REGISTRATION(by_name->next);
        else
            by_name = by_name->next;
    }

    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_unregister_call(client_id_t client_id, void *annotation_func)
{
    int iter = 0;
    annotation_handler_t *handler;

    TABLE_RWLOCK(handlers, write, lock);
    handler = generic_hash_lookup(GLOBAL_DCONTEXT, handlers, KEY(annotation_func));
    if (handler != NULL) {
        annotation_receiver_t *receiver = handler->receiver_list;
        if (receiver->client_id == client_id) {
            if (receiver->next == NULL) {
                iter = generic_hash_remove(GLOBAL_DCONTEXT, handlers,
                                           KEY(annotation_func));
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
        generic_hash_remove(GLOBAL_DCONTEXT, handlers, KEY(annotation_func));
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

void
dr_annot_unregister_return_by_name(const char *target_name)
{
    int iter = 0;
    ptr_uint_t key;
    annotation_handler_t *handler;

    TABLE_RWLOCK(handlers, write, lock);
    do {
        iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, handlers,
                                         iter, &key, (void *) &handler);
        if (iter < 0)
            break;
        if (IS_HANDLER_NAME(handler, target_name)) {
            iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, handlers,
                                               iter, key);
        }
    } while (true);
    TABLE_RWLOCK(handlers, write, unlock);
}

void
dr_annot_unregister_return(void *annotation_func)
{
    TABLE_RWLOCK(handlers, write, lock);
    generic_hash_remove(GLOBAL_DCONTEXT, handlers, KEY(annotation_func));
    TABLE_RWLOCK(handlers, write, unlock);
}

#ifdef WINDOWS
void
annot_module_load(module_handle_t base)
{
    annotation_registration_by_name_t *by_name;

    TABLE_RWLOCK(handlers, write, lock);
    by_name = by_name_list->head;
    while (by_name != NULL) {
        // addr = get_proc_address(base, by_name->symbol_name)
        // if (addr != NULL)
        //   annot_bind_registration(addr, by_name);
        by_name = by_name->next;
    }
    TABLE_RWLOCK(handlers, write, unlock);
}

void
annot_module_unload(module_handle_t base, size_t size)
{
    int iter = 0;
    ptr_uint_t key;
    void *handler;
    app_pc start = (app_pc) base;
    app_pc end = ((app_pc) base + size);

    TABLE_RWLOCK(handlers, write, lock);
    do {
        iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, handlers,
                                         iter, &key, &handler);
        if (iter < 0)
            break;
        if ((key > (ptr_uint_t) start) && (key < (ptr_uint_t) end))
            iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, handlers,
                                               iter, key);
    } while (true);
    TABLE_RWLOCK(handlers, write, unlock);
}
#endif

#if 0
instr_t *
annot_match(dcontext_t *dcontext, instr_t *instr)
{
    instr_t *first_call = NULL, *last_added_instr = NULL;

    if (instr_is_call_direct(instr) || instr_is_ubr(instr)) { // ubr: tail call `gcc -O3`
        app_pc target = instr_get_branch_target_pc(instr);
        annotation_handler_t *handler;

        TABLE_RWLOCK(handlers, read, lock);
        handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                               (ptr_uint_t)target);
        if (handler != NULL) {
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
#endif

instr_t *
annot_match(dcontext_t *dcontext, instr_t *cti_instr)
{
    const char *annotation_name;
    bool is_expression;
    app_pc annotation_pc;
    identify_annotation(dcontext, cti_instr, &annotation_name, &is_expression,
                        &annotation_pc);
    if (annotation_name != NULL) {
        dr_printf("Decoded %s invocation of %s at "PFX"\n",
                  is_expression ? "expression" : "statement",
                  annotation_name, annotation_pc);
    }

    return NULL;
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

    // if nobody is registered, return true here

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

/**** Private Function Definitions ****/

static annotation_handler_t *
annot_register_call(client_id_t client_id, void *annotation_func, void *callee,
                    bool save_fpstate, uint num_args
                    _IF_NOT_X64(annotation_calling_convention_t call_type))
{
    annotation_handler_t *handler;
    annotation_receiver_t *receiver;
    ASSERT_TABLE_SYNCHRONIZED(handlers, WRITE);
    handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           KEY(annotation_func));
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(annotation_handler_t));
        handler->type = ANNOT_HANDLER_CALL;
        handler->id.annotation_func = (app_pc) annotation_func;
        handler->num_args = num_args;

        if (num_args == 0) {
            handler->args = NULL;
        } else {
            handler->args = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT,
                opnd_t, num_args, ACCT_OTHER, UNPROTECTED);
            specify_args(handler, num_args _IF_NOT_X64(call_type));
        }

        generic_hash_add(GLOBAL_DCONTEXT, handlers, KEY(annotation_func), handler);
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
    receiver->client_id = client_id;
    receiver->instrumentation.callback = callee;
    receiver->save_fpstate = save_fpstate;
    receiver->next = handler->receiver_list;
    handler->receiver_list = receiver;

    return handler;
}

static annotation_handler_t *
annot_register_return(void *annotation_func, void *return_value)
{
    annotation_handler_t *handler;
    annotation_receiver_t *receiver;
    ASSERT_TABLE_SYNCHRONIZED(handlers, WRITE);
    handler = (annotation_handler_t *) generic_hash_lookup(GLOBAL_DCONTEXT, handlers,
                                                           KEY(annotation_func));
    if (handler == NULL) {
        handler = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_handler_t,
                                  ACCT_OTHER, UNPROTECTED);
        memset(handler, 0, sizeof(annotation_handler_t));
        handler->type = ANNOT_HANDLER_RETURN_VALUE;
        handler->id.annotation_func = (app_pc) annotation_func;
        generic_hash_add(GLOBAL_DCONTEXT, handlers, KEY(annotation_func), handler);
    }

    receiver = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, annotation_receiver_t,
                               ACCT_OTHER, UNPROTECTED);
    receiver->client_id = 0; // not used
    receiver->instrumentation.return_value = return_value;
    receiver->save_fpstate = false;
    receiver->next = handler->receiver_list;
    handler->receiver_list = receiver;
    return handler;
}

static inline void
annot_bind_registration(generic_func_t target, annotation_registration_by_name_t *by_name)
{
    annotation_handler_t *handler;

    ASSERT_TABLE_SYNCHRONIZED(handlers, WRITE);
    handler = NULL;
    if (by_name->type == ANNOT_HANDLER_CALL) {
        handler = annot_register_call(by_name->client_id, (void *) target,
                                      by_name->instrumentation.callback,
                                      by_name->save_fpstate, by_name->num_args
                                      _IF_NOT_X64(by_name->call_type));
    } else if (by_name->type == ANNOT_HANDLER_RETURN_VALUE) {
        handler = annot_register_return((void *) target,
                                        by_name->instrumentation.return_value);
    } else {
        ASSERT_MESSAGE(CHKLVL_ASSERTS,
                       "Cannot register annotation of this type by name.", false);
    }
    if (handler->symbol_name == NULL)
        handler->symbol_name = heap_strcpy(by_name->symbol_name);
}

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
    }

    return VG_ID__LAST;
}

static inline bool
is_annotation_tag(dcontext_t *dcontext, app_pc start_pc, instr_t *scratch,
                  OUT const char **name)
{
    app_pc cur_pc = start_pc;

    instr_reset(dcontext, scratch);
    cur_pc = decode(dcontext, cur_pc, scratch);
    if (instr_is_mov(scratch)) {
        opnd_t src = instr_get_src(scratch, 0);
        if (IS_ANNOTATION_LABEL_REFERENCE(src)) {
            char buf[24];
            app_pc buf_ptr;
            app_pc src_ptr = GET_ANNOTATION_LABEL_REFERENCE(src, start_pc);
#ifdef UNIX
            app_pc got_ptr;
            instr_reset(dcontext, scratch);
            cur_pc = decode(dcontext, cur_pc, scratch);
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
            if (!safe_read(src_ptr, sizeof(app_pc), &buf_ptr))
                return false;
            if (!safe_read(buf_ptr, 20, buf))
                return false;
            buf[21] = '\0';
            if (strcmp(buf, "dynamorio-annotation") != 0)
                return false;
#ifdef WINDOWS
            instr_reset(dcontext, scratch);
            cur_pc = decode(dcontext, cur_pc, scratch);
            if (!instr_is_prefetch(scratch))
                return false;
#endif
            *name = (const char *) (buf_ptr + 21);
            return true;
        }
    }
    return false;
}

#ifdef WINDOWS
static inline void
identify_annotation(dcontext_t *dcontext, instr_t *cti_instr, OUT const char **name,
                    OUT bool *is_expression, OUT app_pc *annotation_pc)
{
    uint instr_count = 0, cti_count = 0;
    instr_t scratch;
    app_pc last_pc, cur_pc = instr_get_translation(cti_instr);
    app_pc cti_target = instr_get_branch_target_pc(cti_instr);

    *annotation_pc = NULL;
    *name = NULL;
    instr_init(dcontext, &scratch);
    cur_pc = decode_cti(dcontext, cur_pc, &scratch); // skip `cti_instr`
    while (true) {
        instr_reset(dcontext, &scratch);
        last_pc = cur_pc;
        cur_pc = decode_cti(dcontext, cur_pc, &scratch);
        if (instr_is_cti(&scratch)) {
            if (instr_is_call_direct(&scratch)) {
                if ((*name == NULL) &&
                    is_annotation_tag(dcontext, instr_get_branch_target_pc(&scratch),
                                      &scratch, name)) {
                    if (*annotation_pc != NULL)
                        break;
                } else {
                    if (*annotation_pc != NULL) // fail: no tag
                        break;
                    *annotation_pc = last_pc;
                    *is_expression = (cti_target <= last_pc);
                    if (*name != NULL)
                        break;
                }
            }
            if (++cti_count > 1)
                break;
        }
        if (++instr_count > MAX_ANNOTATION_INSTR_COUNT)
            break;
    }
}
#else
/*
  4004f1:   eb 12                   jmp    400505 <main+0x18>
  4004fd:   48 0f bc 05 eb 0a 20    bsf    0x200aeb(%rip),%rax        # 600ff0
  400504:   00
  --> (char ***) (0x400504 + 0x200aeb)

  8048422:   eb 10                   jmp    8048434 <main+0x27>
  8048429:   b8 d7 1b 00 00          mov    $0x1bd7,%eax
  804842e:   2b 05 1c 00 00 00       sub    0x1c,%eax
  --> **(char ***) (0x8048429 + 0x1bd7 + 0x1c)
*/
static inline void
identify_annotation(dcontext_t *dcontext, instr_t *cti_instr, OUT const char **name,
                    OUT bool *is_expression, OUT app_pc *annotation_pc)
{
    instr_t scratch;
    app_pc last_pc, cur_pc = instr_get_translation(cti_instr);

    *name = NULL;
    instr_init(dcontext, &scratch);
    cur_pc = decode_cti(dcontext, cur_pc, &scratch);
    instr_reset(dcontext, &scratch);

    if (is_annotation_tag(dcontext, cur_pc, &scratch, name)) {
        *is_expression = (instr_get_opcode(&scratch) == OP_bsr);
        do {
            instr_reset(dcontext, &scratch);
            last_pc = cur_pc;
            cur_pc = decode_cti(dcontext, cur_pc, &scratch);
        } while (!instr_is_call(&scratch));

        *annotation_pc = last_pc;
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
             annotation_calling_convention_t call_type)
{
    uint i;
    if (call_type == ANNOT_CALL_TYPE_FASTCALL) {
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
    } else { // ANNOT_CALL_TYPE_STDCALL
        for (i = 0; i < num_args; i++) {
            handler->args[i] = OPND_CREATE_MEMPTR(
                DR_REG_XSP, sizeof(ptr_uint_t) * i);
        }
        handler->arg_stack_space = (sizeof(ptr_uint_t) * num_args);
    }
}
#endif

#if defined(WINDOWS) && !defined(X64)
static int
annot_vsnprintf(char *s, size_t max, const char *fmt, ...)
{
    int res;
    va_list ap;
    extern int our_vsnprintf(char *s, size_t max, const char *fmt, va_list ap);

    va_start(ap, fmt);
    res = our_vsnprintf(s, max, fmt, ap);
    va_end(ap);
    return res;
}
#endif

static inline void
free_annotation_registration_by_name(annotation_registration_by_name_t *by_name)
{
    heap_str_free(by_name->target_name);
    heap_str_free(by_name->symbol_name);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, by_name, annotation_registration_by_name_t,
                   ACCT_OTHER, UNPROTECTED);
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
    if (handler->symbol_name != NULL)
        heap_str_free(handler->symbol_name);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, p, annotation_handler_t, ACCT_OTHER, UNPROTECTED);
}
