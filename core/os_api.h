/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

#ifndef _DR_OS_UTILS_H_
#define _DR_OS_UTILS_H_ 1

/**
 * @file dr_os_utils.h
 * @brief Operating-system-specific querying routines.
 */

/**************************************************
 * STATE SWAPPING TYPES
 */

/**
 * Flags that control the behavior of dr_switch_to_app_state_ex()
 * and dr_switch_to_dr_state_ex().
 */
typedef enum {
#ifdef WINDOWS
    DR_STATE_PEB = 0x0001,          /**< Switch the PEB pointer. */
    DR_STATE_TEB_MISC = 0x0002,     /**< Switch miscellaneous TEB fields. */
    DR_STATE_STACK_BOUNDS = 0x0004, /**< Switch the TEB stack bounds fields. */
    DR_STATE_ALL = ~0,              /**< Switch all state. */
#else
    /**
     * On Linux, DR's own TLS can optionally be swapped, but this is risky
     * and not recommended as incoming signals are not properly handled when in
     * such a state.  Thus DR_STATE_ALL does *not* swap it.
     */
    DR_STATE_DR_TLS = 0x0001,
    DR_STATE_ALL = (~0) & (~DR_STATE_DR_TLS), /**< Switch all normal state. */
    DR_STATE_GO_NATIVE = ~0,                  /**< Switch all state.  Use with care. */
#endif
} dr_state_flags_t;

DR_API
/**
 * Returns whether the given thread indicated by \p drcontext
 * is currently using the application version of its system state.
 * \sa dr_switch_to_dr_state(), dr_switch_to_app_state().
 *
 * This function does not indicate whether the machine context
 * (registers) contains application state or not.
 *
 * On Linux, DR very rarely switches the system state, while on
 * Windows DR switches the system state to the DR and client version
 * on every event callback or clean call.
 */
bool
dr_using_app_state(void *drcontext);

DR_API
/** Equivalent to dr_switch_to_app_state_ex(drcontext, DR_STATE_ALL). */
void
dr_switch_to_app_state(void *drcontext);

DR_API
/**
 * Swaps to the application version of any system state for the given
 * thread.  This is meant to be used prior to examining application
 * memory, when private libraries are in use and there are two
 * versions of system state.  Invoking non-DR library routines while
 * the application state is in place can lead to unpredictable
 * results: call dr_switch_to_dr_state() (or the _ex version) before
 * doing so.
 *
 * This function does not affect whether the current machine context
 * (registers) contains application state or not.
 *
 * The \p flags argument allows selecting a subset of the state to swap.
 */
void
dr_switch_to_app_state_ex(void *drcontext, dr_state_flags_t flags);

DR_API
/** Equivalent to dr_switch_to_dr_state_ex(drcontext, DR_STATE_ALL). */
void
dr_switch_to_dr_state(void *drcontext);

DR_API
/**
 * Should only be called after calling dr_switch_to_app_state() (or
 * the _ex version), or in certain cases where a client is running its
 * own code in an application state.  Swaps from the application
 * version of system state for the given thread back to the DR and
 * client version.
 *
 * This function does not affect whether the current machine context
 * (registers) contains application state or not.
 *
 * A client must call dr_switch_to_dr_state() in order to safely call
 * private library routines if it is running in an application context
 * where dr_using_app_state() returns true.  On Windows, this is the
 * case for any application context, as the system state is always
 * swapped.  On Linux, however, execution of application code in the
 * code cache only swaps the machine context and not the system state.
 * Thus, on Linux, while in the code cache, dr_using_app_state() will
 * return false, and it is safe to invoke private library routines
 * without calling dr_switch_to_dr_state().  Only if client or
 * client-invoked code will examine a segment selector or descriptor
 * does the state need to be swapped.  A state swap is much more
 * expensive on Linux (it requires a system call) than on Windows.
 *
 * The same flags that were passed to dr_switch_to_app_state_ex() should
 * be passed here.
 */
