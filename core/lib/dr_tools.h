/* **********************************************************
 * Copyright (c) 2010-2022, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _DR_TOOLS_H_
#define _DR_TOOLS_H_ 1

/**************************************************
 * TOP-LEVEL ROUTINES
 */
/**
 * @file dr_tools.h
 * @brief Main API routines, including transparency support.
 */

DR_API
/**
 * Creates a DR context that can be used in a standalone program.
 * \warning This context cannot be used as the drcontext for a thread
 * running under DR control!  It is only for standalone programs that
 * wish to use DR as a library of disassembly, etc. routines.
 * \return NULL on failure, such as running on an unsupported operating
 * system version.
 */
void *
dr_standalone_init(void);

DR_API
/**
 * Restores application state modified by dr_standalone_init(), which can
 * include some signal handlers.
 */
void
dr_standalone_exit(void);

#ifndef GLOBAL_DCONTEXT
/**
 * Use this dcontext for use with the standalone static decoder library.
 * Pass it whenever a decoding-related API routine asks for a context.
 */
#    define GLOBAL_DCONTEXT ((void *)-1)
#endif

/**************************************************
 * UTILITY ROUTINES
 */

#ifdef WINDOWS
/**
 * If \p x is false, displays a message about an assertion failure
 * (appending \p msg to the message) and then calls dr_abort()
 */
#    define DR_ASSERT_MSG(x, msg)                                                   \
        ((void)((!(x)) ? (dr_messagebox("ASSERT FAILURE: %s:%d: %s (%s)", __FILE__, \
                                        __LINE__, #x, msg),                         \
                          dr_abort(), 0)                                            \
                       : 0))
#else
#    define DR_ASSERT_MSG(x, msg)                                                \
        ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)\n", \
                                     __FILE__, __LINE__, #x, msg),               \
                          dr_abort(), 0)                                         \
                       : 0))
#endif

/**
 * If \p x is false, displays a message about an assertion failure and
 * then calls dr_abort()
 */
#define DR_ASSERT(x) DR_ASSERT_MSG(x, "")

DR_API
/** Returns true if all DynamoRIO caches are thread private. */
bool
dr_using_all_private_caches(void);

DR_API
/** \deprecated Replaced by dr_set_process_exit_behavior() */
void
dr_request_synchronized_exit(void);

DR_API
/**
 * Returns the client-specific option string specified at client
 * registration.  \p client_id is the client ID passed to dr_client_main().
 *
 * \deprecated This routine is replaced by dr_client_main()'s arguments and
 * by dr_get_option_array().
 * The front-end \p drrun and other utilities now re-quote all tokens,
 * providing simpler option passing without escaping or extra quote layers.
 * This routine, for compatibility, strips those quotes off and returns
 * a flat string without any token-delimiting quotes.
 */
const char *
dr_get_options(client_id_t client_id);

DR_API
/**
 * Returns the client-specific option string specified at client
 * registration, parsed into an array of \p argc separate option tokens
 * stored in \p argv.  This is the same array of arguments passed
 * to the dr_client_main() routine.
 */
bool
dr_get_option_array(client_id_t client_id, int *argc OUT, const char ***argv OUT);

DR_API
/**
 * Read the value of a string DynamoRIO runtime option named \p option_name into
 * \p buf.  Options are listed in \ref sec_options.  DynamoRIO has many other
 * undocumented options which may be queried through this API, but they are not
 * officially supported.  The option value is truncated to \p len bytes and
 * null-terminated.
 * \return false if no option named \p option_name exists, and true otherwise.
 */
bool
dr_get_string_option(const char *option_name, char *buf OUT, size_t len);

DR_API
/**
 * Read the value of an integer DynamoRIO runtime option named \p option_name
 * into \p val.  This includes boolean options.  Options are listed in \ref
 * sec_options.  DynamoRIO has many other undocumented options which may be
 * queried through this API, but they are not officially supported.
 * \warning Always pass a full uint64 for \p val even if the option is a smaller
 * integer to avoid overwriting nearby data.
 * \return false if no option named \p option_name exists, and true otherwise.
 */
bool
dr_get_integer_option(const char *option_name, uint64 *val OUT);

DR_API
/**
 * Returns the client library name and path that were originally specified
 * to load the library.  If the resulting string is longer than #MAXIMUM_PATH
 * it will be truncated.  \p client_id is the client ID passed to a client's
 * dr_client_main() function.
 */
const char *
dr_get_client_path(client_id_t client_id);

DR_API
/**
 * Returns the base address of the client library.  \p client_id is
 * the client ID passed to a client's dr_client_main() function.
 */
byte *
dr_get_client_base(client_id_t client_id);

DR_API
/**
 * Sets information presented to users in diagnostic messages.
 * Only one name is supported, regardless of how many clients are in use.
 * If this routine is called a second time, the new values supersede
 * the original.
 * The \p report_URL is meant to be a bug tracker location where users
 * should go to report errors in the client end-user tool.
 */
bool
dr_set_client_name(const char *name, const char *report_URL);

DR_API
/**
 * Sets the version string presented to users in diagnostic messages.
 * This has a maximum length of 96 characters; anything beyond that is
 * silently truncated.
 */
bool
dr_set_client_version_string(const char *version);

DR_API
/**
 * Returns the error code of the last failed API routine. Users should check whether or
 * not the API routine that they just called has failed prior to calling this function.
 *
 * \warning Not all API routines currently support the registering of an error code upon
 * their failure. Therefore, check the routine's documentation to see whether it supports
 * setting error codes.
 */
dr_error_code_t
dr_get_error_code(void *drcontext);

DR_API
/** Retrieves the current time. */
void
dr_get_time(dr_time_t *time);

DR_API
/**
 * Returns the number of milliseconds since Jan 1, 1601 (this is
 * the current UTC time).
 *
 * \note This is the Windows standard.  UNIX time functions typically
 * count from the Epoch (Jan 1, 1970).  The Epoch is 11644473600*1000
 * milliseconds after Jan 1, 1601.
 */
uint64
dr_get_milliseconds(void);

DR_API
/**
 * Returns the number of microseconds since Jan 1, 1601 (this is
 * the current UTC time).
 *
 * \note This is the Windows standard.  UNIX time functions typically
 * count from the Epoch (Jan 1, 1970).  The Epoch is 11644473600*1000*1000
 * microseconds after Jan 1, 1601.
 */
uint64
dr_get_microseconds(void);

DR_API
/**
 * Returns a pseudo-random number in the range [0..max).
 * The pseudo-random sequence can be repeated by passing the seed
 * used during a run to the next run via the -prng_seed runtime option.
 */
uint
dr_get_random_value(uint max);

DR_API
/**
 * Sets the seed used for dr_get_random_value().  Generally this would
 * only be called during client initialization.
 */
void
dr_set_random_seed(uint seed);

DR_API
/** Returns the seed used for dr_get_random_value(). */
uint
dr_get_random_seed(void);

DR_API
/**
 * Aborts the process immediately without any cleanup (i.e., the exit event
 * will not be called).
 */
void
dr_abort(void);

DR_API
/**
 * Aborts the process immediately without any cleanup (i.e., the exit event
 * will not be called) with the exit code \p exit_code.
 *
 * On Linux, only the bottom 8 bits of \p exit_code will be honored
 * for a normal exit.  If bits 9..16 are not all zero, DR will send an
 * unhandled signal of that signal number instead of performing a normal
 * exit.
 */
void
dr_abort_with_code(int exit_code);

DR_API
/**
 * Exits the process, first performing a full cleanup that will
 * trigger the exit event (dr_register_exit_event()).  The process
 * exit code is set to \p exit_code.
 *
 * On Linux, only the bottom 8 bits of \p exit_code will be honored
 * for a normal exit.  If bits 9..16 are not all zero, DR will send an
 * unhandled signal of that signal number instead of performing a normal
 * exit.
 *
 * \note Calling this from \p dr_client_main or from the primary thread's
 * initialization event is not guaranteed to always work, as DR may
 * invoke a thread exit event where a thread init event was never
 * called.  We recommend using dr_abort_ex() or waiting for full
 * initialization prior to use of this routine.
 */
void
dr_exit_process(int exit_code);

/** Indicates the type of memory dump for dr_create_memory_dump(). */
typedef enum {
    /**
     * A "livedump", or "ldmp", DynamoRIO's own custom memory dump format.
     * The ldmp format does not currently support specifying a context
     * for the calling thread, so it will always include the call frames
     * to dr_create_memory_dump().  The \p ldmp.exe tool can be used to
     * create a dummy process (using the \p dummy.exe executable) which
     * can then be attached to by the debugger (use a non-invasive attach)
     * in order to view the memory dump contents.
     *
     * \note Windows only.
     */
    DR_MEMORY_DUMP_LDMP = 0x0001,
} dr_memory_dump_flags_t;

/** Indicates the type of memory dump for dr_create_memory_dump(). */
typedef struct _dr_memory_dump_spec_t {
    /** The size of this structure.  Set this to sizeof(dr_memory_dump_spec_t). */
    size_t size;
    /** The type of memory dump requested. */
    dr_memory_dump_flags_t flags;
    /**
     * This field only applies to DR_MEMORY_DUMP_LDMP.  This string is
     * stored inside the ldmp as the reason for the dump.
     */
    const char *label;
    /**
     * This field only applies to DR_MEMORY_DUMP_LDMP.  This is an optional output
     * field that, if non-NULL, will be written with the path to the created file.
     */
    char *ldmp_path;
    /**
     * This field only applies to DR_MEMORY_DUMP_LDMP.  This is the maximum size,
     * in bytes, of ldmp_path.
     */
    size_t ldmp_path_size;
} dr_memory_dump_spec_t;

DR_API
/**
 * Requests that DR create a memory dump file of the current process.
 * The type of dump is specified by \p spec.
 *
 * \return whether successful.
 *
 * \note this function is only supported on Windows for now.
 */
bool
dr_create_memory_dump(dr_memory_dump_spec_t *spec);

/**************************************************
 * APPLICATION-INDEPENDENT MEMORY ALLOCATION
 */

