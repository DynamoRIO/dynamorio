#ifndef _BB_REGION_ANNOTATIONS_H_
#define _BB_REGION_ANNOTATIONS_H_ 1

#define BB_REGION_ANNOTATE_INIT_COUNTER(id, label) \
    bb_region_annotate_init_counter(id, label)

#define BB_REGION_ANNOTATE_START_COUNTER(id) \
    bb_region_annotate_start_counter(id)

#define BB_REGION_ANNOTATE_STOP_COUNTER(id) \
    bb_region_annotate_stop_counter(id)

#define BB_REGION_GET_BASIC_BLOCK_STATS(id, commit_count, bb_count) \
    bb_region_get_basic_block_stats(id, commit_count, bb_count)

#define BB_REGION_TEST_EIGHT_ARGS(a, b, c, d, e, f, g, h) \
    bb_region_test_eight_args(a, b, c, d, e, f, g, h)

#define BB_REGION_TEST_NINE_ARGS(a, b, c, d, e, f, g, h, i) \
    bb_region_test_nine_args(a, b, c, d, e, f, g, h, i)

#define BB_REGION_TEST_TEN_ARGS(a, b, c, d, e, f, g, h, i, j) \
    bb_region_test_ten_args(a, b, c, d, e, f, g, h, i, j)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
__declspec(dllexport) void __fastcall
bb_region_annotate_init_counter(unsigned int id, const char *label);

__declspec(dllexport) void __fastcall
bb_region_annotate_start_counter(unsigned int id);

__declspec(dllexport) void __fastcall
bb_region_annotate_stop_counter(unsigned int id);

__declspec(dllexport) void __fastcall
bb_region_get_basic_block_stats(unsigned int id, unsigned int *commit_count,
    unsigned int *bb_count);

__declspec(dllexport) void __fastcall
bb_region_test_eight_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h);

__declspec(dllexport) void __fastcall
bb_region_test_nine_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i);

__declspec(dllexport) void __fastcall
bb_region_test_ten_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i, unsigned int j);
#else
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_annotate_init_counter(unsigned int id, const char *label)
    __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_annotate_start_counter(unsigned int id) __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_annotate_stop_counter(unsigned int id) __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_get_basic_block_stats(unsigned int id, unsigned int *commit_count,
    unsigned int *bb_count) __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_test_eight_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h)
    __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_test_nine_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i)
    __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
bb_region_test_ten_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i, unsigned int j)
    __attribute__ ((weak));
# endif

#ifdef __cplusplus
}
#endif

#endif

