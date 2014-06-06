#ifndef _DYNAMORIO_ANNOTATIONS_H_
#define _DYNAMORIO_ANNOTATIONS_H_ 1

#define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO \
    dynamorio_annotate_running_on_dynamorio

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
__declspec(dllexport) char __fastcall
dynamorio_annotate_running_on_dynamorio();
#else
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
char
dynamorio_annotate_running_on_dynamorio() __attribute__ ((weak));
#endif

#ifdef __cplusplus
}
#endif

#endif