/** Flags used with dr_custom_alloc() */
typedef enum {
    /**
     * If this flag is not specified, dr_custom_alloc() uses a managed
     * heap to allocate the memory, just like dr_thread_alloc() or
     * dr_global_alloc().  In that case, it ignores any requested
     * protection bits (\p prot parameter), and the location (\p addr
     * parameter) must be NULL.  If this flag is specified, a
     * page-aligned, separate block of memory is allocated, in a
     * similar fashion to dr_nonheap_alloc().
     */
    DR_ALLOC_NON_HEAP = 0x0001,
    /**
     * This flag only applies to heap memory (i.e., when
     * #DR_ALLOC_NON_HEAP is not specified).  If this flag is not
     * specified, global heap is used (just like dr_global_alloc())
     * and the \p drcontext parameter is ignored.  If it is specified,
     * thread-private heap specific to \p drcontext is used, just like
     * dr_thread_alloc().
     */
    DR_ALLOC_THREAD_PRIVATE = 0x0002,
    /**
     * Allocate memory that is 32-bit-displacement reachable from the
     * code caches and from the client library.  Memory allocated
     * through dr_thread_alloc(), dr_global_alloc(), and
     * dr_nonheap_alloc() is also reachable, but for
     * dr_custom_alloc(), the resulting memory is not reachable unless
     * this flag is specified.  If this flag is passed, the requested
     * location (\p addr parameter) must be NULL.  This flag is not
     * compatible with #DR_ALLOC_LOW_2GB, #DR_ALLOC_FIXED_LOCATION, or
     * #DR_ALLOC_NON_DR.
     */
    DR_ALLOC_CACHE_REACHABLE = 0x0004,
    /**
     * This flag only applies to non-heap memory (i.e., when
     * #DR_ALLOC_NON_HEAP is specified).  The flag requests that
     * memory be allocated at a specific address, given in the \p addr
     * parameter.  Without this flag, the \p addr parameter is not
     * honored.  This flag is not compatible with #DR_ALLOC_LOW_2GB or
     * #DR_ALLOC_CACHE_REACHABLE.
     */
    DR_ALLOC_FIXED_LOCATION = 0x0008,
    /**
     * This flag only applies to non-heap memory (i.e., when
     * #DR_ALLOC_NON_HEAP is specified) in 64-bit mode.  The flag
     * requests that memory be allocated in the low 2GB of the address
     * space.  If this flag is passed, the requested location (\p addr
     * parameter) must be NULL.  This flag is not compatible with
     * #DR_ALLOC_FIXED_LOCATION.
     */
    DR_ALLOC_LOW_2GB = 0x0010,
    /**
     * This flag only applies to non-heap memory (i.e., when
     * #DR_ALLOC_NON_HEAP is specified).  When this flag is specified,
     * the allocated memory is not considered to be DynamoRIO or tool
     * memory and thus is not kept separate from the application.
     * This is similar to dr_raw_mem_alloc().  Use of this memory is
     * at the client's own risk.  This flag is not compatible with
     * #DR_ALLOC_CACHE_REACHABLE.
     */
    DR_ALLOC_NON_DR = 0x0020,
#ifdef WINDOWS
    /**
     * This flag only applies to non-heap, non-DR memory (i.e., when
     * both #DR_ALLOC_NON_HEAP and #DR_ALLOC_NON_DR are specified) on
     * Windows.  When this flag is specified, the allocated memory is
     * reserved but not committed, just like the MEM_RESERVE Windows API
     * flag (the default is MEM_RESERVE|MEM_COMMIT).
     */
    DR_ALLOC_RESERVE_ONLY = 0x0040,
    /**
     * This flag only applies to non-heap, non-DR memory (i.e., when both
     * #DR_ALLOC_NON_HEAP and #DR_ALLOC_NON_DR are specified) on Windows.
     * This flag must be combined with DR_ALLOC_FIXED_LOCATION.  When this
     * flag is specified, previously allocated memory is committed, just
     * like the MEM_COMMIT Windows API flag (when this flag is not passed,
     * the effect is MEM_RESERVE|MEM_COMMIT).  When passed to
     * dr_custom_free(), this flag causes a de-commit, just like the
     * MEM_DECOMMIT Windows API flag.  This flag cannot be combined with
     * #DR_ALLOC_LOW_2GB and must include a non-NULL requested location (\p
     * addr parameter).
     */
    DR_ALLOC_COMMIT_ONLY = 0x0080,
#endif
} dr_alloc_flags_t;

DR_API
/**
 * Allocates \p size bytes of memory from DR's memory pool specific to the
 * thread associated with \p drcontext.
 * This memory is only guaranteed to be aligned to the pointer size:
 * 8 byte alignment for 64-bit; 4-byte alignment for 32-bit.
 * (The wrapped malloc() guarantees the more standard double-pointer-size.)
 */
void *
dr_thread_alloc(void *drcontext, size_t size);

DR_API
/**
 * Frees thread-specific memory allocated by dr_thread_alloc().
 * \p size must be the same as that passed to dr_thread_alloc().
 */
void
dr_thread_free(void *drcontext, void *mem, size_t size);

DR_API
/**
 * Allocates \p size bytes of memory from DR's global memory pool.
 * This memory is only guaranteed to be aligned to the pointer size:
 * 8 byte alignment for 64-bit; 4-byte alignment for 32-bit.
 * (The wrapped malloc() guarantees the more standard double-pointer-size.)
 */
void *
dr_global_alloc(size_t size);

DR_API
/**
 * Frees memory allocated by dr_global_alloc().
 * \p size must be the same as that passed to dr_global_alloc().
 */
void
dr_global_free(void *mem, size_t size);

DR_API
/**
 * Allocates memory with the properties requested by \p flags.
 *
 * If \p addr is non-NULL (only allowed with certain flags), it must
 * be page-aligned.
 *
 * To make more space available for the code caches when running
 * larger applications, or for clients that use a lot of heap memory
 * that is not directly referenced from the cache, we recommend that
 * dr_custom_alloc() be called to obtain memory that is not guaranteed
 * to be reachable from the code cache (by not passing
 * #DR_ALLOC_CACHE_REACHABLE).  This frees up space in the reachable
 * region.
 *
 * Returns NULL on failure.
 */
void *
dr_custom_alloc(void *drcontext, dr_alloc_flags_t flags, size_t size, uint prot,
                void *addr);

DR_API
/**
 * Frees memory allocated by dr_custom_alloc().  The same \p flags
 * and \p size must be passed here as were passed to dr_custom_alloc().
 */
bool
dr_custom_free(void *drcontext, dr_alloc_flags_t flags, void *addr, size_t size);

DR_API
/**
 * Allocates \p size bytes of memory as a separate allocation from DR's
 * heap, allowing for separate protection.
 * The \p prot protection should use the DR_MEMPROT_READ,
 * DR_MEMPROT_WRITE, and DR_MEMPROT_EXEC bits.
 * When creating a region to hold dynamically generated code, use
 * this routine in order to create executable memory.
 */
void *
dr_nonheap_alloc(size_t size, uint prot);

DR_API
/**
 * Frees memory allocated by dr_nonheap_alloc().
 * \p size must be the same as that passed to dr_nonheap_alloc().
 */
void
dr_nonheap_free(void *mem, size_t size);

DR_API
/**
 * \warning This raw memory allocation interface is in flux and is subject to
 * change in the next release.  Consider it experimental in this release.
 *
 * Allocates \p size bytes (page size aligned) of memory as a separate
 * allocation at preferred base \p addr that must be page size aligned,
 * allowing for separate protection.
 * If \p addr is NULL, an arbitrary address is picked.
 *
 * The \p prot protection should use the DR_MEMPROT_READ,
 * DR_MEMPROT_WRITE, and DR_MEMPROT_EXEC bits.
 * The allocated memory is not considered to be DynamoRIO or tool memory and
 * thus is not kept separate from the application. Use of this memory is at the
 * client's own risk.
 *
 * The resulting memory is guaranteed to be initialized to all zeroes.
 *
 * Returns the actual address allocated or NULL if memory allocation at
 * preferred base fails.
 */
void *
dr_raw_mem_alloc(size_t size, uint prot, void *addr);

DR_API
/**
 * Frees memory allocated by dr_raw_mem_alloc().
 * \p addr and \p size must be the same as that passed to dr_raw_mem_alloc()
 * on Windows.
 */
bool
dr_raw_mem_free(void *addr, size_t size);

#ifdef LINUX
DR_API
/**
 * Calls mremap with the specified parameters and returns the result.
 * The old memory must be non-DR memory, and the new memory is also
 * considered to be non-DR memory (see #DR_ALLOC_NON_DR).
 * \note Linux-only.
 */
void *
dr_raw_mremap(void *old_address, size_t old_size, size_t new_size, int flags,
              void *new_address);

DR_API
/**
 * Sets the program break to the specified value.  Invokes the SYS_brk
 * system call and returns the result.  This is the application's
 * program break, so use this system call only when deliberately
 * changing the application's behavior.
 * \note Linux-only.
 */
void *
dr_raw_brk(void *new_address);
#endif

DR_API
/**
 * Allocates memory from DR's global memory pool, but mimics the
 * behavior of malloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option to
 * replace a client's use of malloc, realloc, and free with internal
 * versions that allocate memory from DR's private pool.  With -wrap,
 * clients can link to libraries that allocate heap memory without
 * interfering with application allocations.
 * The returned address is guaranteed to be double-pointer-aligned:
 * aligned to 16 bytes for 64-bit; aligned to 8 bytes for 32-bit.
 */
void *
__wrap_malloc(size_t size);

DR_API
/**
 * Reallocates memory from DR's global memory pool, but mimics the
 * behavior of realloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option; see
 * __wrap_malloc() for more information.
 * The returned address is guaranteed to be double-pointer-aligned:
 * aligned to 16 bytes for 64-bit; aligned to 8 bytes for 32-bit.
 */
void *
__wrap_realloc(void *mem, size_t size);

DR_API
/**
 * Allocates memory from DR's global memory pool, but mimics the
 * behavior of calloc.  Memory must be freed with __wrap_free().  The
 * __wrap routines are intended to be used with ld's -wrap option; see
 * __wrap_malloc() for more information.
 * The returned address is guaranteed to be double-pointer-aligned:
 * aligned to 16 bytes for 64-bit; aligned to 8 bytes for 32-bit.
 */
void *
__wrap_calloc(size_t nmemb, size_t size);

DR_API
/**
 * Frees memory from DR's global memory pool.  Memory must have been
 * allocated with __wrap_malloc(). The __wrap routines are intended to
 * be used with ld's -wrap option; see __wrap_malloc() for more
 * information.
 */
void
__wrap_free(void *mem);

DR_API
/**
 * Allocates memory for a new string identical to 'str' and copies the
 * contents of 'str' into the new string, including a terminating
 * null.  Memory must be freed with __wrap_free().  The __wrap
 * routines are intended to be used with ld's -wrap option; see
 * __wrap_malloc() for more information.
 * The returned address is guaranteed to be double-pointer-aligned:
 * aligned to 16 bytes for 64-bit; aligned to 8 bytes for 32-bit.
 */
char *
__wrap_strdup(const char *str);

/**************************************************
 * LOCK SUPPORT
 */

DR_API
/**
 * Initializes a mutex.
 *
 * Warning: there are restrictions on when DR-provided mutexes, and
 * locks in general, can be held by a client: no lock should be held
 * while application code is executing in the code cache.  Locks can
 * be used while inside client code reached from clean calls out of
 * the code cache, but they must be released before returning to the
 * cache.  A lock must also be released by the same thread that acquired
 * it.  Failing to follow these restrictions can lead to deadlocks.
 */
void *
dr_mutex_create(void);

DR_API
/** Deletes \p mutex. */
void
dr_mutex_destroy(void *mutex);

DR_API
/** Locks \p mutex.  Waits until the mutex is successfully held. */
void
dr_mutex_lock(void *mutex);

DR_API
/**
 * Unlocks \p mutex.  Asserts that mutex is currently locked by the
 * current thread.
 */
void
dr_mutex_unlock(void *mutex);

DR_API
/** Tries once to lock \p mutex and returns whether or not successful. */
bool
dr_mutex_trylock(void *mutex);

DR_API
/**
 * Returns true iff \p mutex is owned by the calling thread.
 * This routine is only available in debug builds.
 * In release builds it always returns true.
 */
bool
dr_mutex_self_owns(void *mutex);

DR_API
/**
 * Instructs DR to treat this lock as an application lock.  Primarily
 * this avoids debug-build checks that no DR locks are held in situations
 * where locks are disallowed.
 *
 * \warning Any one lock should either be a DR lock or an application lock.
 * Use this routine with caution and do not call it on a DR lock that is
 * used in DR contexts, as it disables debug checks.
 *
 * \warning This routine is not sufficient on its own to prevent deadlocks
 * during scenarios where DR wants to suspend all threads such as detach or
 * relocation. See dr_app_recurlock_lock() and dr_mark_safe_to_suspend().
 *
 * \return whether successful.
 */
bool
dr_mutex_mark_as_app(void *mutex);

DR_API
/**
 * Creates and initializes a read-write lock.  A read-write lock allows
 * multiple readers or alternatively a single writer.  The lock
 * restrictions for mutexes apply (see dr_mutex_create()).
 */
void *
dr_rwlock_create(void);

DR_API
/** Deletes \p rwlock. */
void
dr_rwlock_destroy(void *rwlock);

DR_API
/** Acquires a read lock on \p rwlock. */
void
dr_rwlock_read_lock(void *rwlock);

DR_API
/** Releases a read lock on \p rwlock. */
void
dr_rwlock_read_unlock(void *rwlock);

DR_API
/** Acquires a write lock on \p rwlock. */
void
dr_rwlock_write_lock(void *rwlock);

DR_API
/** Releases a write lock on \p rwlock. */
void
dr_rwlock_write_unlock(void *rwlock);

DR_API
/** Tries once to acquire a write lock on \p rwlock and returns whether successful. */
bool
dr_rwlock_write_trylock(void *rwlock);

