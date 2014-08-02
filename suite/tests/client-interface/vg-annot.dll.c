#include "dr_api.h"
#include <string.h>

static uint num_bytes_made_defined = 0;
static uint num_define_memory_requests = 0;

static client_id_t client_id;

#ifdef WINDOWS
static app_pc *skip_truncation = NULL;
#endif

static ptr_uint_t
handle_running_on_valgrind(vg_client_request_t *request)
{
    return 1;
}

static ptr_uint_t
handle_make_mem_defined_if_addressable(vg_client_request_t *request)
{
    dr_printf("Make %d bytes defined if addressable.\n", request->args[1]);

    num_bytes_made_defined += request->args[1];
    num_define_memory_requests++;

    return 0;
}

#ifdef WINDOWS // truncating this block causes an app exception (unrelated to annotations)
static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    if ((info->names.module_name != NULL) &&
        (strcmp("ntdll.dll", info->names.module_name) == 0)) {
        *skip_truncation = (app_pc) dr_get_proc_address(info->handle,
                                                        "KiUserExceptionDispatcher");
    }
}
#endif

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
    if (dr_fragment_app_pc(tag) == *skip_truncation)
        return DR_EMIT_DEFAULT;
#endif

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
    return DR_EMIT_DEFAULT;
}

void exit_event(void)
{
    dr_printf("Received %d 'define memory' requests for a total of %d bytes.\n",
        num_define_memory_requests, num_bytes_made_defined);

#ifdef WINDOWS
    dr_global_free(skip_truncation, sizeof(app_pc));
#endif
}

DR_EXPORT
void dr_init(client_id_t id)
{
    const char *options = dr_get_options(id);

    client_id = id;

    if (strcmp(options, "+bb") == 0) {
        dr_printf("Init vg-annot with full decoding.\n");
        dr_register_bb_event(empty_bb_event);
    } else if (strcmp(options, "+b/b") == 0) {
        dr_printf("Init vg-annot with bb truncation.\n");
        dr_register_bb_event(bb_event_truncate);
    } else {
        dr_printf("Init vg-annot with fast decoding.\n");
    }

#ifdef WINDOWS
    skip_truncation = dr_global_alloc(sizeof(app_pc));
    *skip_truncation = NULL;

    dr_register_module_load_event(event_module_load);
#endif
    dr_register_exit_event(exit_event);

    dr_annotation_register_valgrind(client_id, VG_ID__RUNNING_ON_VALGRIND,
                                    handle_running_on_valgrind);

    dr_annotation_register_valgrind(client_id, VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                    handle_make_mem_defined_if_addressable);
}
