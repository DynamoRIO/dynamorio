#include "dr_api.h"
#include <string.h>

static uint num_bytes_made_defined = 0;
static uint num_define_memory_requests = 0;

static ptr_uint_t
handle_running_on_valgrind(vg_client_request_t *request)
{
    dr_printf("handle_running_on_valgrind()\n");
    return 1;
}

static ptr_uint_t
handle_make_mem_defined_if_addressable(vg_client_request_t *request)
{
    dr_printf("handle_make_mem_defined_if_addressable(): %d bytes\n",
        request->args[1]);

    num_bytes_made_defined += request->args[1];
    num_define_memory_requests++;

    return 0;
}

dr_emit_flags_t
empty_bb_event(void *drcontext, void *tag, instrlist_t *bb,
               bool for_trace, bool translating)
{
}

void exit_event(void)
{
    dr_printf("Received %d 'define memory' requests.\n", num_define_memory_requests);
    dr_printf("Made %d memory bytes defined.\n", num_bytes_made_defined);
}

// TODO: second mode which registers for bb event, to test full-decode path
DR_EXPORT
void dr_init(client_id_t id)
{
    const char *options = dr_get_options(id);

    if (strcmp(options, "+bb") == 0) {
        dr_printf("Init vg-annot with full decoding.\n");
        dr_register_bb_event(empty_bb_event);
    } else {
        dr_printf("Init vg-annot with fast decoding.\n");
    }

    dr_register_exit_event(exit_event);

    dr_annot_register_valgrind(
        VG_ID__RUNNING_ON_VALGRIND,
        handle_running_on_valgrind);

    dr_annot_register_valgrind(
        VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
        handle_make_mem_defined_if_addressable);
}
