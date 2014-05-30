#include "bbcount_region_annotations.h"

#pragma auto_inline(off)

#ifdef _MSC_VER
__declspec(dllexport) void __fastcall
bb_region_annotate_init_counter(unsigned int id, const char *label)
{
}

__declspec(dllexport) void __fastcall
bb_region_annotate_start_counter(unsigned int id)
{
}

__declspec(dllexport) void __fastcall
bb_region_annotate_stop_counter(unsigned int id)
{
}

__declspec(dllexport) void __fastcall
bb_region_get_basic_block_stats(unsigned int id, unsigned int *commit_count,
    unsigned int *bb_count)
{
    *commit_count = 0;
    *bb_count = 0;
}
#else
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_annotate_init_counter(unsigned int id, const char *label)
{
}

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_annotate_start_counter(unsigned int id)
{
}

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_annotate_stop_counter(unsigned int id)
{
}

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_get_basic_block_stats(unsigned int id, unsigned int *commit_count,
    unsigned int *bb_count)
{
    *commit_count = 0;
    *bb_count = 0;
}
#endif

#pragma auto_inline(on)
