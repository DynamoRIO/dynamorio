#ifndef _DYNAMORIO_ANNOTATIONS_H_
#define _DYNAMORIO_ANNOTATIONS_H_ 1

typedef enum _bool {
    false,
    true
} bool;

#define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO \
    dynamorio_annotate_running_on_dynamorio

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
__declspec(dllexport) bool __fastcall
dynamorio_annotate_running_on_dynamorio();
#else
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
bool
dynamorio_annotate_running_on_dynamorio() __attribute__ ((weak));
#endif

#ifdef __cplusplus
}
#endif

#endif
