/* Code Manipulation API Sample:
 * bbcount_region.c
 *
 * Reports the dynamic execution count of all basic blocks.
 */

#include <stddef.h> /* for offsetof */
#include <string.h> /* memset */
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include "dr_annotation.h"

typedef struct _context_t {
    uint id;
    char *label;
    uint mode;
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
    dr_fprintf(STDERR, "Initialize mode %d\n", mode);
}

static void
init_context(uint id, const char *label, uint initial_mode)
{
    context_t *context;
    dr_mutex_lock(context_lock);

    dr_printf("Initialize context %d '%s' in mode %d.\n", id, label, initial_mode);

    context = get_context(id);
    if (context == NULL) {
        context = dr_global_alloc(sizeof(context_t));
        context->id = id;
        context->label = dr_global_alloc((sizeof(char) * strlen(label)) + 1);
        context->mode = initial_mode;
        strcpy(context->label, label);

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
    }
    dr_mutex_unlock(context_lock);

    dr_printf("Set context %d mode from %d to %d.\n",
        context_id, original_mode, new_mode);
}

static void
test_eight_args(uint a, uint b, uint c, uint d, uint e, uint f,
                uint g, uint h)
{
    dr_fprintf(STDERR, "Client 'bbcount_region' tests many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d\n",
        a, b, c, d, e, f, g, h);
}

static void
test_nine_args(uint a, uint b, uint c, uint d, uint e, uint f,
               uint g, uint h, uint i)
{
    dr_fprintf(STDERR, "Client 'bbcount_region' tests many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d\n",
        a, b, c, d, e, f, g, h, i);
}

static void
test_ten_args(uint a, uint b, uint c, uint d, uint e, uint f,
              uint g, uint h, uint i, uint j)
{
    dr_fprintf(STDERR, "Client 'bbcount_region' tests many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d, j=%d\n",
        a, b, c, d, e, f, g, h, i, j);
}

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    dr_annot_find_and_register_call(drcontext, info, "test_annotation_init_mode",
        (void *) init_mode, 1 _IF_NOT_X64(ANNOT_FASTCALL));
    dr_annot_find_and_register_call(drcontext, info, "test_annotation_init_context",
        (void *) init_context, 3 _IF_NOT_X64(ANNOT_FASTCALL));
    dr_annot_find_and_register_call(drcontext, info, "test_annotation_set_mode",
        (void *) set_mode, 2 _IF_NOT_X64(ANNOT_FASTCALL));
    dr_annot_find_and_register_call(drcontext, info, "bb_region_test_eight_args",
        (void *) test_eight_args, 8 _IF_NOT_X64(ANNOT_FASTCALL));
    dr_annot_find_and_register_call(drcontext, info, "bb_region_test_nine_args",
        (void *) test_nine_args, 9 _IF_NOT_X64(ANNOT_FASTCALL));
    dr_annot_find_and_register_call(drcontext, info, "bb_region_test_ten_args",
        (void *) test_ten_args, 10 _IF_NOT_X64(ANNOT_FASTCALL));
}

static void
event_exit(void)
{
    context_t *context, *next;
    for (context = context_list->head; context != NULL; context = next) {
        next = context->next;
        dr_printf("Context '%s' terminates in mode %d.\n",
               context->label, context->mode);
    }

    dr_mutex_destroy(context_lock);
    for (context = context_list->head; context != NULL; context = next) {
        next = context->next;
        dr_global_free(context->label, (sizeof(char) * strlen(context->label)) + 1);
        dr_global_free(context, sizeof(context_t));
    }
    dr_global_free(context_list, sizeof(context_list_t));
}

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_printf("Init annotation test client.\n");

    context_lock = dr_mutex_create();

    context_list = dr_global_alloc(sizeof(context_list_t));
    memset(context_list, 0, sizeof(context_list_t));

    dr_register_exit_event(event_exit);
    dr_register_module_load_event(event_module_load);

# ifdef WINDOWS
    dr_enable_console_printing();
# endif
}
