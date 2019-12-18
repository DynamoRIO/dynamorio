/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from Android headers to make
 ***   information necessary for a loader to call into the Bionic
 ***   C library available to DynamoRIO.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _ANDROID_LINKER_H_
#define _ANDROID_LINKER_H_ 1

#include <unistd.h>
#include "module_private.h"

/* i#1701: our loader must emulate what Android's dynamic linker does
 * when initializing the initial internal pthread data struct and passing
 * kernel arguments to Bionic.
 *
 * These definitions are from Android's pthread_internal.h,
 * pthread_types.h, pthread.h, and KernelArgumentBlock.h.
 * The type names are changed to our equivalents.
 */

typedef struct {
    uint flags;
    void *stack_base;
    size_t stack_size;
    size_t guard_size;
    int sched_policy;
    int sched_priority;
#ifdef __LP64__
    char __reserved[16];
#endif
} android_pthread_attr_t;

typedef struct {
#if defined(__LP64__)
    int __private[10];
#else
    int __private[1];
#endif
} android_pthread_mutex_t;

enum android_join_state_t {
    ANDROID_THREAD_NOT_JOINED,
    ANDROID_THREAD_EXITED_NOT_JOINED,
    ANDROID_THREAD_JOINED,
    ANDROID_THREAD_DETACHED
};

enum {
    ANDROID_TLS_SLOT_SELF = 0,
    ANDROID_TLS_SLOT_THREAD_ID,
    ANDROID_TLS_SLOT_ERRNO,
    ANDROID_TLS_SLOT_OPENGL_API = 3,
    ANDROID_TLS_SLOT_OPENGL = 4,
    ANDROID_TLS_SLOT_BIONIC_PREINIT = ANDROID_TLS_SLOT_OPENGL_API,
    ANDROID_TLS_SLOT_STACK_GUARD = 5,
    ANDROID_TLS_SLOT_DLERROR,
    ANDROID_BIONIC_TLS_SLOTS
};

#define ANDROID_RESERVED_KEYS 12
#define ANDROID_PTHREAD_KEYS_MAX 128
#define ANDROID_PTHREAD_KEYS_TOT (ANDROID_RESERVED_KEYS + ANDROID_PTHREAD_KEYS_MAX)
#define ANDROID_DLERROR_BUFFER_SIZE 512

typedef struct _android_v5_pthread_internal_t {
    struct _pthread_internal_t *next;
    struct _pthread_internal_t *prev;
    pid_t tid;
    pid_t cached_pid_;
    android_pthread_attr_t attr;
    int /* really std::atomic<ThreadJoinState> */ join_state;
    void *cleanup_stack;
    void *(*start_routine)(void *);
    void *start_routine_arg;
    void *return_value;
    void *alternate_signal_stack;
    android_pthread_mutex_t startup_handshake_mutex;
    size_t mmap_size;
    /* The TLS register points here, to slot #0 (ANDROID_TLS_SLOT_SELF) */
    void *tls[ANDROID_BIONIC_TLS_SLOTS];
    int /* really pthread_key_t */ pthread_keys[ANDROID_PTHREAD_KEYS_TOT];
    /* This is our added field. */
    void *dr_tls_base;
} android_v5_pthread_internal_t;

typedef struct _android_v6_pthread_internal_t {
    struct _pthread_internal_t *next;
    struct _pthread_internal_t *prev;
    pid_t tid;
    pid_t cached_pid_;
    android_pthread_attr_t attr;
    int /* really std::atomic<ThreadJoinState> */ join_state;
    void *cleanup_stack;
    void *(*start_routine)(void *);
    void *start_routine_arg;
    void *return_value;
    void *alternate_signal_stack;
    android_pthread_mutex_t startup_handshake_mutex;
    size_t mmap_size;
    void *thread_local_dtors;
    /* The TLS register points here, to slot #0 (ANDROID_TLS_SLOT_SELF) */
    void *tls[ANDROID_BIONIC_TLS_SLOTS];
    int /* really pthread_key_t */ pthread_keys[ANDROID_PTHREAD_KEYS_TOT];
    char dlerror_buffer[ANDROID_DLERROR_BUFFER_SIZE];
    /* This is our added field. */
    void *dr_tls_base;
} android_v6_pthread_internal_t;

/* Adapated from class KernelArgumentBlock */
typedef struct _android_kernel_args_t {
    int argc;
    char **argv;
    char **envp;
    ELF_AUXV_TYPE *auxv;
    char *abort_message_ptr;
} android_kernel_args_t;

#endif /* _ANDROID_LINKER_H_ */
