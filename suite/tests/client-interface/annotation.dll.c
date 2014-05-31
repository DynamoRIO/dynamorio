#include <string.h> /* memset */
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include "dr_annotation.h"

#define MAX_MODE_HISTORY 100

#define PRINT(s) dr_printf("\t<"s">\n")
#define PRINTF(s, ...) dr_printf("\t<"s">\n", __VA_ARGS__)

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
        context = dr_global_alloc(sizeof(context_t));
        context->id = id;
        context->label = dr_global_alloc((sizeof(char) * strlen(label)) + 1);
        context->mode = initial_mode;
        context->mode_history = dr_global_alloc(MAX_MODE_HISTORY * sizeof(uint));
        context->mode_history[0] = initial_mode;
        context->mode_history_index = 1;
        strcpy(context->label, label);
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
register_call(void *drcontext, const module_data_t *info, const char *annotation,
    void *target, uint num_args)
{
    if (!dr_annot_find_and_register_call(drcontext, info, annotation, target, num_args
        _IF_NOT_X64(ANNOT_FASTCALL))) {
        //dr_fprintf(STDERR, "\t[Found no instances of annotation '%s' in module %s]\n",
        //    annotation, dr_module_preferred_name(info));
    }
}

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    register_call(drcontext, info, "test_annotation_init_mode",
        (void *) init_mode, 1);
    register_call(drcontext, info, "test_annotation_init_context",
        (void *) init_context, 3);
    register_call(drcontext, info, "test_annotation_set_mode",
        (void *) set_mode, 2);
    register_call(drcontext, info, "test_annotation_eight_args",
        (void *) test_eight_args, 8);
    register_call(drcontext, info, "test_annotation_nine_args",
        (void *) test_nine_args, 9);
    register_call(drcontext, info, "test_annotation_ten_args",
        (void *) test_ten_args, 10);
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
}

DR_EXPORT void
dr_init(client_id_t id)
{
    PRINT("Init annotation test client");

    context_lock = dr_mutex_create();

    context_list = dr_global_alloc(sizeof(context_list_t));
    memset(context_list, 0, sizeof(context_list_t));

    dr_register_exit_event(event_exit);
    dr_register_module_load_event(event_module_load);

# ifdef WINDOWS
    dr_enable_console_printing();
# endif
}
