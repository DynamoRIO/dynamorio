#include "dr_api.h"

static void
thread_exit(void *drcontext)
{
    bool same = dr_get_process_id() == dr_get_process_id_from_drcontext(drcontext);
    dr_fprintf(STDERR, "thread exit: %s process id\n", same ? "same" : "different");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_thread_exit_event(thread_exit);
}