void
dr_switch_to_dr_state_ex(void *drcontext, dr_state_flags_t flags);

/**************************************************
 * APPLICATION COMMAND LINE
 */

/**
 * Encodings of an application's command-line argument.
 */
typedef enum {
    /**
     * C String Encoding.
     */
    DR_APP_ARG_CSTR_COMPAT,
    /**
     * UTF 16 String Encoding.
     */
    DR_APP_ARG_UTF_16,
} dr_app_arg_encoding_t;

/**
 * Contains information regarding an application's command-line argument.
 */
typedef struct _dr_app_arg_t {
    /**
     * The start boundary where the content of the arg begins.
     */
    void *start;
    /**
     * The size, in bytes, of the argument.
     */
    size_t size;
    /**
     * The encoding of the argument.
     */
    dr_app_arg_encoding_t encoding;
} dr_app_arg_t;

DR_API
/**
 * Provides information about the app's arguments by setting \p args_array up to
 * the count denoted by \p args_count. Therefore, \p args_count is not the size of the
 * buffer in bytes but the number of #dr_app_arg_t values that \p args_array can store.
 * Returns the number of args set or -1 on error.
 *
 * Use dr_app_arg_as_cstring() to get the argument as a string.
 *
 * Use dr_num_app_args() to query the total number of command-line arguments passed to
 * the application.
 *
 * \note Currently, this function is only available on Unix with
 * early injection.
 *
 * \note An error code may be obtained via dr_get_error_code() when this routine fails.
 */
int
dr_get_app_args(OUT dr_app_arg_t *args_array, int args_count);

DR_API
/**
 * Returns the number of command-line arguments passed to the application.
 *
 * \note Currently, this function is only available on Unix with
 * early injection.
 *
 * \note An error code may be obtained via dr_get_error_code() when this routine fails.
 */
int
dr_num_app_args(void);

DR_API
/**
 * Returns the passed argument \p app_arg as a string. \p buf is used only if needed, and
 * therefore the caller should not assume that the string is in the \p buf. In other
 * words, always use the returned value to refer to the string. Returns NULL on error
 * such as when \p buf is needed as storage and the size of the buffer \p buf_size
 * is not sufficient.
 *
 * To obtain a suitable upper-bound size of the string buffer, get the size of the
 * argument from the #dr_app_arg_t value retrieved via dr_get_app_args().
 *
 * \note Currently, this function is only available on Unix with
 * early injection.
 *
 * \note An error code may be obtained via dr_get_error_code() when this routine fails.
 */
const char *
dr_app_arg_as_cstring(IN dr_app_arg_t *app_arg, char *buf, int buf_size);

DR_API
/** Returns the image name (without path) of the current application. */
const char *
dr_get_application_name(void);

DR_API
/** Returns the process id of the current process. */
process_id_t
dr_get_process_id(void);

DR_API
/**
 * Returns the process id of the process associated with drcontext \p drcontext.
 * The returned value may be different from dr_get_process_id() if the passed context
 * was created in a different process, which may happen in thread exit callbacks.
 */
process_id_t
dr_get_process_id_from_drcontext(void *drcontext);

#ifdef UNIX
DR_API
/**
 * Returns the process id of the parent of the current process.
 * \note Linux only.
 */
process_id_t
dr_get_parent_id(void);
#endif

#ifdef WINDOWS