DR_API
/** Returns whether the calling thread owns the write lock on \p rwlock. */
bool
dr_rwlock_self_owns_write_lock(void *rwlock);

DR_API
/**
 * Instructs DR to treat this lock as an application lock.  Primarily
 * this avoids debug-build checks that no DR locks are held in situations
 * where locks are disallowed.
 *
 * \warning Any one lock should either be a DR lock or an application lock.
 * Use this routine with caution and do not call it on a DR lock that is
 * used in DR contexts, as it disables debug checks.
 *
 * \return whether successful.
 */
bool
dr_rwlock_mark_as_app(void *rwlock);

DR_API
/**
 * Creates and initializes a recursive lock.  A recursive lock allows
 * the same thread to acquire it multiple times.  The lock
 * restrictions for mutexes apply (see dr_mutex_create()).
 */
void *
dr_recurlock_create(void);

DR_API
/** Deletes \p reclock. */
void
dr_recurlock_destroy(void *reclock);

DR_API
/** Acquires \p reclock, or increments the ownership count if already owned. */
void
dr_recurlock_lock(void *reclock);

DR_API
/**
 * Acquires \p reclock, or increments the ownership count if already owned.
 * Calls to this method which block (i.e. when the lock is already held) are
 * marked safe to suspend AND transfer; in that case the provided mcontext \p mc
 * will overwrite the current thread's mcontext. \p mc must have a valid PC
 * and its flags must be DR_MC_ALL.
 *
 * This routine must be used in clients holding application locks to prevent
 * deadlocks in a way similar to dr_mark_safe_to_suspend(), but this routine
 * is intended to be called by a clean call and may return execution to the
 * provided mcontext rather than returning normally.
 *
 * If this routine is called from a clean call, callers should not return
 * normally. Instead, dr_redirect_execution() or dr_redirect_native_target()
 * should be called to to prevent a return into a flushed code page.
 */
void
dr_app_recurlock_lock(void *reclock, dr_mcontext_t *mc);

DR_API
/** Decrements the ownership count of \p reclock and releases if zero. */
void
dr_recurlock_unlock(void *reclock);

DR_API
/** Tries once to acquire \p reclock and returns whether successful. */
bool
dr_recurlock_trylock(void *reclock);

DR_API
/** Returns whether the calling thread owns \p reclock. */
bool
dr_recurlock_self_owns(void *reclock);

DR_API
/**
 * Instructs DR to treat this lock as an application lock.  Primarily
 * this avoids debug-build checks that no DR locks are held in situations
 * where locks are disallowed.
 *
 * \warning Any one lock should either be a DR lock or an application lock.
 * Use this routine with caution and do not call it on a DR lock that is
 * used in DR contexts, as it disables debug checks.
 *
 * \return whether successful.
 */
bool
dr_recurlock_mark_as_app(void *reclock);

DR_API
/** Creates an event object on which threads can wait and be signaled. */
void *
dr_event_create(void);

DR_API
/** Destroys an event object. */
bool
dr_event_destroy(void *event);

DR_API
/** Suspends the current thread until \p event is signaled. */
bool
dr_event_wait(void *event);

DR_API
/** Wakes up at most one thread waiting on \p event. */
bool
dr_event_signal(void *event);

DR_API
/** Resets \p event to no longer be in a signaled state. */
bool
dr_event_reset(void *event);

DR_API
/**
 * Use this function to mark a region of code as safe for DR to suspend
 * the client while inside the region.  DR will not relocate the client
 * from the region and will resume it at precisely the suspend point.
 *
 * This function must be used in client code that acquires application locks.
 * Use this feature with care!  Do not mark code as safe to suspend that has
 * a code cache return point.  I.e., do not call this routine from a clean
 * call. For acquiring application locks from a clean call, see
 * dr_app_recurlock_lock().
 *
 * No DR locks can be held while in a safe region.  Consequently, do
 * not call this routine from any DR event callback.  It may only be used
 * from natively executing code.
 *
 * Always invoke this routine in pairs, with the first passing true
 * for \p enter and the second passing false, thus delimiting the
 * region.
 */
bool
dr_mark_safe_to_suspend(void *drcontext, bool enter);

DR_API
/**
 * Atomically adds \p val to \p *dest and returns the sum.
 * \p dest must not straddle two cache lines.
 */
int
dr_atomic_add32_return_sum(volatile int *dest, int val);

#ifdef X64
DR_API
/**
 * Atomically adds \p val to \p *dest and returns the sum.
 * \p dest must not straddle two cache lines.
 * Currently 64-bit-build only.
 */
int64
dr_atomic_add64_return_sum(volatile int64 *dest, int64 val);
#endif

DR_API
/** Atomically and visibly loads the value at \p src and returns it. */
int
dr_atomic_load32(volatile int *src);

DR_API
/** Atomically and visibly stores \p val to \p dest. */
void
dr_atomic_store32(volatile int *dest, int val);

#ifdef X64
DR_API
/**
 * Atomically and visibly loads the value at \p src and returns it.
 * Currently 64-bit-build only.
 */
int64
dr_atomic_load64(volatile int64 *src);

DR_API
/**
 * Atomically and visibly stores \p val to \p dest.
 * Currently 64-bit-build only.
 */
void
dr_atomic_store64(volatile int64 *dest, int64 val);
#endif

/** Flags for use with dr_map_executable_file(). */
typedef enum {
    /**
     * Requests that writable segments are not mapped, to save address space.
     * This may be ignored on some platforms and may only be honored for
     * a writable segment that is at the very end of the loaded module.
     */
    DR_MAPEXE_SKIP_WRITABLE = 0x0002,
} dr_map_executable_flags_t;

DR_API
/**
 * Loads \p filename as an executable file for examination, rather
 * than for execution.  No entry point, initialization, or constructor
 * code is executed, nor is any thread-local storage or other
 * resources set up.  Returns the size (which may include unmappped
 * gaps) in \p size.  The return value of the function is the base
 * address at which the file is mapped.
 *
 * \note Not currently supported on Mac OSX.
 */
byte *
dr_map_executable_file(const char *filename, dr_map_executable_flags_t flags,
                       size_t *size OUT);

DR_API
/**
 * Unmaps a file loaded by dr_map_executable_file().
 */
bool
dr_unmap_executable_file(byte *base, size_t size);

/**************************************************
 * SYSTEM CALL PROCESSING ROUTINES
 */

/**
 * Data structure used to obtain or modify the result of an application
 * system call by dr_syscall_get_result_ex() and dr_syscall_set_result_ex().
 */
typedef struct _dr_syscall_result_info_t {
    /** The caller should set this to the size of the structure. */
    size_t size;
    /**
     * Indicates whether the system call succeeded or failed.  For
     * dr_syscall_set_result_ex(), this requests that DR set any
     * additional machine state, if any, used by the particular
     * plaform that is not part of \p value to indicate success or
     * failure (e.g., on MacOS the carry flag is used to indicate
     * success).
     *
     * For Windows, the success result from dr_syscall_get_result_ex()
     * should only be relied upon for ntoskrnl system calls.  For
     * other Windows system calls (such as win32k.sys graphical
     * (NtGdi) or user (NtUser) system calls), computing success
     * depends on each particular call semantics and is beyond the
     * scope of this routine (consider using the "drsyscall" Extension
     * instead).
     *
     * For Mach syscalls on MacOS, the success result from
     * dr_syscall_get_result_ex() should not be relied upon.
     * Computing success depends on each particular call semantics and
     * is beyond the scope of this routine (consider using the
     * "drsyscall" Extension instead).
     */
    bool succeeded;
    /**
     * The raw main value returned by the system call.
     * See also the \p high field.
     */
    reg_t value;
    /**
     * On some platforms (such as MacOS), a 32-bit application's
     * system call can return a 64-bit value.  For such calls,
     * this field will hold the top 32 bit bits, if requested
     * by \p use_high.  It is up to the caller to know which
     * system calls have 64-bit return values.  System calls that
     * return only 32-bit values do not clear the upper bits.
     * Consider using the "drsyscall" Extension in order to obtain
     * per-system-call semantic information, including return type.
     */
    reg_t high;
    /**
     * This should be set by the caller, and only applies to 32-bit
     * system calls.  For dr_syscall_get_result_ex(), this requests
     * that DR fill in the \p high field.  For
     * dr_syscall_set_result_ex(), this requests that DR set the high
     * 32 bits of the application-facing result to the value in the \p
     * high field.
     */
    bool use_high;
    /**
     * This should be set by the caller.  For dr_syscall_get_result_ex(),
     * this requests that DR fill in the \p errno_value field.
     * For dr_syscall_set_result_ex(), this requests that DR set the
     * \p value to indicate the particular error code in \p errno_value.
     */
    bool use_errno;
    /**
     * If requested by \p use_errno, if a system call fails (i.e., \p
     * succeeded is false) dr_syscall_get_result_ex() will set this
     * field to the absolute value of the error code returned (i.e.,
     * on Linux, it will be inverted from what the kernel directly
     * returns, in order to facilitate cross-platform clients that
     * operate on both Linux and MacOS).  For Linux and Macos, when
     * \p succeeded is true, \p errno_value is set to 0.
     *
     * If \p use_errno is set for dr_syscall_set_result_ex(), then
     * this value will be stored as the system call's return value,
     * negated if necessary for the underlying platform.  In that
     * case, \p value will be ignored.
     */
    uint errno_value;
} dr_syscall_result_info_t;

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event())
 * event.  Returns the value of system call parameter number \p param_num.
 *
 * It is up to the caller to ensure that reading this parameter is
 * safe: this routine does not know the number of parameters for each
 * system call, nor does it check whether this might read off the base
 * of the stack.
 *
 * \note On some platforms, notably MacOS, a 32-bit application's
 * system call can still take a 64-bit parameter (typically on the
 * stack).  In that situation, this routine will consider the 64-bit
 * parameter to be split into high and low parts, each with its own
 * parameter number.
 */
reg_t
dr_syscall_get_param(void *drcontext, int param_num);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event())
 * event, or from a post-syscall (dr_register_post_syscall_event())
 * event when also using dr_syscall_invoke_another().  Sets the value
 * of system call parameter number \p param_num to \p new_value.
 *
 * It is up to the caller to ensure that writing this parameter is
 * safe: this routine does not know the number of parameters for each
 * system call, nor does it check whether this might write beyond the
 * base of the stack.
 *
 * \note On some platforms, notably MacOS, a 32-bit application's
 * system call can still take a 64-bit parameter (typically on the
 * stack).  In that situation, this routine will consider the 64-bit
 * parameter to be split into high and low parts, each with its own
 * parameter number.
 */
void
dr_syscall_set_param(void *drcontext, int param_num, reg_t new_value);

DR_API
/**
 * Usable only from a post-syscall (dr_register_post_syscall_event())
 * event.  Returns the return value of the system call that will be
 * presented to the application.
 *
 * \note On some platforms (such as MacOS), a 32-bit application's
 * system call can return a 64-bit value.  Use dr_syscall_get_result_ex()
 * to obtain the upper bits in that case.
 *
 * \note On some platforms (such as MacOS), whether a system call
 * succeeded or failed cannot be determined from the main result
 * value.  Use dr_syscall_get_result_ex() to obtain the success result
 * in such cases.
 */
reg_t
dr_syscall_get_result(void *drcontext);

DR_API
/**
 * Usable only from a post-syscall (dr_register_post_syscall_event())
 * event.  Returns whether it successfully retrieved the results
 * of the system call into \p info.
 *
 * The caller should set the \p size, \p use_high, and \p use_errno fields
 * of \p info prior to calling this routine.
 * See the fields of #dr_syscall_result_info_t for details.
 */
bool
dr_syscall_get_result_ex(void *drcontext, dr_syscall_result_info_t *info INOUT);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event()) or
 * post-syscall (dr_register_post_syscall_event()) event.
 * For pre-syscall, should only be used when skipping the system call.
 * This sets the return value of the system call that the application sees
 * to \p value.
 *
 * \note On MacOS, do not use this function as it fails to set the
 * carry flag and thus fails to properly indicate whether the system
 * call succeeded or failed: use dr_syscall_set_result_ex() instead.
 */
