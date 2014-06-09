#ifndef _TEST_CUSTOM_ANNOTATIONS_H_
#define _TEST_CUSTOM_ANNOTATIONS_H_ 1

#define TEST_ANNOTATION_INIT_MODE(mode) \
    test_annotation_init_mode(mode)

#define TEST_ANNOTATION_INIT_CONTEXT(id, name, mode) \
    test_annotation_init_context(id, name, mode)

#define TEST_ANNOTATION_SET_MODE(context_id, mode) \
    test_annotation_set_mode(context_id, mode)

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
# endif

#ifdef __cplusplus
}
#endif

#endif