/** Windows versions */
typedef enum {
    /** Windows 10 1803 major update. */
    DR_WINDOWS_VERSION_10_1803 = 105,
    /** Windows 10 1709 major update. */
    DR_WINDOWS_VERSION_10_1709 = 104,
    /** Windows 10 1703 major update. */
    DR_WINDOWS_VERSION_10_1703 = 103,
    /** Windows 10 1607 major update. */
    DR_WINDOWS_VERSION_10_1607 = 102,
    /**
     * Windows 10 TH2 1511.  For future Windows updates that change system call
     * numbers, we'll perform our own artificial minor version number update as
     * done here, and use the YYMM version as the sub-name, as officially the OS
     * version will supposedly remain 10.0 forever.
     */
    DR_WINDOWS_VERSION_10_1511 = 101,
    /** Windows 10 pre-TH2 */
    DR_WINDOWS_VERSION_10 = 100,
    /** Windows 8.1 */
    DR_WINDOWS_VERSION_8_1 = 63,
    /** Windows Server 2012 R2 */
    DR_WINDOWS_VERSION_2012_R2 = DR_WINDOWS_VERSION_8_1,
    /** Windows 8 */
    DR_WINDOWS_VERSION_8 = 62,
    /** Windows Server 2012 */
    DR_WINDOWS_VERSION_2012 = DR_WINDOWS_VERSION_8,
    /** Windows 7 */
    DR_WINDOWS_VERSION_7 = 61,
    /** Windows Server 2008 R2 */
    DR_WINDOWS_VERSION_2008_R2 = DR_WINDOWS_VERSION_7,
    /** Windows Vista */
    DR_WINDOWS_VERSION_VISTA = 60,
    /** Windows Server 2008 */
    DR_WINDOWS_VERSION_2008 = DR_WINDOWS_VERSION_VISTA,
    /** Windows Server 2003 */
    DR_WINDOWS_VERSION_2003 = 52,
    /** Windows XP 64-bit */
    DR_WINDOWS_VERSION_XP_X64 = DR_WINDOWS_VERSION_2003,
    /** Windows XP */
    DR_WINDOWS_VERSION_XP = 51,
    /** Windows 2000 */
    DR_WINDOWS_VERSION_2000 = 50,
    /** Windows NT */
    DR_WINDOWS_VERSION_NT = 40,
} dr_os_version_t;

/** Data structure used with dr_get_os_version() */
typedef struct _dr_os_version_info_t {
    /** The size of this structure.  Set this to sizeof(dr_os_version_info_t). */
    size_t size;
    /** The operating system version */
    dr_os_version_t version;
    /** The service pack major number */
    uint service_pack_major;
    /** The service pack minor number */
    uint service_pack_minor;
    /** The build number. */
    uint build_number;
    /** The release identifier (such as "1803" for a Windows 10 release). */
    char release_id[64];
    /** The edition (such as "Education" or "Professional"). */
    char edition[64];
} dr_os_version_info_t;

DR_API
/**
 * Returns information about the version of the operating system.
 * Returns whether successful.  \note Windows only.  \note The Windows
 * API routine GetVersionEx may hide distinctions between versions,
 * such as between Windows 8 and Windows 8.1.  DR reports the true
 * low-level version.
 *
 */
bool
dr_get_os_version(dr_os_version_info_t *info);

DR_API
/**
 * Returns true if this process is a 32-bit process operating on a
 * 64-bit Windows kernel, known as Windows-On-Windows-64, or WOW64.
 * Returns false otherwise.  \note Windows only.
 */
bool
dr_is_wow64(void);

DR_API
/**
 * Returns a pointer to the application's Process Environment Block
 * (PEB).  DR swaps to a private PEB when running client code, in
 * order to isolate the client and its dependent libraries from the
 * application, so conventional methods of reading the PEB will obtain
 * the private PEB instead of the application PEB.  \note Windows only.
 */
void *
dr_get_app_PEB(void);

DR_API
/**
 * Converts a process handle to a process id.
 * \return Process id if successful; INVALID_PROCESS_ID on failure.
 * \note Windows only.
 */
process_id_t
dr_convert_handle_to_pid(HANDLE process_handle);

DR_API
/**
 * Converts a process id to a process handle.
 * \return Process handle if successful; INVALID_HANDLE_VALUE on failure.
 * \note Windows only.
 */
HANDLE
dr_convert_pid_to_handle(process_id_t pid);

#endif /* WINDOWS */

/**************************************************
 * CLIENT AUXILIARY LIBRARIES
 */

/**
 * A handle to a loaded client auxiliary library.  This is a different
 * type than module_handle_t and is not necessarily the base address.
 */