void
dr_syscall_set_result(void *drcontext, reg_t value);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event()) or
 * post-syscall (dr_register_post_syscall_event()) event.
 * For pre-syscall, should only be used when skipping the system call.
 *
 * This sets the returned results of the system call as specified in
 * \p info.  Returns whether it successfully did so.
 * See the fields of #dr_syscall_result_info_t for details.
 */
bool
dr_syscall_set_result_ex(void *drcontext, dr_syscall_result_info_t *info);

DR_API
/**
 * Usable only from a pre-syscall (dr_register_pre_syscall_event())
 * event, or from a post-syscall (dr_register_post_syscall_event())
 * event when also using dr_syscall_invoke_another().  Sets the system
 * call number of the system call about to be invoked to \p new_num.
 */
void
dr_syscall_set_sysnum(void *drcontext, int new_num);

DR_API
/**
 * Usable only from a post-syscall (dr_register_post_syscall_event())
 * event.  An additional system call will be invoked immediately,
 * using the current values of the parameters, which can be set with
 * dr_syscall_set_param().  The system call to be invoked should be
 * specified with dr_syscall_set_sysnum().
 *
 * Use this routine with caution.  Especially on Windows, care must be
 * taken if the application is expected to continue afterward.  When
 * system call parameters are stored on the stack, modifying them can
 * result in incorrect application behavior, particularly when setting
 * more parameters than were present in the original system call,
 * which will result in corruption of the application stack.
 *
 * On Windows, when the first system call is interruptible
 * (alertable), the additional system call may be delayed.
 *
 * DR will set key registers such as r10 for 64-bit or xdx for
 * sysenter or WOW64 system calls.  However, DR will not set ecx for
 * WOW64; that is up to the client.
 */
void
dr_syscall_invoke_another(void *drcontext);

#ifdef WINDOWS
DR_API
/**
 * Must be invoked from dr_client_main().  Requests that the named ntoskrnl
 * system call be intercepted even when threads are native (e.g., due
 * to #DR_EMIT_GO_NATIVE).  Only a limited number of system calls
 * being intercepted while native are supported.  This routine will
 * fail once that limit is reached.
 *
 * @param[in] name      The system call name.  The name must match an exported
 *   system call wrapper in \p ntdll.dll.
 * @param[in] sysnum    The system call number (the value placed in the eax register).
 * @param[in] num_args  The number of arguments to the system call.
 * @param[in] wow64_index  The value placed in the ecx register when this system
 *   call is executed in a WOW64 process.  This value should be obtainable
 *   by examining the system call wrapper.
 *
 * \note Windows only.
 */
bool
dr_syscall_intercept_natively(const char *name, int sysnum, int num_args,
                              int wow64_index);
#endif

/**************************************************
 * PLATFORM-INDEPENDENT FILE SUPPORT
 *
 * Since a FILE cannot be used outside of the DLL it was created in,
 * we have to use HANDLE on Windows.
 * We hide the distinction behind the file_t type
 */

DR_API
/**
 * Creates a new directory.  Fails if the directory already exists
 * or if it can't be created.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_create_dir(const char *fname);

DR_API
/**
 * Deletes the given directory.  Fails if the directory is not empty.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_delete_dir(const char *fname);

DR_API
/**
 * Returns the current directory for this process in \p buf.
 * On Windows, reading the current directory is considered unsafe
 * except during initialization, as it is stored in user memory and
 * access is not controlled via any standard synchronization.
 */
bool
dr_get_current_directory(char *buf, size_t bufsz);

DR_API
/**
 * Checks for the existence of a directory.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_directory_exists(const char *fname);

DR_API
/**
 * Checks the existence of a file.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_file_exists(const char *fname);

/* flags for use with dr_open_file() */
/** Open with read access. */
#define DR_FILE_READ 0x1
/** Open with write access, but do not open if the file already exists. */
#define DR_FILE_WRITE_REQUIRE_NEW 0x2
/**
 * Open with write access.  If the file already exists, set the file position to the
 * end of the file.
 */
#define DR_FILE_WRITE_APPEND 0x4
/**
 * Open with write access.  If the file already exists, truncate the
 * file to zero length.
 */
#define DR_FILE_WRITE_OVERWRITE 0x8
/**
 * Open with large (>2GB) file support.  Only applicable on 32-bit Linux.
 * \note DR's log files and tracedump files are all created with this flag.
 */
#define DR_FILE_ALLOW_LARGE 0x10
/** Linux-only.  This file will be closed in the child of a fork. */
#define DR_FILE_CLOSE_ON_FORK 0x20
/**
 * Open with write-only access.  Meant for use with pipes.  Linux-only.
 * Mutually exclusive with DR_FILE_WRITE_REQUIRE_NEW, DR_FILE_WRITE_APPEND, and
 * DR_FILE_WRITE_OVERWRITE.
 */
#define DR_FILE_WRITE_ONLY 0x40

DR_API
/**
 * Opens the file \p fname. If no such file exists then one is created.
 * The file access mode is set by the \p mode_flags argument which is drawn from
 * the DR_FILE_* defines ORed together.  Returns INVALID_FILE if unsuccessful.
 *
 * On Windows, \p fname is safest as an absolute path (when using Windows system
 * calls directly there is no such thing as a relative path).  A relative path
 * passed to this routine will be converted to absolute on a best-effort basis
 * using the current directory that was set at process initialization time.
 * (The most recently set current directory can be retrieved (albeit with no
 * safety guarantees) with dr_get_current_directory().)  Drive-implied-absolute
 * paths ("\foo.txt") and other-drive-relative paths ("c:foo.txt") are not
 * supported.
 *
 * On Linux, the file descriptor will be marked as close-on-exec.  The
 * DR_FILE_CLOSE_ON_FORK flag can be used to automatically close a
 * file on a fork.
 *
 * \note No more then one write mode flag can be specified.
 *
 * \note On Linux, DR hides files opened by clients from the
 * application by using file descriptors that are separate from the
 * application's and preventing the application from closing
 * client-opened files.
 */
file_t
dr_open_file(const char *fname, uint mode_flags);

DR_API
/** Closes file \p f. */
void
dr_close_file(file_t f);

DR_API
/**
 * Renames the file \p src to \p dst, replacing an existing file named \p dst if
 * \p replace is true.
 * Atomic if \p src and \p dst are on the same filesystem.
 * Returns true if successful.
 */
bool
dr_rename_file(const char *src, const char *dst, bool replace);

DR_API
/**
 * Deletes the file referred to by \p filename.
 * Returns true if successful.
 * On both Linux and Windows, if filename refers to a symlink, the symlink will
 * be deleted and not the target of the symlink.
 * On Windows, this will fail to delete any file that was not opened with
 * FILE_SHARE_DELETE and is still open.
 * Relative path support on Windows is identical to that described in dr_open_file().
 */
bool
dr_delete_file(const char *filename);

DR_API
/** Flushes any buffers for file \p f. */
void
dr_flush_file(file_t f);

DR_API
/**
 * Writes \p count bytes from \p buf to file \p f.
 * Returns the actual number written.
 */
ssize_t
dr_write_file(file_t f, const void *buf, size_t count);

DR_API
/**
 * Reads up to \p count bytes from file \p f into \p buf.
 * Returns the actual number read.
 */
ssize_t
dr_read_file(file_t f, void *buf, size_t count);

/* NOTE - keep in synch with OS_SEEK_* in os_shared.h and SEEK_* from Linux headers.
 * The extra BEGIN END is to get spacing nice. Once we have more control over the
 * layout of the API header files share with os_shared.h. */
/* For use with dr_file_seek(), specifies the origin at which to apply the offset. */
#define DR_SEEK_SET 0 /**< start of file */
#define DR_SEEK_CUR 1 /**< current file position */
#define DR_SEEK_END 2 /**< end of file */

DR_API
/**
 * Sets the current file position for file \p f to \p offset bytes
 * from the specified origin, where \p origin is one of the DR_SEEK_*
 * values.  Returns true if successful.
 */
bool
dr_file_seek(file_t f, int64 offset, int origin);

DR_API
/**
 * Returns the current position for the file \p f in bytes from the start of the file.
 * Returns -1 on an error.
 */
int64
dr_file_tell(file_t f);

DR_API
/**
 * Returns a new copy of the file handle \p f.
 * Returns INVALID_FILE on error.
 */
file_t
dr_dup_file_handle(file_t f);

DR_API
/**
 * Determines the size of the file \p fd.
 * On success, returns the size in \p size.
 * \return whether successful.
 */
bool
dr_file_size(file_t fd, OUT uint64 *size);

/** Flags for use with dr_map_file(). */
enum {
    /**
     * If set, changes to mapped memory are private to the mapping process and
     * are not reflected in the underlying file.  If not set, changes are visible
     * to other processes that map the same file, and will be propagated
     * to the file itself.
     */
    DR_MAP_PRIVATE = 0x0001,
#ifdef UNIX
    /**
     * If set, indicates that the passed-in start address is required rather than a
     * hint.  On Linux, this has the same semantics as mmap with MAP_FIXED: i.e.,
     * any existing mapping in [addr,addr+size) will be unmapped.  This flag is not
     * supported on Windows.
     */
    DR_MAP_FIXED = 0x0002,
#endif
#ifdef WINDOWS
    /**
     * If set, loads the specified file as an executable image, rather than a data
     * file.  This flag is not supported on Linux.
     */
    DR_MAP_IMAGE = 0x0004,
#endif
    /**
     * If set, loads the specified file at a location that is reachable from
     * the code cache and client libraries by a 32-bit displacement.  If not
     * set, the mapped file is not guaranteed to be reachable from the cache.
     */
    DR_MAP_CACHE_REACHABLE = 0x0008,
};

DR_API
/**
 * Memory-maps \p size bytes starting at offset \p offs from the file \p f
 * at address \p addr with privileges \p prot.
 *
 * @param[in] f The file to map.
 * @param[in,out] size The requested size to map.  Upon successful return,
 *   contains the actual mapped size.
 * @param[in] offs The offset within the file at which to start the map.
 * @param[in] addr The requested start address of the map.  Unless \p fixed
 *   is true, this is just a hint and may not be honored.
 * @param[in] prot The access privileges of the mapping, composed of
 *   the DR_MEMPROT_READ, DR_MEMPROT_WRITE, and DR_MEMPROT_EXEC bits.
 * @param[in] flags Optional DR_MAP_* flags.
 *
 * \note Mapping image files for execution is not supported.
 *
 * \return the start address of the mapping, or NULL if unsuccessful.
 */
void *
dr_map_file(file_t f, INOUT size_t *size, uint64 offs, app_pc addr, uint prot,
            uint flags);

DR_API
/**
 * Unmaps a portion of a file mapping previously created by dr_map_file().
 * \return whether successful.
 *
 * @param[in]  map   The base address to be unmapped. Must be page size aligned.
 * @param[in]  size  The size to be unmapped.
 *                   All pages overlapping with the range are unmapped.
 *
 * \note On Windows, the whole file will be unmapped instead.
 *
 */
bool
dr_unmap_file(void *map, size_t size);

/* TODO add copy_file, truncate_file etc.
 * All should be easy though at some point should perhaps tell people to just use the raw
 * systemcalls, esp for linux where they're documented and let them provide their own
 * wrappers. */

/**************************************************
 * PRINTING
 */

DR_API
/**
 * Writes to DR's log file for the thread with drcontext \p drcontext if the current
 * loglevel is >= \p level and the current \p logmask & \p mask != 0.
 * The mask constants are the DR_LOG_* defines below.
 * Logging is disabled for the release build.
 * If \p drcontext is NULL, writes to the main log file.
 */
void
dr_log(void *drcontext, uint mask, uint level, const char *fmt, ...);

/* hack to get these constants in the right place in dr_ir_api.h */

