/* Code Manipulation API Sample:
 * bbcount_region.c
 *
 * Reports the dynamic execution count of all basic blocks.
 */

#include <stddef.h> /* for offsetof */
#include <string.h> /* memset */
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include "dr_annot.h"

#ifdef UNIX
# include <pthread.h>
#endif

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

/*
#ifdef __LP64__
# define _IF_X64(x) , x
# define _IF_NOT_X64(x)
#else
# define _IF_X64(x)
# define _IF_NOT_X64(x) , x
#endif
*/

#define REPORT_ENABLED 1
#if defined(REPORT_ENABLED) // && defined(SHOW_RESULTS)
# define REPORT(...) \
do { \
    if (dr_is_notify_on()) \
        dr_fprintf(STDERR, __VA_ARGS__); \
} while (0);
#else
# define REPORT(...)
#endif

#define NULL_TERMINATE(buf) buf[(sizeof(buf)/sizeof(buf[0])) - 1] = '\0'

#define TESTALL(mask, var) (((mask) & (var)) == (mask))
#define TESTANY(mask, var) (((mask) & (var)) != 0)

#define REGISTER_ANNOTATION_NOARG_CALL(drcontext, handle, target_name, call) \
do { \
    generic_func_t target = dr_get_proc_address(handle, target_name); \
    if (target != NULL) \
        annot_register_call(drcontext, target, call, false, 0); \
} while (0)

static reg_id_t tls_segment_register;
static uint tls_offset;

typedef struct _stats_t {
    uint id;
    char *label;
    uint current_region_start;
    uint process_total;
    uint region_count;
    struct _stats_t *next;
} stats_t;

typedef struct _stats_list_t {
    stats_t *head;
    stats_t *tail;
} stats_list_t;

static void *stats_lock;
static stats_list_t *stats_list;

typedef struct _counter_t {
    uint count;
} counter_t;

/**** Private Function Declarations ****/

static stats_t *get_stats(uint id);

static void init_counter(uint id, const char *label);

static void start_counter();

static void stop_counter();

static void
get_basic_block_stats(uint id, uint *region_count, uint *bb_count);

static void
test_many_args(uint a, uint b, uint c, uint d, uint e, uint f,
               uint g, uint h, uint i, uint j);

static ptr_uint_t
handle_make_mem_defined_if_addressable(vg_client_request_t *request);

static counter_t *get_counter();

static void event_module_load(void *drcontext, const module_data_t *info, bool loaded);

static void event_thread_init(void *dcontext);

static dr_emit_flags_t event_basic_block(void *dcontext, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating);

static void event_exit(void);

/**** Public Function Definitions ****/

DR_EXPORT void
dr_init(client_id_t id)
{
    const char *options = dr_get_options(id);
    char *c;

    for (c = (char *) options; *c != '\0'; c++) {
        if (*c == '-') {
            switch (*(++c)) {
                case 'v':
                    annot_register_valgrind(
                        VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                        handle_make_mem_defined_if_addressable);
                    break;
                case '\0':
                    break;
            }
        }
    }

    stats_lock = dr_mutex_create();

    stats_list = dr_global_alloc(sizeof(stats_list_t));
    memset(stats_list, 0, sizeof(stats_list_t));

    /* register events */
    dr_register_exit_event(event_exit);
    dr_register_module_load_event(event_module_load);
    dr_register_thread_init_event(event_thread_init);
    dr_register_bb_event(event_basic_block);

    /* allocate TLS */
    dr_raw_tls_calloc(&tls_segment_register, &tls_offset, 1, 0);

#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
# ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called in dr_init(). */
        dr_enable_console_printing();
# endif
    }
#endif

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, LOG_ALL, 1, "Client 'bbcount_region' initializing\n");
    REPORT("Client bbcount_region is running\n");
}

/**** Private Function Definitions ****/

static inline stats_t *
get_stats(uint id)
{
    stats_t *stats = stats_list->head;
    for (; stats != NULL; stats = stats->next) {
        if (stats->id == id)
            return stats;
    }
    return NULL;
}

static void
init_counter(uint id, const char *label)
{
    stats_t *stats;
    dr_mutex_lock(stats_lock);

    REPORT("Client 'bbcount_region' initializing counter id %d with label '%s'\n",
        id, label);

    stats = get_stats(id);
    if (stats == NULL) {
        stats = dr_global_alloc(sizeof(stats_t));
        memset(stats, 0, sizeof(stats_t));
        stats->id = id;
        stats->label = dr_global_alloc((sizeof(char) * strlen(label)) + 1);
        strcpy(stats->label, label);

        if (stats_list->head == NULL) {
            stats_list->head = stats_list->tail = stats;
        } else {
            stats_list->tail->next = stats;
            stats_list->tail = stats;
        }
    }

    dr_mutex_unlock(stats_lock);
}

static void
start_counter(uint id)
{
    stats_t *stats;
    counter_t *counter = get_counter();

    dr_mutex_lock(stats_lock);
    stats = get_stats(id);
    if (stats != NULL)
        stats->current_region_start = counter->count;
    dr_mutex_unlock(stats_lock);

    REPORT("Client 'bbcount_region' starting counter id(%d) on DC 0x%x at %d\n",
        id, dr_get_current_drcontext(), counter->count);
}

