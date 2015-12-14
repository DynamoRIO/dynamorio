#include "globals.h"
#include "annotations.h"
#include "lib/dr_annotations.h"
#include "jit_opt.h"

#define DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_manage_code_area"

#define DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_unmanage_code_area"

#ifdef ANNOTATIONS
static void
annotation_manage_code_area(void *start, size_t size)
{
    LOG(GLOBAL, LOG_ANNOTATIONS, 2,
        "Add code area "PFX"-"PFX" to JIT managed regions\n",
        start, (app_pc) start + size);

    set_region_jit_managed(start, size);
}

static void
annotation_unmanage_code_area(void *start, size_t size)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (!is_jit_managed_area(start))
        return;

    LOG(GLOBAL, LOG_ANNOTATIONS, 2,
        "Remove code area "PFX"-"PFX" from JIT managed regions\n",
        start, (app_pc) start + size);

    mutex_lock(&thread_initexit_lock);
    flush_fragments_and_remove_region(dcontext, start, size,
                                      true/*own initexit_lock*/, false/*keep futures*/);
    mutex_unlock(&thread_initexit_lock);

    jitopt_clear_span(start, (app_pc) start + size);
}
#endif

void
jitopt_init()
{
    if (DYNAMO_OPTION(opt_jit)) {
#ifdef ANNOTATIONS
        dr_annotation_register_call(DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME,
                                    (void *) annotation_manage_code_area, false, 2,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);
        dr_annotation_register_call(DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME,
                                    (void *) annotation_unmanage_code_area, false, 2,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);
#endif /* ANNOTATIONS */
    }
}

void
jitopt_exit()
{
    /* nothing */
}

void
jitopt_add_dgc_bb(app_pc start_pc, app_pc end_pc, bool is_trace_head)
{
    /* i#1114 NYI: add bb to the fragment tree */
}

void
jitopt_clear_span(app_pc start, app_pc end)
{
    /* i#1114 NYI: remove fragments overlapping [start,end) from the fragment tree */
}