/* The log mask constants */
#define DR_LOG_NONE 0x00000000     /**< Log no data. */
#define DR_LOG_STATS 0x00000001    /**< Log per-thread and global statistics. */
#define DR_LOG_TOP 0x00000002      /**< Log top-level information. */
#define DR_LOG_THREADS 0x00000004  /**< Log data related to threads. */
#define DR_LOG_SYSCALLS 0x00000008 /**< Log data related to system calls. */
#define DR_LOG_ASYNCH 0x00000010   /**< Log data related to signals/callbacks/etc. */
#define DR_LOG_INTERP 0x00000020   /**< Log data related to app interpretation. */
#define DR_LOG_EMIT 0x00000040     /**< Log data related to emitting code. */
#define DR_LOG_LINKS 0x00000080    /**< Log data related to linking code. */
#define DR_LOG_CACHE                                           \
    0x00000100 /**< Log data related to code cache management. \
                */
#define DR_LOG_FRAGMENT                                     \
    0x00000200 /**< Log data related to app code fragments. \
                */
#define DR_LOG_DISPATCH                                                           \
    0x00000400                    /**< Log data on every context switch dispatch. \
                                   */
#define DR_LOG_MONITOR 0x00000800 /**< Log data related to trace building. */
#define DR_LOG_HEAP 0x00001000    /**< Log data related to memory management. */
#define DR_LOG_VMAREAS 0x00002000 /**< Log data related to address space regions. */
#define DR_LOG_SYNCH 0x00004000   /**< Log data related to synchronization. */
#define DR_LOG_MEMSTATS                                                            \
    0x00008000                         /**< Log data related to memory statistics. \
                                        */
#define DR_LOG_OPTS 0x00010000         /**< Log data related to optimizations. */
#define DR_LOG_SIDELINE 0x00020000     /**< Log data related to sideline threads. */
#define DR_LOG_SYMBOLS 0x00040000      /**< Log data related to app symbols. */
#define DR_LOG_RCT 0x00080000          /**< Log data related to indirect transfers. */
#define DR_LOG_NT 0x00100000           /**< Log data related to Windows Native API. */
#define DR_LOG_HOT_PATCHING 0x00200000 /**< Log data related to hot patching. */
#define DR_LOG_HTABLE 0x00400000       /**< Log data related to hash tables. */
#define DR_LOG_MODULEDB 0x00800000     /**< Log data related to the module database. */
#define DR_LOG_ALL 0x00ffffff          /**< Log all data. */
#ifdef DR_LOG_DEFINE_COMPATIBILITY
#    define LOG_NONE DR_LOG_NONE         /**< Identical to #DR_LOG_NONE. */
#    define LOG_STATS DR_LOG_STATS       /**< Identical to #DR_LOG_STATS. */
#    define LOG_TOP DR_LOG_TOP           /**< Identical to #DR_LOG_TOP. */
#    define LOG_THREADS DR_LOG_THREADS   /**< Identical to #DR_LOG_THREADS. */
#    define LOG_SYSCALLS DR_LOG_SYSCALLS /**< Identical to #DR_LOG_SYSCALLS. */
#    define LOG_ASYNCH DR_LOG_ASYNCH     /**< Identical to #DR_LOG_ASYNCH. */
#    define LOG_INTERP DR_LOG_INTERP     /**< Identical to #DR_LOG_INTERP. */
#    define LOG_EMIT DR_LOG_EMIT         /**< Identical to #DR_LOG_EMIT. */
#    define LOG_LINKS DR_LOG_LINKS       /**< Identical to #DR_LOG_LINKS. */
#    define LOG_CACHE DR_LOG_CACHE       /**< Identical to #DR_LOG_CACHE. */
#    define LOG_FRAGMENT DR_LOG_FRAGMENT /**< Identical to #DR_LOG_FRAGMENT. */
#    define LOG_DISPATCH DR_LOG_DISPATCH /**< Identical to #DR_LOG_DISPATCH. */
#    define LOG_MONITOR DR_LOG_MONITOR   /**< Identical to #DR_LOG_MONITOR. */
#    define LOG_HEAP DR_LOG_HEAP         /**< Identical to #DR_LOG_HEAP. */
#    define LOG_VMAREAS DR_LOG_VMAREAS   /**< Identical to #DR_LOG_VMAREAS. */
#    define LOG_SYNCH DR_LOG_SYNCH       /**< Identical to #DR_LOG_SYNCH. */
#    define LOG_MEMSTATS DR_LOG_MEMSTATS /**< Identical to #DR_LOG_MEMSTATS. */
#    define LOG_OPTS DR_LOG_OPTS         /**< Identical to #DR_LOG_OPTS. */
#    define LOG_SIDELINE DR_LOG_SIDELINE /**< Identical to #DR_LOG_SIDELINE. */
#    define LOG_SYMBOLS DR_LOG_SYMBOLS   /**< Identical to #DR_LOG_SYMBOLS. */
#    define LOG_RCT DR_LOG_RCT           /**< Identical to #DR_LOG_RCT. */
#    define LOG_NT DR_LOG_NT             /**< Identical to #DR_LOG_NT. */
#    define LOG_HOT_PATCHING \
        DR_LOG_HOT_PATCHING              /**< Identical to #DR_LOG_HOT_PATCHING. */
#    define LOG_HTABLE DR_LOG_HTABLE     /**< Identical to #DR_LOG_HTABLE. */
#    define LOG_MODULEDB DR_LOG_MODULEDB /**< Identical to #DR_LOG_MODULEDB. */
#    define LOG_ALL DR_LOG_ALL           /**< Identical to #DR_LOG_ALL. */
#endif

DR_API
/**
 * Returns the log file for the thread with drcontext \p drcontext.
 * If \p drcontext is NULL, returns the main log file.
 */
file_t
dr_get_logfile(void *drcontext);

DR_API
/**
 * Returns true iff the -stderr_mask runtime option is non-zero, indicating
 * that the user wants notification messages printed to stderr.
 */
bool
dr_is_notify_on(void);

DR_API
/** Returns a handle to stdout. */
file_t
dr_get_stdout_file(void);

DR_API
/** Returns a handle to stderr. */
file_t
dr_get_stderr_file(void);

DR_API
/** Returns a handle to stdin. */
file_t
dr_get_stdin_file(void);

#ifdef PROGRAM_SHEPHERDING
DR_API
/** Writes a security violation forensics report to the supplied file. The forensics
 *  report will include detailed information about the source and target addresses of the
 *  violation as well as information on the current thread, process, and machine.  The
 *  forensics report is generated in an xml block described by dr_forensics-1.0.dtd.
 *  The encoding used is iso-8859-1.
 *
 *  The dcontext, violation, and action arguments are supplied by the security violation
 *  event callback.  The file argument is the file to write the forensics report to and
 *  the violation_name argument is a supplied name for the violation.
 */
void
dr_write_forensics_report(void *dcontext, file_t file,
                          dr_security_violation_type_t violation,
                          dr_security_violation_action_t action,
                          const char *violation_name);
#endif /* PROGRAM_SHEPHERDING */

#ifdef WINDOWS
DR_API
/**
 * Displays a message in a pop-up window. \note Windows only. \note On Windows Vista
 * most Windows services are unable to display message boxes.
 */
void
dr_messagebox(const char *fmt, ...);
#endif

DR_API
/**
 * Stdout printing that won't interfere with the
 * application's own printing.
 * It is not buffered, which means that it should not be used for
 * very frequent, small print amounts: for that the client should either
 * do its own buffering or it should use printf from the C library
 * via DR's private loader.
 * \note On Windows 7 and earlier, this routine is not able to print
 * to the \p cmd window
 * unless dr_enable_console_printing() is called ahead of time, and
 * even then there are limitations: see dr_enable_console_printing().
 * \note This routine supports printing wide characters via the ls
 * or S format specifiers.  On Windows, they are assumed to be UTF-16,
 * and are converted to UTF-8.  On Linux, they are converted by simply
 * dropping the high-order bytes.
 * \note If the data to be printed is large it will be truncated to
 * an internal buffer size.  Use dr_snprintf() and dr_write_file() for
 * large output.
 * \note When printing floating-point values on x86, the caller's code should
 * use proc_save_fpstate() or be inside a clean call that
 * has requested to preserve the floating-point state, unless it can prove
 * that its compiler will not use x87 operations.
 */
void
dr_printf(const char *fmt, ...);

DR_API
/**
 * Printing to a file that won't interfere with the
 * application's own printing.
 * It is not buffered, which means that it should not be used for
 * very frequent, small print amounts: for that the client should either
 * do its own buffering or it should use printf from the C library
 * via DR's private loader.
 * \note On Windows 7 and earlier, this routine is not able to print to STDOUT or
 * STDERR in the \p cmd window unless dr_enable_console_printing() is
 * called ahead of time, and even then there are limitations: see
 * dr_enable_console_printing().
 * \note This routine supports printing wide characters via the ls
 * or S format specifiers.  On Windows, they are assumed to be UTF-16,
 * and are converted to UTF-8.  On Linux, they are converted by simply
 * dropping the high-order bytes.
 * \note If the data to be printed is large it will be truncated to
 * an internal buffer size.  Use dr_snprintf() and dr_write_file() for
 * large output.
 * \note On Linux this routine does not check for errors like EINTR.  Use
 * dr_write_file() if that is a concern.
 * \note When printing floating-point values, the caller's code should
 * use proc_save_fpstate() or be inside a clean call that
 * has requested to preserve the floating-point state, unless it can prove
 * that its compiler will not use x87 operations.
 * On success, the number of bytes written is returned.
 * On error, -1 is returned.
 */
ssize_t
dr_fprintf(file_t f, const char *fmt, ...);

DR_API
/**
 * Identical to dr_fprintf() but exposes va_list.
 */
ssize_t
dr_vfprintf(file_t f, const char *fmt, va_list ap);

#ifdef WINDOWS
DR_API
/**
 * Enables dr_printf() and dr_fprintf() to work with a legacy console
 * window (viz., \p cmd on Windows 7 or earlier).  Loads a private
 * copy of kernel32.dll (if not
 * already loaded) in order to accomplish this.  To keep the default
 * DR lean and mean, loading kernel32.dll is not performed by default.
 *
 * This routine must be called during client initialization (\p dr_client_main()).
 * If called later, it will fail.
 *
 * Without calling this routine, dr_printf() and dr_fprintf() will not
 * print anything in a console window on Windows 7 or earlier, nor will they
 * print anything when running a graphical application.
 *
 * Even after calling this routine, there are significant limitations
 * to console printing support in DR:
 *
 *  - On Windows versions prior to Vista, and for WOW64 applications
 *    on Vista, it does not work from the exit event.  Once the
 *    application terminates its state with csrss (toward the very end
 *    of ExitProcess), no output will show up on the console.  We have
 *    no good solution here yet as exiting early is not ideal.
 *  - In the future, with earliest injection (Issue 234), writing to the
 *    console may not work from the client init event on Windows 7 and
 *    earlier (it will work on Windows 8).
 *
 * These limitations stem from the complex arrangement of the console
 * window in Windows (prior to Windows 8), where printing to it
 * involves sending a message
 * in an undocumented format to the \p csrss process, rather than a
 * simple write to a file handle.  We recommend using a terminal
 * window such as cygwin's \p rxvt rather than the \p cmd window, or
 * alternatively redirecting all output to a file, which will solve
 * all of the above limitations.
 *
 * Returns whether successful.
 * \note Windows only.
 */
bool
dr_enable_console_printing(void);

DR_API
/**
 * Returns true if the current standard error handle belongs to a
 * legacy console window (viz., \p cmd on Windows 7 or earlier).  DR's
 * dr_printf() and dr_fprintf()
 * do not work with such console windows unless
 * dr_enable_console_printing() is called ahead of time, and even then
 * there are limitations detailed in dr_enable_console_printing().
 * This routine may result in loading a private copy of kernel32.dll.
 * \note Windows only.
 */
bool
dr_using_console(void);
#endif

