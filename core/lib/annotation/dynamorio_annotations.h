#ifndef _DYNAMORIO_ANNOTATIONS_H_
#define _DYNAMORIO_ANNOTATIONS_H_ 1

#ifdef UNIX
# define DYNAMORIO_ANNOTATE_WEAK_ATTRIBUTE __attribute__ ((weak))
#else
# define DYNAMORIO_ANNOTATE_WEAK_ATTRIBUTE
#endif

typedef char bool;

#define true (1)
#define false (0)

#define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO \
    dynamorio_annotate_running_on_dynamorio

bool
dynamorio_annotate_running_on_dynamorio() DYNAMORIO_ANNOTATE_WEAK_ATTRIBUTE;

#endif