typedef void *dr_auxlib_handle_t;
/** An exported routine in a loaded client auxiliary library. */
typedef void (*dr_auxlib_routine_ptr_t)();
#if defined(WINDOWS) && !defined(X64)
/**
 * A handle to a loaded 64-bit client auxiliary library.  This is a different
 * type than module_handle_t and is not necessarily the base address.
 */
typedef uint64 dr_auxlib64_handle_t;
/** An exported routine in a loaded 64-bit client auxiliary library. */
typedef uint64 dr_auxlib64_routine_ptr_t;
#endif

DR_API
/**
 * Loads the library with the given path as an auxiliary client
 * library.  The library is not treated as an application module but
 * as an extension of DR.  The library will be included in
 * dr_memory_is_in_client() and any faults in the library will be
 * considered client faults.  The bounds of the loaded library are
 * returned in the optional out variables.  On failure, returns NULL.
 *
 * If only a filename and not a full path is given, this routine will
 * search for the library in the standard search locations for DR's
 * private loader.
 */
dr_auxlib_handle_t
dr_load_aux_library(const char *name, byte **lib_start /*OPTIONAL OUT*/,
                    byte **lib_end /*OPTIONAL OUT*/);

DR_API
/**
 * Looks up the exported routine with the given name in the given
 * client auxiliary library loaded by dr_load_aux_library().  Returns
 * NULL on failure.
 */
dr_auxlib_routine_ptr_t
dr_lookup_aux_library_routine(dr_auxlib_handle_t lib, const char *name);

DR_API
/**
 * Unloads the given library, which must have been loaded by
 * dr_load_aux_library().  Returns whether successful.
 */
bool
dr_unload_aux_library(dr_auxlib_handle_t lib);

#if defined(WINDOWS) && !defined(X64)

DR_API
/**
 * Similar to dr_load_aux_library(), but loads a 64-bit library for
 * access from a 32-bit process running on a 64-bit Windows kernel.
 * Fails if called from a 32-bit kernel or from a 64-bit process.
 * The library will be located in the low part of the address space
 * with 32-bit addresses.
 * Functions in the library can be called with dr_invoke_x64_routine().
 *
 * \warning Invoking 64-bit code is fragile.  Currently, this routine
 * uses the system loader, under the assumption that little isolation
 * is needed versus application 64-bit state.  Consider use of this routine
 * experimental: use at your own risk!
 *
 * \note Windows only.
 *
 * \note Currently this routine does not support loading kernel32.dll
 * or any library that depends on it.
 * It also does not invoke the entry point for any dependent libraries
 * loaded as part of loading \p name.
 *
 * \note Currently this routine does not support Windows 8 or higher.
 */
dr_auxlib64_handle_t
dr_load_aux_x64_library(const char *name);

DR_API
/**
 * Looks up the exported routine with the given name in the given
 * 64-bit client auxiliary library loaded by dr_load_aux_x64_library().  Returns
 * NULL on failure.
 * The returned function can be called with dr_invoke_x64_routine().
 *
 * \note Windows only.
 *
 * \note Currently this routine does not support Windows 8.
 */
dr_auxlib64_routine_ptr_t
dr_lookup_aux_x64_library_routine(dr_auxlib64_handle_t lib, const char *name);

DR_API
/**
 * Unloads the given library, which must have been loaded by
 * dr_load_aux_x64_library().  Returns whether successful.
 *
 * \note Windows only.
 */
bool
dr_unload_aux_x64_library(dr_auxlib64_handle_t lib);