DR_API
/**
 * Utility routine to print a formatted message to a string.  Will not
 * print more than max characters.  If successful, returns the number
 * of characters printed, not including the terminating null
 * character.  If the number of characters to write equals max, then
 * the caller is responsible for supplying a terminating null
 * character.  If the number of characters to write exceeds max, then
 * max characters are written and -1 is returned.  If an error
 * occurs, a negative value is returned.
 * \note This routine supports printing wide characters via the ls
 * or S format specifiers.  On Windows, they are assumed to be UTF-16,
 * and are converted to UTF-8.  On Linux, they are converted by simply
 * dropping the high-order bytes.
 * \note When printing floating-point values, the caller's code should
 * use proc_save_fpstate() or be inside a clean call that
 * has requested to preserve the floating-point state, unless it can prove
 * that its compiler will not use x87 operations..
 */
int
dr_snprintf(char *buf, size_t max, const char *fmt, ...);

DR_API
/**
 * Wide character version of dr_snprintf().  All of the comments for
 * dr_snprintf() apply, except for the hs or S format specifiers.
 * On Windows, these will assume that the input is UTF-8, and will
 * convert to UTF-16.  On Linux, they will widen a single-byte
 * character string into a wchar_t character string with zero as the
 * high-order bytes.
 */
int
dr_snwprintf(wchar_t *buf, size_t max, const wchar_t *fmt, ...);

DR_API
/**
 * Identical to dr_snprintf() but exposes va_list.
 */
int
dr_vsnprintf(char *buf, size_t max, const char *fmt, va_list ap);

DR_API
/**
 * Identical to dr_snwprintf() but exposes va_list.
 */
int
dr_vsnwprintf(wchar_t *buf, size_t max, const wchar_t *fmt, va_list ap);

DR_API
/**
 * Utility routine to parse strings that match a pre-defined format string,
 * similar to the sscanf() C routine.
 *
 * @param[in] str   String to parse.
 * @param[in] fmt   Format string controlling parsing.
 * @param[out] ...  All remaining parameters interpreted as output parameter
 *                  pointers.  The type of each parameter must match the type
 *                  implied by the corresponding format specifier in \p fmt.
 * \return The number of specifiers matched.
 *
 * The benefit of using dr_sscanf() over native sscanf() is that DR's
 * implementation is standalone, signal-safe, and cross-platform.  On Linux,
 * sscanf() has been observed to call malloc().  On Windows, sscanf() will call
 * strlen(), which can break when using mapped files.
 *
 * The behavior of dr_sscanf() is mostly identical to that of the sscanf() C
 * routine.
 *
 * Supported format specifiers:
 * - \%s: Matches a sequence of non-whitespace characters.  The string is copied
 *   into the provided output buffer.  To avoid buffer overflow, the caller
 *   should use a width specifier.
 * - \%c: Matches any single character.
 * - \%d: Matches a signed decimal integer.
 * - \%u: Matches an unsigned decimal integer.
 * - \%x: Matches an unsigned hexadecimal integer, with or without a leading 0x.
 * - \%p: Matches a pointer-sized hexadecimal integer as %x does.
 * - \%%: Matches a literal % character.  Does not store output.
 *
 * Supported format modifiers:
 * - *: The * modifier causes the scan to match the specifier, but not store any
 *   output.  No output parameter is consumed for this specifier, and one should
 *   not be passed.
 * - 0-9: A decimal integer preceding the specifier gives the width to match.
 *   For strings, this indicates the maximum number of characters to copy.  For
 *   integers, this indicates the maximum number of digits to parse.
 * - h: Marks an integer specifier as short.
 * - l: Marks an integer specifier as long.
 * - ll: Marks an integer specifier as long long.  Use this for 64-bit integers.
 *
 * \warning dr_sscanf() does \em not support parsing floating point numbers yet.
 */
int
dr_sscanf(const char *str, const char *fmt, ...);

DR_API
/**
 * Utility function that aids in tokenizing a string, such as a client
 * options string from dr_get_options().  The function scans \p str
 * until a non-whitespace character is found.  It then starts copying
 * into \p buf until a whitespace character is found denoting the end
 * of the token.  If the token begins with a quote, the token
 * continues (including across whitespace) until the matching end
 * quote is found.  Characters considered whitespace are ' ', '\\t',
 * '\\r', and '\\n'; characters considered quotes are '\\'', '\\"', and
 * '`'.
 *
 * @param[in]  str     The start of the string containing the next token.
 * @param[out] buf     A buffer to store a null-terminated copy of the next token.
 * @param[in]  buflen  The capacity of the buffer, in characters.  If the token
 *                     is too large to fit, it will be truncated and null-terminated.
 *
 * \return a pointer to the end of the token in \p str.  Thus, to retrieve
 * the subsequent token, call this routine again with the prior return value
 * as the new value of \p str.  Returns NULL when the end of \p str is reached.
 */
const char *
dr_get_token(const char *str, char *buf, size_t buflen);

DR_API
/** Prints \p msg followed by the instruction \p instr to file \p f. */
void
dr_print_instr(void *drcontext, file_t f, instr_t *instr, const char *msg);

DR_API
/** Prints \p msg followed by the operand \p opnd to file \p f. */
void
dr_print_opnd(void *drcontext, file_t f, opnd_t opnd, const char *msg);

/**************************************************
 * THREAD SUPPORT
 */

DR_API
/**
 * Returns the DR context of the current thread.
 */
void *
dr_get_current_drcontext(void);

DR_API
/** Returns the thread id of the thread with drcontext \p drcontext. */
thread_id_t
dr_get_thread_id(void *drcontext);

#ifdef WINDOWS
DR_API
/**
 * Returns a Windows handle to the thread with drcontext \p drcontext.
 * This handle is DR's handle to this thread (it is not a separate
 * copy) and as such it should not be closed by the caller; nor should
 * it be used beyond the thread's exit, as DR's handle will be closed
 * at that point.
 *
 * The handle should have THREAD_ALL_ACCESS privileges.
 * \note Windows only.
 */
HANDLE
dr_get_dr_thread_handle(void *drcontext);
#endif

DR_API
/**
 * Returns the user-controlled thread-local-storage field.  To
 * generate an instruction sequence that reads the drcontext field
 * inline in the code cache, use dr_insert_read_tls_field().
 */
void *
dr_get_tls_field(void *drcontext);

DR_API
/**
 * Sets the user-controlled thread-local-storage field.  To
 * generate an instruction sequence that reads the drcontext field
 * inline in the code cache, use dr_insert_write_tls_field().
 */
void
dr_set_tls_field(void *drcontext, void *value);

DR_API
/**
 * Get DR's thread local storage segment base pointed at by \p tls_register.
 * It can be used to get the base of the thread-local storage segment
 * used by #dr_raw_tls_calloc.
 *
 * \note It should not be called on thread exit event,
 * as the thread exit event may be invoked from other threads.
 * See #dr_register_thread_exit_event for details.
 */
void *
dr_get_dr_segment_base(IN reg_id_t tls_register);

DR_API
/**
 * Allocates \p num_slots contiguous thread-local storage (TLS) slots that
 * can be directly accessed via an offset from \p tls_register.
 * If \p alignment is non-zero, the slots will be aligned to \p alignment.
 * These slots will be initialized to 0 for each new thread.
 * The slot offsets are [\p offset .. \p offset + (num_slots - 1)].
 * These slots are disjoint from the #dr_spill_slot_t register spill slots
 * and the client tls field (dr_get_tls_field()).
 * Returns whether or not the slots were successfully obtained.
 * The linear address of the TLS base pointed at by \p tls_register can be obtained
 * using #dr_get_dr_segment_base.
 * Raw TLs slots can be read directly using dr_insert_read_raw_tls() and written
 * using dr_insert_write_raw_tls().
 *
 * Supports passing 0 for \p num_slots, in which case \p tls_register will be
 * written but no other action taken.
 *
 * \note These slots are useful for thread-shared code caches.  With
 * thread-private caches, DR's memory pools are guaranteed to be
 * reachable via absolute or rip-relative accesses from the code cache
 * and client libraries.
 *
 * \note These slots are a limited resource.  On Windows the slots are
 * shared with the application and reserving even one slot can result
 * in failure to initialize for certain applications.  On Linux they
 * are more plentiful and transparent but currently DR limits clients
 * to no more than 64 slots.
 *
 * \note On Mac OS, TLS slots may not be initialized to zero.
 */
bool
dr_raw_tls_calloc(OUT reg_id_t *tls_register, OUT uint *offset, IN uint num_slots,
                  IN uint alignment);

DR_API
/**
 * Frees \p num_slots raw thread-local storage slots starting at
 * offset \p offset that were allocated with dr_raw_tls_calloc().
 * Returns whether or not the slots were successfully freed.
 */
bool
dr_raw_tls_cfree(uint offset, uint num_slots);

DR_API
/**
 * Returns an operand that refers to the raw TLS slot with offset \p
 * tls_offs from the TLS base \p tls_register.
 */
opnd_t
dr_raw_tls_opnd(void *drcontext, reg_id_t tls_register, uint tls_offs);

DR_API
/**
 * Inserts into ilist prior to "where" instruction(s) to read into the
 * general-purpose full-size register \p reg from the raw TLS slot with offset
 * \p tls_offs from the TLS base \p tls_register.
 */
void
dr_insert_read_raw_tls(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t tls_register, uint tls_offs, reg_id_t reg);

DR_API
/**
 * Inserts into ilist prior to "where" instruction(s) to store the value in the
 * general-purpose full-size register \p reg into the raw TLS slot with offset
 * \p tls_offs from the TLS base \p tls_register.
 */
void
dr_insert_write_raw_tls(void *drcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t tls_register, uint tls_offs, reg_id_t reg);

/* PR 222812: due to issues in supporting client thread synchronization
 * and other complexities we are using nudges for simple push-i/o and
 * saving thread creation for sideline usage scenarios.
 * These are implemented in <os>/os.c.
 */
/* PR 231301: for synch with client threads we can't distinguish between
 * client_lib->ntdll/gencode/other_lib (which is safe) from
 * client_lib->dr->ntdll/gencode/other_lib (which isn't) so we consider both
 * unsafe.  If the client thread spends a lot of time in ntdll or worse directly
 * makes blocking/long running system calls (note dr_thread_yield, dr_sleep,
 * dr_mutex_lock, and dr_messagebox are ok) then it may have performance or
 * correctness (if the synch times out) impacts.
 */
DR_API
/**
 * Creates a new thread that is marked as a non-application thread (i.e., DR
 * will let it run natively and not execute its code from the code cache).  The
 * thread will terminate automatically simply by returning from \p func; if
 * running when the application terminates its last thread, the client thread
 * will also terminate when DR shuts the process down.
 *
 * Init and exit events will not be raised for this thread (instead simply place
 * init and exit code in \p func).
 *
 * The new client thread has a drcontext that can be used for thread-private
 * heap allocations.  It has a stack of the same size as the DR stack used by
 * application threads.
 *
 * On Linux, this thread is guaranteed to have its own private itimer
 * if dr_set_itimer() is called from it.  However this does mean it
 * will have its own process id.
 *
 * A client thread should refrain from spending most of its time in
 * calls to other libraries or making blocking or long-running system
 * calls as such actions may incur performance or correctness problems
 * with DR's synchronization engine, which needs to be able to suspend
 * client threads at safe points and cannot determine whether the
 * aforementioned actions are safe for suspension.  Calling
 * dr_sleep(), dr_thread_yield(), dr_messagebox(), or using DR's locks
 * are safe.  If a client thread spends a lot of time holding locks,
 * consider marking it as un-suspendable by calling
 * dr_client_thread_set_suspendable() for better performance.
 *
 * Client threads, whether suspendable or not, must never execute from
 * the code cache as the underlying fragments might be removed by another
 * thread.
 *
 * Client threads are suspended while DR is not executing the application.
 * This includes initialization time: the client thread's \p func code will not
 * execute until DR starts executing the application.
 *
 * \note Thread creation via this routine is not yet fully
 * transparent: on Windows, the thread will show up in the list of
 * application threads if the operating system is queried about
 * threads.  The thread will not trigger a DLL_THREAD_ATTACH message.
 * On Linux, the thread will not receive signals meant for the application,
 * and is guaranteed to have a private itimer.
 */
