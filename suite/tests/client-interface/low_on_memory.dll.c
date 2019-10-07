/*
 * API regression test low on memory events. Upon the calling of malloc
 * routines, the test allocates large chunks of data, filling up memory.
 * The test then checks that DynamoRIO triggers a low on memory call-back,
 * which in turn clears the memory allocated.
 *
 * The test currently assumes that the application is single-threaded.
 */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drwrap.h"

typedef struct node_type {

    int int_array[50000];
    struct node_type *next;

} node_t;

static bool is_wrapped;
static bool is_clear;
static node_t *head;

static void
insert_new_node()
{

    node_t **node = &head;

    while (*node != NULL) {
        node = &((*node)->next);
    }

    *node = dr_global_alloc(sizeof(node_t));
    (*node)->next = NULL;
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    if (!is_clear)
        insert_new_node();
}

static void
module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    app_pc towrap = (app_pc)dr_get_proc_address(mod->handle, "malloc");
    if (towrap != NULL) {
        is_wrapped |= drwrap_wrap(towrap, wrap_pre, NULL);
    }
}

static void
low_on_memory_event()
{
    if (!is_clear) {
        if (head == NULL)
            dr_fprintf(STDERR, "clear mismatch!\n");

        node_t *node = head;
        while (node != NULL) {
            node_t *temp = node;
            node = node->next;
            dr_global_free(temp, sizeof(node_t));
        }

        dr_fprintf(STDERR, "low on memory event!\n");
        is_clear = true;
        head = NULL;
    } else {
        dr_fprintf(STDERR, "another low on memory event!\n");
    }
}

static void
exit_event(void)
{
    if (!is_wrapped)
        dr_fprintf(STDERR, "was not wrapped!\n");

    if (!is_clear)
        dr_fprintf(STDERR, "was not cleared!\n");

    if (!dr_unregister_low_on_memory_event(low_on_memory_event))
        dr_fprintf(STDERR, "unregister failed!\n");

    drmgr_unregister_module_load_event(module_load_event);
    dr_unregister_exit_event(exit_event);

    dr_flush_file(STDOUT);

    drwrap_exit();
    drmgr_exit();
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    drmgr_init();
    drwrap_init();

#ifdef WINDOWS
    if (dr_is_notify_on())
        dr_enable_console_printing();
#endif

    is_wrapped = false;
    is_clear = false;
    insert_new_node();

    drmgr_register_module_load_event(module_load_event);
    dr_register_exit_event(exit_event);
    dr_register_low_on_memory_event(low_on_memory_event);
}
