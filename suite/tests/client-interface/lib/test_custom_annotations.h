#ifndef _BB_REGION_ANNOTATIONS_H_
#define _BB_REGION_ANNOTATIONS_H_ 1

#define TEST_ANNOTATION_INIT_MODE(mode) \
    test_annotation_init_mode(mode)

#define TEST_ANNOTATION_INIT_CONTEXT(id, name, mode) \
    test_annotation_init_context(id, name, mode)

#define TEST_ANNOTATION_SET_MODE(context_id, mode) \
    test_annotation_set_mode(context_id, mode)

#define TEST_ANNOTATION_EIGHT_ARGS(a, b, c, d, e, f, g, h) \
    test_annotation_eight_args(a, b, c, d, e, f, g, h)

#define TEST_ANNOTATION_NINE_ARGS(a, b, c, d, e, f, g, h, i) \
    test_annotation_nine_args(a, b, c, d, e, f, g, h, i)

#define TEST_ANNOTATION_TEN_ARGS(a, b, c, d, e, f, g, h, i, j) \
    test_annotation_ten_args(a, b, c, d, e, f, g, h, i, j)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
__declspec(dllexport) void __fastcall
test_annotation_init_mode(unsigned int mode);

__declspec(dllexport) void __fastcall
test_annotation_init_context(unsigned int id, const char *name,
    unsigned int initial_mode);

__declspec(dllexport) void __fastcall
test_annotation_set_mode(unsigned int context_id, unsigned int mode);

__declspec(dllexport) void __fastcall
test_annotation_eight_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h);

__declspec(dllexport) void __fastcall
test_annotation_nine_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i);

__declspec(dllexport) void __fastcall
test_annotation_ten_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i, unsigned int j);
#else
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_init_mode(unsigned int mode)
    __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_init_context(unsigned int id, const char *name, unsigned int initial_mode)
    __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_set_mode(unsigned int context_id, unsigned int mode)
    __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_eight_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h)
    __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_nine_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i)
    __attribute__ ((weak));

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_ten_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i, unsigned int j)
    __attribute__ ((weak));
# endif

#ifdef __cplusplus
}
#endif

#endif