bool
dr_create_client_thread(void (*func)(void *param), void *arg);

DR_API
/**
 * Can only be called from a client thread: returns false if called
 * from a non-client thread.
 *
 * Controls whether a client thread created with dr_create_client_thread()
 * will be suspended by DR for synchronization operations such as
 * flushing or client requests like dr_suspend_all_other_threads().
 * A client thread that spends a lot of time holding locks can gain
 * greater performance by not being suspended.
 *
 * A client thread \b will be suspended for a thread termination
 * operation, including at process exit, regardless of its suspendable
 * requests.
 */
bool
dr_client_thread_set_suspendable(bool suspendable);

DR_API
/** Current thread sleeps for \p time_ms milliseconds. */
void
dr_sleep(int time_ms);

DR_API
/** Current thread gives up its time quantum. */
void
dr_thread_yield(void);

/** Flags controlling the behavior of dr_suspend_all_other_threads_ex(). */
typedef enum {
    /**
     * By default, native threads are not suspended by
     * dr_suspend_all_other_threads_ex().  This flag requests that native
     * threads (including those temporarily-native due to actions such as
     * #DR_EMIT_GO_NATIVE) be suspended as well.
     */
    DR_SUSPEND_NATIVE = 0x0001,
} dr_suspend_flags_t;

/* FIXME - xref PR 227619 - some other event handler are safe (image_load/unload for*
 * example) which we could note here. */
DR_API
/**
 * Suspends all other threads in the process and returns an array of
 * contexts in \p drcontexts with one context per successfully suspended
 * thread.  The contexts can be passed to routines like dr_get_thread_id()
 * or dr_get_mcontext().  However, the contexts may not be modified:
 * dr_set_mcontext() is not supported.  dr_get_mcontext() can be called on
 * the caller of this routine, unless in a Windows nudge callback.
 *
 * The \p flags argument controls which threads are suspended and may
 * add further options in the future.
 *
 * The number of successfully suspended threads, which is also the length
 * of the \p drcontexts array, is returned in \p num_suspended, which is a
 * required parameter.  The number of un-successfully suspended threads, if
 * any, is returned in the optional parameter \p num_unsuspended.  The
 * calling thread is not considered in either count.  DR can fail to
 * suspend a thread for privilege reasons (e.g., on Windows in a
 * low-privilege process where another process injected a thread).  This
 * function returns true iff all threads were suspended, in which case \p
 * num_unsuspended will be 0.
 *
 * The caller must invoke dr_resume_all_other_threads() in order to resume
 * the suspended threads, free the \p drcontexts array, and release
 * coarse-grain locks that prevent new threads from being created.
 *
 * This routine may not be called from any registered event callback
 * other than the nudge event or the pre- or post-system call event.
 * It may be called from clean calls out of the cache.
 * This routine may not be called while any locks are held that
 * could block a thread processing a registered event callback or cache
 * callout.
 *
 * \note A client wishing to invoke this routine from an event callback can
 * queue up a nudge via dr_nudge_client() and invoke this routine from the
 * nudge callback.
 */
bool
dr_suspend_all_other_threads_ex(OUT void ***drcontexts, OUT uint *num_suspended,
                                OUT uint *num_unsuspended, dr_suspend_flags_t flags);

DR_API
/** Identical to dr_suspend_all_other_threads_ex() with \p flags set to 0. */
bool
dr_suspend_all_other_threads(OUT void ***drcontexts, OUT uint *num_suspended,
                             OUT uint *num_unsuspended);

DR_API
/**
 * May only be used after invoking dr_suspend_all_other_threads().  This
 * routine resumes the threads that were suspended by
 * dr_suspend_all_other_threads() and must be passed the same array and
 * count of suspended threads that were returned by
 * dr_suspend_all_other_threads().  It also frees the \p drcontexts array
 * and releases the locks acquired by dr_suspend_all_other_threads().  The
 * return value indicates whether all resumption attempts were successful.
 */
bool
dr_resume_all_other_threads(IN void **drcontexts, IN uint num_suspended);

DR_API
/**
 * Returns whether the thread represented by \p drcontext is currently
 * executing natively (typically due to an earlier #DR_EMIT_GO_NATIVE
 * return value).
 */
bool
dr_is_thread_native(void *drcontext);

DR_API
/**
 * Causes the thread owning \p drcontext to begin executing in the
 * code cache again once it is resumed.  The thread must currently be
 * suspended (typically by dr_suspend_all_other_threads_ex() with
 * #DR_SUSPEND_NATIVE) and must be currently native (typically from
 * #DR_EMIT_GO_NATIVE).  \return whether successful.
 */
bool
dr_retakeover_suspended_native_thread(void *drcontext);

/* We do not translate the context to avoid lock issues (PR 205795).
 * We do not delay until a safe point (via regular delayable signal path)
 * since some clients may want the interrupted context: for a general
 * timer clients should create a separate thread.
 */
#ifdef UNIX
DR_API
/**
 * Installs an interval timer in the itimer sharing group that
 * contains the calling thread.
 *
 * @param[in] which Must be one of ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF
 * @param[in] millisec The frequency of the timer, in milliseconds.  Passing
 *   0 disables the timer.
 * @param[in] func The function that will be called each time the
 *   timer fires.  It will be passed the context of the thread that
 *   received the itimer signal and its machine context, which has not
 *   been translated and so may contain raw code cache values.  The function
 *   will be called from a signal handler that may have interrupted a
 *   lock holder or other critical code, so it must be careful in its
 *   operations: keep it as simple as possible, and avoid any non-reentrant actions
 *   such as lock usage. If a general timer that does not interrupt client code
 *   is required, the client should create a separate thread via
 *   dr_create_client_thread() (which is guaranteed to have a private
 *   itimer) and set the itimer there, where the callback function can
 *   perform more operations safely if that new thread never acquires locks
 *   in its normal operation.
 *
 * Itimer sharing varies by kernel.  Prior to 2.6.12 itimers were
 * thread-private; after 2.6.12 they are shared across a thread group,
 * though there could be multiple thread groups in one address space.
 * The dr_get_itimer() function can be used to see whether a thread
 * already has an itimer in its group to avoid re-setting an itimer
 * set by an earlier thread.  A client thread created by
 * dr_create_client_thread() is guaranteed to not share its itimers
 * with application threads.
 *
 * The itimer will operate successfully in the presence of an
 * application itimer of the same type.
 *
 * Additional itimer signals are blocked while in our signal handler.
 *
 * The return value indicates whether the timer was successfully
 * installed (or uninstalled if 0 was passed for \p millisec).
 *
 * \note Linux-only.
 */
bool
dr_set_itimer(int which, uint millisec,
              void (*func)(void *drcontext, dr_mcontext_t *mcontext));

DR_API
/**
 * If an interval timer is already installed in the itimer sharing group that
 * contains the calling thread, returns its frequency.  Else returns 0.
 *
 * @param[in] which Must be one of ITIMER_REAL, ITIMER_VIRTUAL, or ITIMER_PROF
 *
 * \note Linux-only.
 */
uint
dr_get_itimer(int which);
#endif /* UNIX */

DR_API
/**
 * Should be called during process initialization.  Requests more accurate
 * tracking of the #dr_where_am_i_t value for use with dr_where_am_i().  By
 * default, if this routine is not called, DR avoids some updates to the value
 * that incur extra overhead, such as identifying clean callees.
 */
void
dr_track_where_am_i(void);

DR_API
/**
 * Returns whether DR is using accurate tracking of the #dr_where_am_i value.
 * Typically this is enabled by calling dr_track_where_am_i().
 */
bool
dr_is_tracking_where_am_i(void);

DR_API
/**
 * Returns the #dr_where_am_i_t value indicating in which area of code \p pc
 * resides.  This is meant for use with dr_set_itimer() for PC sampling for
 * profiling purposes.  If the optional \p tag is non-NULL and \p pc is inside a
 * fragment in the code cache, the fragment's tag is returned in \p tag.  It is
 * recommended that the user of this routine also call dr_track_where_am_i()
 * during process initialization for more accurate results.
 */
dr_where_am_i_t
dr_where_am_i(void *drcontext, app_pc pc, OUT void **tag);

/****************************************************************************
 * ADAPTIVE OPTIMIZATION SUPPORT
 */

/* xref PR 199115 and PR 237461: We decided to make the replace and
 * delete routines valid for -thread_private only.  Both routines
 * replace for the current thread and leave the other threads
 * unmodified.  The rationale is that we expect these routines will be
 * primarily useful for optimization, where a client wants to modify a
 * fragment specific to one thread.
 */
DR_API
/**
 * Replaces the fragment with tag \p tag with the instructions in \p
 * ilist.  This routine is only valid with the -thread_private option;
 * it replaces the fragment for the current thread only.  After
 * replacement, the existing fragment is allowed to complete if
 * currently executing.  For example, a clean call replacing the
 * currently executing fragment will safely return to the existing
 * code.  Subsequent executions will use the new instructions.
 *
 * \note The routine takes control of \p ilist and all responsibility
 * for deleting it.  The client should not keep, use, or reference,
 * the instrlist or any of the instrs it contains after passing.
 *
 * \note This routine supports replacement for the current thread
 * only.  \p drcontext must be from the current thread and must
 * be the drcontext used to create the instruction list.
 * This routine may not be called from the thread exit event.
 *
 * \return false if the fragment does not exist and true otherwise.
 */
bool
dr_replace_fragment(void *drcontext, void *tag, instrlist_t *ilist);

DR_API
/**
 * Deletes the fragment with tag \p tag.  This routine is only valid
 * with the -thread_private option; it deletes the fragment in the
 * current thread only.  After deletion, the existing fragment is
 * allowed to complete execution.  For example, a clean call deleting
 * the currently executing fragment will safely return to the existing
 * code.  Subsequent executions will cause DynamoRIO to reconstruct
 * the fragment, and therefore call the appropriate fragment-creation
 * event hook, if registered.
 *
 * \note This routine supports deletion for the current thread
 * only.  \p drcontext must be from the current thread and must
 * be the drcontext used to create the instruction list.
 * This routine may not be called from the thread exit event.
 *
 * \note Other options of removing the code fragments from code cache include
 * dr_flush_region(), dr_unlink_flush_region(), and dr_delay_flush_region().
 *
 * \return false if the fragment does not exist and true otherwise.
 */
bool
dr_delete_fragment(void *drcontext, void *tag);

/* FIXME - xref PR 227619 - some other event handler are safe (image_load/unload for*
 * example) which we could note here. */
DR_API
/**
 * Flush all fragments containing any code from the region [\p start, \p start + \p size).
 * Once this routine returns no execution will occur out of the fragments flushed.
 * This routine may only be called during a clean call from the cache, from a nudge
 * event handler, or from a pre- or post-system call event handler.
 * It may not be called from any other event callback.  No locks can
 * held when calling this routine.  If called from a clean call, caller can NOT return
 * to the cache (the fragment that was called out of may have been flushed even if it
 * doesn't apparently overlap the flushed region). Instead the caller must redirect
 * execution via dr_redirect_execution() (or DR_SIGNAL_REDIRECT from a signal callback)
 * after this routine to continue execution.  Returns true if successful.
 *
 * \note This routine may not be called from any registered event callback other than
 * the nudge event, the pre- or post-system call events, the exception event, or
 * the signal event; clean calls
 * out of the cache may call this routine.
 *
 * \note If called from a clean call, caller must continue execution by calling
 * dr_redirect_execution() after this routine, as the fragment containing the callout may
 * have been flushed. The context to use can be obtained via
 * dr_get_mcontext() with the exception of the pc to continue at which must be passed as
 * an argument to the callout (see instr_get_app_pc()) or otherwise determined.
 *
 * \note This routine may not be called while any locks are held that could block a thread
 * processing a registered event callback or cache callout.
 *
 * \note dr_delay_flush_region() has fewer restrictions on use, but is less synchronous.
 *
 * \note Use \p size == 1 to flush fragments containing the instruction at address
 * \p start. A flush of \p size == 0 is not allowed.
 *
 * \note Use flush_completion_callback to specify logic to be executed after the flush
 * and before the threads are resumed. Use NULL if not needed.
 *
 * \note As currently implemented, dr_delay_flush_region() with no completion callback
 * routine specified can be substantially more performant.
 */
