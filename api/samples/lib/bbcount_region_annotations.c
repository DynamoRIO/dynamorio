#include "bbcount_region_annotations.h"

#pragma auto_inline(off)

void
bb_region_annotate_init_counter(unsigned int id, const char *label)
{
}

void
bb_region_annotate_start_counter(unsigned int id)
{
}

void
bb_region_annotate_stop_counter(unsigned int id)
{
}

void
bb_region_get_basic_block_stats(unsigned int id, unsigned int *commit_count,
    unsigned int *bb_count)
{
    *commit_count = 0;
    *bb_count = 0;
}

#pragma auto_inline(on)
