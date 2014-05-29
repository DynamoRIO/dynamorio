#include "dr_api.h"

static uint num_bytes_made_defined = 0;
static uint num_define_memory_requests = 0;

static ptr_uint_t
handle_running_on_valgrind(vg_client_request_t *request)
{
    return 1;
}

static ptr_uint_t
handle_make_mem_defined_if_addressable(vg_client_request_t *request)
{
    dr_fprintf(STDOUT, "handle_make_mem_defined_if_addressable(): %d bytes\n",
        request->args[1]);

    num_bytes_made_defined += request->args[1];
    num_define_memory_requests++;

    return 0;
}

void exit_event(void)
{
    dr_printf("Received %d 'define memory' requests.\n", num_define_memory_requests);
    dr_printf("Made %d memory bytes defined.\n", num_bytes_made_defined);
}

DR_EXPORT
void dr_init(client_id_t id)
{
    dr_register_exit_event(exit_event);

    annot_register_valgrind(
        VG_ID__RUNNING_ON_VALGRIND,
        handle_running_on_valgrind);

    annot_register_valgrind(
        VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
        handle_make_mem_defined_if_addressable);
}