DR_API
/**
 * Must be called from 32-bit mode.  Switches to 64-bit mode, calls \p
 * func64 with the given parameters, switches back to 32-bit mode, and
 * then returns to the caller.  Requires that \p func64 be located in
 * the low 4GB of the address space.  All parameters must be 32-bit
 * sized, and all are widened via sign-extension when passed to \p
 * func64.
 *
 * \return -1 on failure; else returns the return value of \p func64.
 *
 * \warning Invoking 64-bit code is fragile.  The WOW64 layer assumes
 * there is no other 64-bit code that will be executed.
 * dr_invoke_x64_routine() attempts to save the WOW64 state, but it
 * has not been tested in all versions of WOW64.  Also, invoking
 * 64-bit code that makes callbacks is not supported, as not only a
 * custom wrapper to call the 32-bit code in the right mode would be
 * needed, but also a way to restore the WOW64 state in case the
 * 32-bit callback makes a system call.  Consider use of this routine
 * experimental: use at your own risk!
 *
 * \note Windows only.
 */
int64
dr_invoke_x64_routine(dr_auxlib64_routine_ptr_t func64, uint num_params, ...);

#endif /* WINDOWS && !X64 */

/**************************************************
 * MEMORY QUERY/ACCESS ROUTINES
 */

#define DR_MEMPROT_NONE 0x00  /**< No read, write, or execute privileges. */
#define DR_MEMPROT_READ 0x01  /**< Read privileges. */
#define DR_MEMPROT_WRITE 0x02 /**< Write privileges. */
#define DR_MEMPROT_EXEC 0x04  /**< Execute privileges. */
#define DR_MEMPROT_GUARD 0x08 /**< Guard page (Windows only) */
/**
 * DR's default cache consistency strategy modifies the page protection of
 * pages containing code, making them read-only.  It pretends on application
 * and client queries that the page is writable.  On a write fault
 * to such a region by the application or by client-added instrumentation, DR
 * automatically handles the fault and makes the page writable.  This requires
 * flushing the code from the code cache, which can only be done safely when in
 * an application context.  Thus, a client writing to such a page is only
 * supported when these criteria are met:
 *
 * -# The client code must be in an application code cache context.  This rules
 *    out all event callbacks (including the basic block event) except for the
 *    pre and post system call events and the nudge event.
 * -# The client must not hold any locks.  An exception is a lock marked as
 *    an application lock (via dr_mutex_mark_as_app(), dr_rwlock_mark_as_app(),
 *    or dr_recurlock_mark_as_app()).
 * -# The client code must not rely on returning to a particular point in the
 *    code cache, as that point might be flushed and removed during the write
 *    fault processing.  This rules out a clean call (unless
 *    dr_redirect_execution() is used), but does allow something like
 *    drwrap_replace_native() which uses a continuation strategy.
 *
 * A client write fault that does not meet the first two criteria will result in
 * a fatal error report and an abort.  It is up to the client to ensure it
 * satisifies the third criterion.
 *
 * Even when client writes do meet these criteria, for performance it's best for
 * clients to avoid writing to such memory.
 */
#define DR_MEMPROT_PRETEND_WRITE 0x10
/**
 * In addition to the appropriate DR_MEMPROT_READ and/or DR_MEMPROT_EXEC flags,
 * this flag will be set for the VDSO and VVAR pages on Linux.
 * The VVAR pages may only be identified by DR on kernels that explicitly label
 * the pages in the /proc/self/maps file (kernel 3.15 and above).
 * In some cases, accessing the VVAR pages can cause problems
 * (e.g., https://github.com/DynamoRIO/drmemory/issues/1778).
 */
#define DR_MEMPROT_VDSO 0x20

/**
 * Flags describing memory used by dr_query_memory_ex().
 */
typedef enum {
    DR_MEMTYPE_FREE,     /**< No memory is allocated here */
    DR_MEMTYPE_IMAGE,    /**< An executable file is mapped here */
    DR_MEMTYPE_DATA,     /**< Some other data is allocated here */
    DR_MEMTYPE_RESERVED, /**< Reserved address space with no physical storage */
    DR_MEMTYPE_ERROR,    /**< Query failed for unspecified reason */
    /**
     * Query failed due to the address being located in Windows kernel space.
     * No further information is available so iteration must stop.
     */
    DR_MEMTYPE_ERROR_WINKERNEL,
} dr_mem_type_t;

/**
 * Describes a memory region.  Used by dr_query_memory_ex().
 */
