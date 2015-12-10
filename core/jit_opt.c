#include "globals.h"
#include "annotations.h"
#include "lib/dr_annotations.h"
#include "jit_opt.h"

#define DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_manage_code_area"

#define DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_unmanage_code_area"

static void
annotation_manage_code_area(void *start, size_t size)
{
    /* i#1114 NYI: mark the corresponding VM area as app-managed */
}

static void
annotation_unmanage_code_area(void *start, size_t size)
{
    /* i#1114 NYI: mark the corresponding VM area as app-managed */
}

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