static void
stop_counter(uint id)
{
    stats_t *stats;
    counter_t *counter = get_counter();

    dr_mutex_lock(stats_lock);
    stats = get_stats(id);
    if (stats != NULL) {
        stats->process_total += (counter->count - stats->current_region_start);
        stats->region_count++;
    }
    dr_mutex_unlock(stats_lock);

    REPORT("Client 'bbcount_region' stopping counter id(%d) on DC 0x%x at raw count %d "
        "(accumulated total %d)\n",
        id, dr_get_current_drcontext(), counter->count, stats->process_total);
}

static void
get_basic_block_stats(uint id, uint *region_count, uint *bb_count)
{
    stats_t *stats;
    dr_mutex_lock(stats_lock);

    stats = get_stats(id);
    if (stats != NULL) {
        *region_count = stats->region_count;
        *bb_count = stats->process_total;
    } else {
        *region_count = *bb_count = 0;
    }

    REPORT("Client 'bbcount_region' providing stats for id(%d): "
        "region_count=%d, bb_count=%d\n",
        id, *region_count, *bb_count);

    dr_mutex_unlock(stats_lock);
}

static void
test_many_args(uint a, uint b, uint c, uint d, uint e, uint f,
               uint g, uint h, uint i, uint j)
{
//#ifdef SHOW_RESULTS
    char msg[512];
    int len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                      "Test many arguments: a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, "
                      "g=%d, h=%d, i=%d, j=%d, \n",
                      a, b, c, d, e, f, g, h, i, j);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
//#endif /* SHOW_RESULTS */
}

static ptr_uint_t
handle_make_mem_defined_if_addressable(vg_client_request_t *request)
{
    dr_fprintf(STDOUT, "handle_make_mem_defined_if_addressable("PFX", 0x%x)\n",
        request->args[0], request->args[1]);
    return 0;
}

static inline
counter_t *get_counter()
{
    return (counter_t *)(((ptr_uint_t) dr_get_dr_segment_base(tls_segment_register) + tls_offset));
}

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    annot_find_and_register_call(drcontext, info, "bb_region_annotate_init_counter",
        (void *) init_counter, 2 _IF_NOT_X64(ANNOT_FASTCALL));
    annot_find_and_register_call(drcontext, info, "bb_region_annotate_start_counter",
        (void *) start_counter, 1 _IF_NOT_X64(ANNOT_FASTCALL));
    annot_find_and_register_call(drcontext, info, "bb_region_annotate_stop_counter",
        (void *) stop_counter, 1 _IF_NOT_X64(ANNOT_FASTCALL));
    annot_find_and_register_call(drcontext, info, "bb_region_get_basic_block_stats",
        (void *) get_basic_block_stats, 3 _IF_NOT_X64(ANNOT_FASTCALL));
    annot_find_and_register_call(drcontext, info, "bb_region_test_many_args",
        (void *) test_many_args, 10 _IF_NOT_X64(ANNOT_FASTCALL));
}

static void
event_thread_init(void *dcontext)
{
    counter_t *counter = get_counter();
    counter->count = 0;
}

static dr_emit_flags_t
event_basic_block(void *dcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *first = instrlist_first(bb);

#ifdef VERBOSE
    dr_printf("in dynamorio_basic_block(tag="PFX")\n", tag);
# ifdef VERBOSE_VERBOSE
    instrlist_disassemble(dcontext, tag, bb, STDOUT);
# endif
#endif

    dr_save_reg(dcontext, bb, first, DR_REG_XAX, SPILL_SLOT_1);
    instrlist_meta_preinsert(bb, first, INSTR_CREATE_mov_ld(dcontext,
        opnd_create_reg(DR_REG_XAX), opnd_create_far_base_disp(
            tls_segment_register, DR_REG_NULL, DR_REG_NULL, 0, tls_offset, OPSZ_PTR)));
    instrlist_meta_preinsert(bb, first, INSTR_CREATE_lea(dcontext,
        opnd_create_reg(DR_REG_XAX),
        opnd_create_base_disp(DR_REG_NULL, DR_REG_XAX, 1, 1, OPSZ_lea)));
    instrlist_meta_preinsert(bb, first, INSTR_CREATE_mov_st(dcontext,
        opnd_create_far_base_disp(tls_segment_register, DR_REG_NULL, DR_REG_NULL,
        0, tls_offset, OPSZ_PTR), opnd_create_reg(DR_REG_XAX)));
    dr_restore_reg(dcontext, bb, first, DR_REG_XAX, SPILL_SLOT_1);

#if defined(VERBOSE) && defined(VERBOSE_VERBOSE)
    dr_printf("Finished instrumenting dynamorio_basic_block(tag="PFX")\n", tag);
    instrlist_disassemble(dcontext, tag, bb, STDOUT);
#endif
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    stats_t *stats, *next;
//#ifdef SHOW_RESULTS
    char msg[512];
    int len;

    for (stats = stats_list->head; stats != NULL; stats = next) {
        next = stats->next;
        len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                          "Instrumentation results for '%s':\n"
                          "%10d basic block executions\n"
                          "%10d region commits\n",
                          stats->label,
                          stats->process_total,
                          stats->region_count);
        DR_ASSERT(len > 0);
        NULL_TERMINATE(msg);
        DISPLAY_STRING(msg);
    }
//#endif /* SHOW_RESULTS */

    dr_raw_tls_cfree(tls_offset, 1);
    dr_mutex_destroy(stats_lock);
    for (stats = stats_list->head; stats != NULL; stats = next) {
        next = stats->next;
        dr_global_free(stats->label, (sizeof(char) * strlen(stats->label)) + 1);
        dr_global_free(stats, sizeof(stats_t));
    }
    dr_global_free(stats_list, sizeof(stats_list_t));
}