typedef struct _dr_mem_info_t {
    /** Starting address of memory region */
    byte *base_pc;
    /** Size of region */
    size_t size;
    /** Protection of region (DR_MEMPROT_* flags) */
    uint prot;
    /** Type of region */
    dr_mem_type_t type;
} dr_mem_info_t;

#ifdef DR_PAGE_SIZE_COMPATIBILITY

#    undef PAGE_SIZE
/**
 * Size of a page of memory. This uses a function call so be careful
 * where performance is critical.
 */
#    define PAGE_SIZE dr_page_size()

/**
 * Convenience macro to align to the start of a page of memory.
 * It uses a function call so be careful where performance is critical.
 */
#    define PAGE_START(x) (((ptr_uint_t)(x)) & ~(dr_page_size() - 1))

#endif /* DR_PAGE_SIZE_COMPATIBILITY */

DR_API
/** Returns the size of a page of memory. */
size_t
dr_page_size(void);

DR_API
/**
 * Checks to see that all bytes with addresses in the range [\p pc, \p pc + \p size - 1]
 * are readable and that reading from that range won't generate an exception (see also
 * dr_safe_read() and DR_TRY_EXCEPT()).
 * \note Nothing guarantees that the memory will stay readable for any length of time.
 * \note On Linux, especially if the app is in the middle of loading a library
 * and has not properly set up the .bss yet, a page that seems readable can still
 * generate SIGBUS if beyond the end of an mmapped file.  Use dr_safe_read() or
 * DR_TRY_EXCEPT() to avoid such problems.
 */
bool
dr_memory_is_readable(const byte *pc, size_t size);

/* FIXME - this is a real view of memory including changes made for dr cache consistency,
 * but what we really want to show the client is the apps view of memory (which would
 * requires fixing correcting the view and fixing up exceptions for areas we made read
 * only) - see PR 198873 */
DR_API
/**
 * An os neutral method for querying a memory address. Returns true
 * iff a memory region containing \p pc is found.  If found additional
 * information about the memory region is returned in the optional out
 * arguments \p base_pc, \p size, and \p prot where \p base_pc is the
 * start address of the memory region continaing \p pc, \p size is the
 * size of said memory region and \p prot is an ORed combination of
 * DR_MEMPROT_* flags describing its current protection.
 *
 * \note To examine only application memory, skip memory for which
 * dr_memory_is_dr_internal() or dr_memory_is_in_client() returns true.
 *
 * \note DR may mark writable code pages as read-only but pretend they're
 * writable.  When this happens, it will include both #DR_MEMPROT_WRITE
 * and #DR_MEMPROT_PRETEND_WRITE in \p prot.
 */
bool
dr_query_memory(const byte *pc, byte **base_pc, size_t *size, uint *prot);

DR_API
/**
 * Provides additional information beyond dr_query_memory().
 * Returns true if it was able to obtain information (including about
 * free regions) and sets the fields of \p info.  This routine can be
 * used to iterate over the entire address space.  Such an iteration
 * should stop on reaching the top of the address space, or on
 * reaching kernel memory (look for #DR_MEMTYPE_ERROR_WINKERNEL) on
 * Windows.
 *
 * Returns false on failure and sets info->type to a DR_MEMTYPE_ERROR*
 * code indicating the reason for failure.
 *
 * \note To examine only application memory, skip memory for which
 * dr_memory_is_dr_internal() returns true.
 *
 * \note DR may mark writable code pages as read-only but pretend they're
 * writable.  When this happens, it will include both #DR_MEMPROT_WRITE
 * and #DR_MEMPROT_PRETEND_WRITE in \p info->prot.
 */
bool
dr_query_memory_ex(const byte *pc, OUT dr_mem_info_t *info);

