#include <string.h> /* memset */
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include "dr_annotation.h"

#define MAX_MODE_HISTORY 100

#define PRINT(s) dr_printf("      <"s">\n")
#define PRINTF(s, ...) dr_printf("      <"s">\n", __VA_ARGS__)

typedef struct _context_t {
    uint id;
    char *label;
    uint mode;
    uint *mode_history;
    uint mode_history_index;
    struct _context_t *next;
} context_t;

typedef struct _context_list_t {
    context_t *head;
    context_t *tail;
} context_list_t;

static void *context_lock;
static context_list_t *context_list;

static client_id_t client_id;

#ifdef WINDOWS
static app_pc *skip_truncation;
#endif

typedef struct _counter_t {
    uint count;
} counter_t;

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

static void
init_mode(uint mode)
{
    PRINTF("Initialize mode %d", mode);
}

static void
init_context(uint id, const char *label, uint initial_mode)
{
    context_t *context;
    dr_mutex_lock(context_lock);

    PRINTF("Initialize context %d '%s' in mode %d", id, label, initial_mode);

    context = get_context(id);
    if (context == NULL) {
        uint label_length = (uint) (sizeof(char) * strlen(label)) + 1;
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
test_eight_args(uint a, uint b, uint c, uint d, uint e, uint f,
                uint g, uint h)
{
    PRINTF("Test many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d",
        a, b, c, d, e, f, g, h);
}

static void
test_nine_args(uint a, uint b, uint c, uint d, uint e, uint f,
               uint g, uint h, uint i)
{
    PRINTF("Test many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d",
        a, b, c, d, e, f, g, h, i);
}

static void
test_ten_args(uint a, uint b, uint c, uint d, uint e, uint f,
              uint g, uint h, uint i, uint j)
{
    PRINTF("Test many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d, j=%d",
        a, b, c, d, e, f, g, h, i, j);
}

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    //register_call(drcontext, info, "test_annotation_nine_args",
    //    (void *) test_nine_args, 9);

#ifdef WINDOWS // truncating these blocks causes app exceptions (unrelated to annotations)
    if ((info->names.module_name != NULL) &&
        (strcmp("ntdll.dll", info->names.module_name) == 0)) {
        skip_truncation[0] = (app_pc) dr_get_proc_address(info->handle,
                                                          "KiUserExceptionDispatcher");
        skip_truncation[1] = (app_pc) dr_get_proc_address(info->handle,
                                                          "LdrInitializeThunk");
    }
#endif
}

dr_emit_flags_t
empty_bb_event(void *drcontext, void *tag, instrlist_t *bb,
               bool for_trace, bool translating)
{
    return DR_EMIT_DEFAULT;
}

dr_emit_flags_t
bb_event_truncate(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *prev, *first = instrlist_first(bb), *instr = instrlist_last(bb);

#ifdef WINDOWS
    app_pc fragment = dr_fragment_app_pc(tag);
    if ((fragment == skip_truncation[0]) || (fragment == skip_truncation[1]))
        return DR_EMIT_DEFAULT;
#endif

    while ((first != NULL) && !instr_ok_to_mangle(first))
        first = instr_get_next(first);
    if (first != NULL) {
        while ((instr != NULL) && (instr != first) && !instr_ok_to_mangle(instr)) {
            prev = instr_get_prev(instr);
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
            instr = prev;
        }
        if ((instr != NULL) && (instr != first)) {
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
register_call(const char *annotation, void *target, uint num_args)
{
    dr_annot_register_call_by_name(client_id, annotation, target, false, num_args
        _IF_NOT_X64(ANNOT_CALL_TYPE_FASTCALL));
}

static void
event_exit(void)
{
    uint i;
    context_t *context, *next;

    for (context = context_list->head; context != NULL; context = next) {
        next = context->next;

        for (i = 1; i < context->mode_history_index; i++) {
            PRINTF("In context %d at event %d, the mode changed from %d to %d",
                context->id, i, context->mode_history[i-1], context->mode_history[i]);
        }
        PRINTF("Context '%s' terminates in mode %d",
               context->label, context->mode);
    }

    dr_mutex_destroy(context_lock);
    for (context = context_list->head; context != NULL; context = next) {
        next = context->next;

        dr_global_free(context->label, (sizeof(char) * strlen(context->label)) + 1);
        dr_global_free(context->mode_history, MAX_MODE_HISTORY * sizeof(uint));
        dr_global_free(context, sizeof(context_t));
    }
    dr_global_free(context_list, sizeof(context_list_t));

#ifdef WINDOWS
    dr_global_free(skip_truncation, 2 * sizeof(app_pc));
#endif
}

DR_EXPORT void
dr_init(client_id_t id)
{
    const char *options = dr_get_options(id);

# ifdef WINDOWS
    dr_enable_console_printing();
# endif

    client_id = id;

    if (strcmp(options, "+bb") == 0) {
        PRINT("Init annotation test client with full decoding");
        dr_register_bb_event(empty_bb_event);
    } else if (strcmp(options, "+b/b") == 0) {
        PRINT("Init annotation test client with bb truncation");
        dr_register_bb_event(bb_event_truncate);
    } else {
        PRINT("Init annotation test client with fast decoding");
    }

    context_lock = dr_mutex_create();

    context_list = dr_global_alloc(sizeof(context_list_t));
    memset(context_list, 0, sizeof(context_list_t));

#ifdef WINDOWS
    skip_truncation = dr_global_alloc(2 * sizeof(app_pc));
    memset(skip_truncation, 0, 2 * sizeof(app_pc));
#endif

    dr_register_exit_event(event_exit);
    dr_register_module_load_event(event_module_load);

    register_call("test_annotation_init_mode", (void *) init_mode, 1);
    register_call("test_annotation_init_context", (void *) init_context, 3);
    register_call("test_annotation_set_mode", (void *) set_mode, 2);
    register_call("test_annotation_eight_args", (void *) test_eight_args, 8);
    register_call("test_annotation_ten_args", (void *) test_ten_args, 10);

    register_call("test_annotation_nine_args", (void *) test_nine_args, 9);
}

/*
· Register by addr
· Unregister by name
· Register by name
· Unregister by addr
· Unregister Valgrind annots
· Unregister return
· Register return by name

By name un/register: execute existing in loaded module, also load module and execute
By addr un/register: execute
*/