bool
dr_flush_region_ex(app_pc start, size_t size,
                   void (*flush_completion_callback)(void *user_data), void *user_data);

DR_API
/** Equivalent to dr_flush_region_ex(start, size, NULL). */
bool
dr_flush_region(app_pc start, size_t size);

/* FIXME - get rid of the no locks requirement by making event callbacks !couldbelinking
 * and no dr locks (see PR 227619) so that client locks owned by this thread can't block
 * any couldbelinking thread.  FIXME - would be nice to make this available for
 * windows since there's less of a performance hit than using synch_all flushing, but
 * with coarse_units can't tell if we need a synch all flush or not and that confuses
 * the interface a lot. FIXME - xref PR 227619 - some other event handler are safe
 * (image_load/unload for example) which we could note here. */
/* FIXME - add a completion callback (see vm_area_check_shared_pending()). */
/* FIXME - could enable on windows when -thread_private since no coarse then. */
DR_API
/**
 * Flush all fragments containing any code from the region [\p start, \p start + \p size).
 * Control will not enter a fragment containing code from the region after this returns,
 * but a thread already in such a fragment will finish out the fragment.  This includes
 * the current thread if this is called from a clean call that returns to the cache.
 * This routine may only be called during a clean call from the cache, from a nudge
 * event handler, or from a pre- or post-system call event handler.
 * It may not be called from any other event callback.  No locks can be
 * held when calling this routine.  Returns true if successful.
 *
 * \note This routine may not be called from any registered event callback other than
 * the nudge event, the pre- or post-system call events, the exception event, or
 * the signal event; clean calls
 * out of the cache may call this routine.
 * \note This routine may not be called while any locks are held that could block a thread
 * processing a registered event callback or cache callout.
 * \note dr_delay_flush_region() has fewer restrictions on use, but is less synchronous.
 * \note Use \p size == 1 to flush fragments containing the instruction at address
 * \p start. A flush of \p size == 0 is not allowed.
 * \note This routine is only available with either the -thread_private
 * or -enable_full_api options.  It is not available when -opt_memory is specified.
 */
bool
dr_unlink_flush_region(app_pc start, size_t size);

/* FIXME - can we better bound when the flush will happen?  Maybe unlink shared syscalls
 * or similar or check the queue in more locations?  Should always hit the flush before
 * executing new code in the cache, and I think we'll always hit it before a nudge is
 * processed too.  Could trigger a nudge, or do this in a nudge, but that's rather
 * expensive. */
DR_API
/**
 * Request a flush of all fragments containing code from the region
 * [\p start, \p start + \p size).  The flush will be performed at the next safe
 * point in time (usually before any new code is added to the cache after this
 * routine is called). If \p flush_completion_callback is non-NULL, it will be
 * called with the \p flush_id provided to this routine when the flush completes,
 * after which no execution will occur out of the fragments flushed. Returns true
 * if the flush was successfully queued.
 *
 * \note dr_flush_region() and dr_unlink_flush_region() can give stronger guarantees on
 * when the flush will occur, but have more restrictions on use.
 * \note Use \p size == 1 to flush fragments containing the instruction at address
 * \p start.  A flush of \p size == 0 is not allowed.
 * \note As currently implemented there may be a performance penalty for requesting a
 * \p flush_completion_callback; for most performant usage set
 * \p flush_completion_callback to NULL.
 */
bool
dr_delay_flush_region(app_pc start, size_t size, uint flush_id,
                      void (*flush_completion_callback)(int flush_id));

DR_API
/** Returns whether or not there is a fragment in code cache with tag \p tag. */
bool
dr_fragment_exists_at(void *drcontext, void *tag);

DR_API
/**
 * Returns true if a basic block with tag \p tag exists in the code cache.
 */
bool
dr_bb_exists_at(void *drcontext, void *tag);

DR_API
/**
 * Looks up the fragment with tag \p tag.
 * If not found, returns 0.
 * If found, returns the total size occupied in the cache by the fragment.
 */
uint
dr_fragment_size(void *drcontext, void *tag);

DR_API
/** Retrieves the application PC of a fragment with tag \p tag. */
app_pc
dr_fragment_app_pc(void *tag);

DR_API
/**
 * Given an application PC, returns a PC that contains the application code
 * corresponding to the original PC.  In some circumstances on Windows DR
 * inserts a jump on top of the original code, which the client will not
 * see in the bb and trace hooks due to DR replacing it there with the
 * displaced original application code in order to present the client with
 * an unmodified view of the application code.  A client should use this
 * routine when attempting to decode the original application instruction
 * that caused a fault from the translated fault address, as the translated
 * address may actually point in the middle of DR's jump.
 *
 * \note Other applications on the system sometimes insert their own hooks,
 * which will not be hidden by DR and will appear to the client as jumps
 * and subsequent displaced code.
 */
app_pc
dr_app_pc_for_decoding(app_pc pc);

DR_API
/**
 * Given a code cache pc, returns the corresponding application pc.
 * This involves translating the state and thus may incur calls to
 * the basic block and trace events (see dr_register_bb_event()).
 * If translation fails, returns NULL.
 * This routine may not be called from a thread exit event.
 */
app_pc
dr_app_pc_from_cache_pc(byte *cache_pc);

DR_API
/**
 * Intended to be called between dr_app_setup() and dr_app_start() to
 * pre-create code cache fragments for each basic block address in the
 * \p tags array.  This speeds up the subsequent attach when
 * dr_app_start() is called.
 * If any code in the passed-in tags array is not readable, it is up to the
 * caller to handle any fault, as DR's own signal handlers are not enabled
 * at this point.
 * Returns whether successful.
 */
bool
dr_prepopulate_cache(app_pc *tags, size_t tags_count);

/**
 * Specifies the type of indirect branch for use with dr_prepopulate_indirect_targets().
 */
typedef enum {
    DR_INDIRECT_RETURN, /**< Return instruction type. */
    DR_INDIRECT_CALL,   /**< Indirect call instruction type. */
    DR_INDIRECT_JUMP,   /**< Indirect jump instruction type. */
} dr_indirect_branch_type_t;

DR_API
/**
 * Intended to augment dr_prepopulate_cache() by populating DR's indirect branch
 * tables, avoiding trips back to the dispatcher during initial execution.  This is only
 * effective when one of the the runtime options -shared_trace_ibt_tables and
 * -shared_bb_ibt_tables (depending on whether traces are enabled) is turned on, as
 * this routine does not try to populate tables belonging to threads other than the
 * calling thread.
 *
 * This is meant to be called between dr_app_setup() and dr_app_start(), immediately
 * after calling dr_prepopulate_cache().  It adds entries for each target address in
 * the \p tags array to the indirect branch table for the branch type \p branch_type.
 *
 * Returns whether successful.
 */
bool
dr_prepopulate_indirect_targets(dr_indirect_branch_type_t branch_type, app_pc *tags,
                                size_t tags_count);

DR_API
/**
 * Retrieves various statistics exported by DR as global, process-wide values.
 * The API is not thread-safe.
 * The caller is expected to pass a pointer to a valid, initialized #dr_stats_t
 * value, with the size field set (see #dr_stats_t).
 * Returns false if stats are not enabled.
 */
bool
dr_get_stats(dr_stats_t *drstats);

/****************************************************************************
 * CUSTOM TRACE SUPPORT
 */

DR_API
/**
 * Marks the fragment associated with tag \p tag as
 * a trace head.  The fragment need not exist yet -- once it is
 * created it will be marked as a trace head.
 *
 * DR associates a counter with a trace head and once it
 * passes the -hot_threshold parameter, DR begins building
 * a trace.  Before each fragment is added to the trace, DR
 * calls the client's end_trace callback to determine whether
 * to end the trace.  (The callback will be called both for
 * standard DR traces and for client-defined traces.)
 *
 * \note Some fragments are unsuitable for trace heads. DR will
 * ignore attempts to mark such fragments as trace heads and will return
 * false. If the client marks a fragment that doesn't exist yet as a trace
 * head and DR later determines that the fragment is unsuitable for
 * a trace head it will unmark the fragment as a trace head without
 * notifying the client.
 *
 * \note Some fragments' notion of trace heads is dependent on
 * which previous block targets them.  For these fragments, calling
 * this routine will only mark as a trace head for targets from
 * the same memory region.
 *
 * Returns true if the target fragment is marked as a trace head.
 */
bool
dr_mark_trace_head(void *drcontext, void *tag);

DR_API
/**
 * Checks to see if the fragment (or future fragment) with tag \p tag
 * is marked as a trace head.
 */
bool
dr_trace_head_at(void *drcontext, void *tag);

DR_API
/** Checks to see that if there is a trace in the code cache at tag \p tag. */
bool
dr_trace_exists_at(void *drcontext, void *tag);

/**************************************************
 * OPEN-ADDRESS HASHTABLE
 */

DR_API
/**
 * Allocates and initializes an open-address library-independent hashtable:
 *
 * @param[in] drcontext  This context controls whether thread-private or global
 *                    heap is used for the table.
 * @param[in] bits       The base-2 log of the initial capacity of the table.
 * @param[in] load_factor_percent The threshold of the table's occupancy at which
 *      it will be resized (so smaller values keep the table sparser
 *      and generally more performant but at the cost of more memory).
 *      This is a percentage and so must be between 0 and 100.
 *      Values are typically in the 20-80 range and for performance
 *      critical tables would usually be below 50.
 * @param[in] synch      Whether to use a lock around all operations.
 * @param[in] free_payload_func An optional function to call when removing an entry.
 *
 * @return a pointer to the heap-allocated table.
 */
void *
dr_hashtable_create(void *drcontext, uint bits, uint load_factor_percent, bool synch,
                    void (*free_payload_func)(void * /*drcontext*/, void *));

DR_API
/**
 * Destroys a hashtable created by dr_hashtable_create().
 *
 * @param[in] drcontext  Must be the same context passed to dr_hashtable_create().
 * @param[in] htable     A pointer to the table itself, returned by dr_hashtable_create().
 */
void
dr_hashtable_destroy(void *drcontext, void *htable);

DR_API
/**
 * Removes all entries in a hashtable created by dr_hashtable_create().
 *
 * @param[in] drcontext  Must be the same context passed to dr_hashtable_create().
 * @param[in] htable     A pointer to the table itself, returned by dr_hashtable_create().
 */
void
dr_hashtable_clear(void *drcontext, void *htable);

DR_API
/**
 * Queries whether an entry for the given key exists.
 *
 * @param[in] drcontext  Must be the same context passed to dr_hashtable_create().
 * @param[in] htable     A pointer to the table itself, returned by dr_hashtable_create().
 * @param[in] key        The key to query.
 *
 * @return the payload value for the key that was passed to dr_hashtable_add(),
 * or NULL if no such key is found.
 */
void *
dr_hashtable_lookup(void *drcontext, void *htable, ptr_uint_t key);

DR_API
/**
 * Adds a new entry to the hashtable.
 *
 * @param[in] drcontext  Must be the same context passed to dr_hashtable_create().
 * @param[in] htable     A pointer to the table itself, returned by dr_hashtable_create().
 * @param[in] key        The key to add.
 * @param[in] payload    The payload to add.
 */
void
dr_hashtable_add(void *drcontext, void *htable, ptr_uint_t key, void *payload);

DR_API
/**
 * Removes an entry for the given key.
 *
 * @param[in] drcontext  Must be the same context passed to dr_hashtable_create().
 * @param[in] htable     A pointer to the table itself, returned by dr_hashtable_create().
 * @param[in] key        The key to remove.
 *
 * @return whether the key was found.
 */
bool
dr_hashtable_remove(void *drcontext, void *htable, ptr_uint_t key);

#endif /* _DR_TOOLS_H_ */