#ifdef WINDOWS
DR_API
/**
 * Equivalent to the win32 API function VirtualQuery().
 * See that routine for a description of
 * arguments and return values.  \note Windows only.
 *
 * \note DR may mark writable code pages as read-only but pretend they're
 * writable.  When this happens, this routine will indicate that the
 * memory is writable.  Call dr_query_memory() or dr_query_memory_ex()
 * before attempting to write to application memory to ensure it's
 * not read-only underneath.
 */
size_t
dr_virtual_query(const byte *pc, MEMORY_BASIC_INFORMATION *mbi, size_t mbi_size);
#endif

DR_API
/**
 * Safely reads \p size bytes from address \p base into buffer \p
 * out_buf.  Reading is done without the possibility of an exception
 * occurring.  Returns true if the entire \p size bytes were read;
 * otherwise returns false and if \p bytes_read is non-NULL returns the
 * partial number of bytes read in \p bytes_read.
 * \note See also DR_TRY_EXCEPT().
 */
bool
dr_safe_read(const void *base, size_t size, void *out_buf, size_t *bytes_read);

DR_API
/**
 * Safely writes \p size bytes from buffer \p in_buf to address \p
 * base.  Writing is done without the possibility of an exception
 * occurring.    Returns true if the entire \p size bytes were written;
 * otherwise returns false and if \p bytes_written is non-NULL returns the
 * partial number of bytes written in \p bytes_written.
 * \note See also DR_TRY_EXCEPT().
 */
bool
dr_safe_write(void *base, size_t size, const void *in_buf, size_t *bytes_written);

DR_API
/** Do not call this directly: use the DR_TRY_EXCEPT macro instead. */
void
dr_try_setup(void *drcontext, void **try_cxt);

DR_API
/** Do not call this directly: use the DR_TRY_EXCEPT macro instead. */
int
dr_try_start(void *buf);

DR_API
/** Do not call this directly: use the DR_TRY_EXCEPT macro instead. */
void
dr_try_stop(void *drcontext, void *try_cxt);

/**
 * Simple try..except support for executing operations that might
 * fault and recovering if they do.  Be careful with this feature
 * as it has some limitations:
 * - do not use a return within a try statement (we do not have
 *   language support)
 * - any automatic variables that you want to use in the except
 *   block should be declared volatile
 * - no locks should be grabbed in a try statement (because
 *   there is no finally support to release them)
 * - nesting is supported, but finally statements are not
 *   supported
 *
 * For fault-free reads in isolation, use dr_safe_read() instead.
 * dr_safe_read() out-performs DR_TRY_EXCEPT.
 *
 * For fault-free writes in isolation, dr_safe_write() can be used,
 * although on Windows it invokes a system call and can be less
 * performant than DR_TRY_EXCEPT.
 */
#define DR_TRY_EXCEPT(drcontext, try_statement, except_statement)  \
    do {                                                           \
        void *try_cxt;                                             \
        dr_try_setup(drcontext, &try_cxt);                         \
        if (dr_try_start(try_cxt) == 0) {                          \
            try_statement dr_try_stop(drcontext, try_cxt);         \
        } else {                                                   \
            /* roll back first in case except faults or returns */ \
            dr_try_stop(drcontext, try_cxt);                       \
            except_statement                                       \
        }                                                          \
    } while (0)

DR_API
/**
 * Modifies the memory protections of the region from \p start through
 * \p start + \p size.  Modification of memory allocated by DR or of
 * the DR or client libraries themselves is allowed under the
 * assumption that the client knows what it is doing.  Modification of
 * the ntdll.dll library on Windows is not allowed.  Returns true if
 * successful.
 */
bool
dr_memory_protect(void *base, size_t size, uint new_prot);

DR_API
/**
 * Returns true iff pc is memory allocated by DR for its own
 * purposes, and would not exist if the application were run
 * natively.
 */
bool
dr_memory_is_dr_internal(const byte *pc);

DR_API
/**
 * Returns true iff pc is located inside a client library, an Extension
 * library used by a client, or an auxiliary client library (see
 * dr_load_aux_library()).
 */
bool
dr_memory_is_in_client(const byte *pc);

#endif /* _DR_OS_UTILS_H_ */
