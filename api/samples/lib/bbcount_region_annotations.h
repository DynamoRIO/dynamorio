#ifndef _BB_REGION_ANNOTATIONS_H_
#define _BB_REGION_ANNOTATIONS_H_ 1

#ifdef UNIX
# define BB_REGION_ANNOTATE_WEAK_ATTRIBUTE __attribute__ ((weak))
#else
# define BB_REGION_ANNOTATE_WEAK_ATTRIBUTE
#endif

#define BB_REGION_ANNOTATE_INIT_COUNTER(id, label) \
    bb_region_annotate_init_counter(id, label)

#define BB_REGION_ANNOTATE_START_COUNTER(id) \
    bb_region_annotate_start_counter(id)

#define BB_REGION_ANNOTATE_STOP_COUNTER(id) \
    bb_region_annotate_stop_counter(id)

#define BB_REGION_GET_BASIC_BLOCK_STATS(id, commit_count, bb_count) \
    bb_region_get_basic_block_stats(id, commit_count, bb_count)

#ifdef __cplusplus
extern "C" {
#endif

void
bb_region_annotate_init_counter(unsigned int id, const char *label)
    BB_REGION_ANNOTATE_WEAK_ATTRIBUTE;

void
bb_region_annotate_start_counter(unsigned int id) BB_REGION_ANNOTATE_WEAK_ATTRIBUTE;

void
bb_region_annotate_stop_counter(unsigned int id) BB_REGION_ANNOTATE_WEAK_ATTRIBUTE;

void
bb_region_get_basic_block_stats(unsigned int id, unsigned int *commit_count,
    unsigned int *bb_count) BB_REGION_ANNOTATE_WEAK_ATTRIBUTE;

#ifdef __cplusplus
}
#endif

#endif

