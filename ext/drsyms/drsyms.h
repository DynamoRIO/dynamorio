/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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

/* DRSyms DynamoRIO Extension 
 *
 * Symbol lookup support (Issue 44).
 * Currently only supports Windows PDB symbols: no Cygwin support or Linux
 * support yet.
 * This API will eventually support both sideline (via a separate
 * process) and online use.  Today only online use is supported.
 */

#ifndef _DRSYMS_H_
#define _DRSYMS_H_ 1

/**
 * @file drsyms.h
 * @brief Header for DRSyms DynamoRIO Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Our API routines take the full path to the module in every query,
 * both for simplicity of use and to allow for flexibility in
 * implementation: can unload symbol data if running out of space
 * without tracking what was previously loaded.
 *
 * They also take in an offset from the module base, rather than an absolute
 * address, to be agnostic of relocation.
 */

/**
 * \addtogroup drsyms Symbol Access Library
 */
/*@{*/ /* begin doxygen group */

/** Success code for each DRSyms operation */
typedef enum {
    DRSYM_SUCCESS,                  /**< Operation succeeded. */
    DRSYM_ERROR,                    /**< Operation failed. */
    DRSYM_ERROR_INVALID_PARAMETER,  /**< Operation failed: invalid parameter */
    DRSYM_ERROR_INVALID_SIZE,       /**< Operation failed: invalid size */
    DRSYM_ERROR_LOAD_FAILED,        /**< Operation failed: unable to load symbols */
    DRSYM_ERROR_SYMBOL_NOT_FOUND,   /**< Operation failed: symbol not found */
    DRSYM_ERROR_LINE_NOT_AVAILABLE, /**< Operation failed: line info not available */
    DRSYM_ERROR_NOT_IMPLEMENTED,    /**< Operation failed: not yet implemented */
    DRSYM_ERROR_FEATURE_NOT_AVAILABLE, /**< Operation failed: not available */
    DRSYM_ERROR_NOMEM,              /**< Operation failed: not enough memory */
} drsym_error_t;

/** Bitfield of options to each DRSyms operation. */
typedef enum {
    DRSYM_LEAVE_MANGLED = 0x00,     /**< Do not demangle C++ symbols. */
    /**
     * Demangle C++ symbols, omitting templates and parameter types.  On Linux,
     * both templates and parameters are collapsed to <> and () respectively.
     * On Windows, templates are still expanded, and parameters are omitted
     * without parentheses.
     */
    DRSYM_DEMANGLE      = 0x01,
    /** Demangle template arguments and parameter types. */
    DRSYM_DEMANGLE_FULL = 0x02,
    DRSYM_DEFAULT_FLAGS = DRSYM_DEMANGLE,   /**< Default flags. */
} drsym_flags_t;

/** Data structure that holds symbol information */
typedef struct _drsym_info_t {
    /* INPUTS */
    /** Input: should be set by caller to sizeof(drsym_info_t) */
    size_t struct_size;
    /** Input: should be set by caller to the size of the name[] buffer, in bytes */
    size_t name_size;

    /* OUTPUTS */
    /** Output: file and line number */
    const char *file;
    uint64 line;
    /** Output: offset from address that starts at line */
    size_t line_offs;
    /** Output: offset from module base of start of symbol */
    size_t start_offs;
    /** Output: offset from module base of end of symbol */
    size_t end_offs;
    /**
     * Output: size of data available for name.  Only name_size bytes will be
     * copied to name.
     */
    size_t name_available_size;
    /** Output: symbol name */
    char name[1];
} drsym_info_t;

#ifdef WINDOWS
# define IF_WINDOWS_ELSE(x,y) x
#else
# define IF_WINDOWS_ELSE(x,y) y
#endif

DR_EXPORT
/**
 * Initialize the symbol access library.
 * 
* @param[in] shmid Identifies the symbol server for sideline operation.
 * \note Sideline operation is not yet implemented.
 */
drsym_error_t
drsym_init(IF_WINDOWS_ELSE(const wchar_t *, int) shmid);

DR_EXPORT
/**
 * Clean up and shut down the symbol access library.
 */
drsym_error_t
drsym_exit(void);

DR_EXPORT
/**
 * Retrieves symbol information for a given module offset.
 * When returning DRSYM_ERROR_LINE_NOT_AVAILABLE, the symbol information
 * start_offs, end_offs, and name will still be valid.
 *
 * @param[in] modpath The full path to the module to be queried.
 * @param[in] modoffs The offset from the base of the module specifying the address
 *   to be queried.
 * @param[in,out] info Information about the symbol at the queried address.
 * @param[in]  flags   Options for the operation.  Ignored on Windows.
 */
drsym_error_t
drsym_lookup_address(const char *modpath, size_t modoffs, drsym_info_t *info /*INOUT*/,
                     uint flags);

enum {
    DRSYM_TYPE_OTHER,  /**< Unknown type, cannot downcast. */
    DRSYM_TYPE_INT,    /**< Integer, cast to drsym_int_type_t. */
    DRSYM_TYPE_PTR,    /**< Pointer, cast to drsym_ptr_type_t. */
    DRSYM_TYPE_FUNC,   /**< Function, cast to drsym_func_type_t. */
    /* Additional type kinds will be added as needed. */
};

/**
 * Base type information.
 * Use the 'kind' member to downcast to a more specific type.
 */
typedef struct _drsym_type_t {
    uint kind;      /**< Type kind, i.e. DRSYM_TYPE_INT or DRSYM_TYPE_PTR. */
    size_t size;    /**< Type size. */
} drsym_type_t;

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 4200)
#endif

typedef struct _drsym_func_type_t {
    drsym_type_t type;
    drsym_type_t *ret_type;
    int num_args;
    drsym_type_t *arg_types[0];  /**< Flexible array of size num_args. */
} drsym_func_type_t;

#ifdef _MSC_VER
# pragma warning(pop)
#endif

typedef struct _drsym_int_type_t {
    drsym_type_t type;
    bool is_signed;
} drsym_int_type_t;

typedef struct _drsym_ptr_type_t {
    drsym_type_t type;
    drsym_type_t *elt_type;
} drsym_ptr_type_t;

DR_EXPORT
/**
 * Retrieves function type information for a given module offset.  After a
 * successful execution, \p *func_type points to the function type.  All memory
 * used to represent the types comes from \p buf, so the caller only needs to
 * dispose \p buf to free them.  Returns DRSYM_ERROR_NOMEM if the buffer is not
 * big enough.
 *
 * @param[in] modpath    The full path to the module to be queried.
 * @param[in] modoffs    The offset from the base of the module specifying
 *                       the start address of the function.
 * @param[out] buf       Memory used for the types.
 * @param[in] buf_sz     Number of bytes in \p buf.
 * @param[out] func_type Pointer to the type of the function.
 */
drsym_error_t
drsym_get_func_type(const char *modpath, size_t modoffs,
                    char *buf, size_t buf_sz,
                    drsym_func_type_t **func_type /*OUT*/);

DR_EXPORT
/**
 * Retrieves the address for a given symbol name.
 *
 * On Windows, we don't support the DRSYM_DEMANGLE_FULL flag.  Also on Windows,
 * if DRSYM_DEMANGLE is set, \p symbol must include the template arguments.
 *
 * @param[in] modpath The full path to the module to be queried.
 * @param[in] symbol The name of the symbol being queried.
 *   To specify a target module, pass "modulename!symbolname" as the symbol
 *   string to look up.
 * @param[out] modoffs The offset from the base of the module specifying the address
 *   of the specified symbol.
 * @param[in]  flags   Options for the operation.  Ignored on Windows.
 */
drsym_error_t
drsym_lookup_symbol(const char *modpath, const char *symbol, size_t *modoffs /*OUT*/,
                    uint flags);

/** 
 * Type for drsym_enumerate_symbols and drsym_search_symbols callback function.
 * Returns whether to continue the enumeration or search.
 *
 * @param[in]  name    Name of the symbol.
 * @param[out] modoffs Offset of the symbol from the module base.
 * @param[in]  data    User parameter passed to drsym_enumerate_symbols() or
 *                     drsym_search_symbols().
 */
typedef bool (*drsym_enumerate_cb)(const char *name, size_t modoffs, void *data);

DR_EXPORT
/**
 * Enumerates all symbol information for a given module.
 * Calls the given callback function for each symbol.
 * If the callback returns false, the enumeration will end.
 *
 * @param[in] modpath   The full path to the module to be queried.
 * @param[in] callback  Function to call for each symbol found.
 * @param[in] data      User parameter passed to callback.
 * @param[in] flags     Options for the operation.  Ignored on Windows.
 */
drsym_error_t
drsym_enumerate_symbols(const char *modpath, drsym_enumerate_cb callback, void *data,
                        uint flags);

DR_EXPORT
/**
 * Given a mangled or decorated C++ symbol, outputs the source name into \p dst.
 * If the unmangled name requires more than \p dst_sz bytes, it is truncated and
 * null-terminated to fit into \p dst.  If the unmangling fails, \p symbol is
 * copied as-is into \p dst, and truncated and null-terminated to fit.
 * Returns zero if the name could not be unmangled, and the number of characters
 * required to store the name if it succeeded.  If there was overflow, the
 * return value may be an estimate of the required size, so a second attempt
 * with the return value is not guaranteed to be successful.  If the caller
 * needs the full name, they may need to make multiple attempts with a larger
 * buffer.
 *
 * @param[out] dst      Output buffer for demangled name.
 * @param[in]  dst_sz   Size of the output buffer in bytes.
 * @param[in]  mangled  Mangled C++ symbol to demangle.
 * @param[in]  flags    Options for the operation.  DRSYM_DEMANGLE is implied.
 */
size_t
drsym_demangle_symbol(char *dst, size_t dst_sz, const char *mangled,
                      uint flags);

#ifdef WINDOWS
DR_EXPORT
/**
 * Enumerates all symbol information matching a pattern for a given module.
 * Calls the given callback function for each matching symbol.
 * If the callback returns false, the enumeration will end.
 *
 * \note drsym_search_symbols() with full=false is significantly
 * faster and uses less memory than drsym_enumerate_symbols(), and is
 * faster than drsym_lookup_symbol(), but requires dbghelp.dll version
 * 6.3 or higher.  If an earlier version is used, this function will
 * use a slower mechanism to perform the search.
 *
 * @param[in] modpath   The full path to the module to be queried.
 * @param[in] match     Regular expression describing the names of the symbols
 *                      to be enumerated.  To specify a target module, use the
 *                      "module_pattern!symbol_pattern" format.
 * @param[in] full      Whether to search all symbols or (the default) just
 *                      functions.  A full search takes significantly
 *                      more time and memory and eliminates the
 *                      performance advantage over other lookup
 *                      methods.  A full search requires dbghelp.dll
 *                      version 6.6 or higher.
 * @param[in] callback  Function to call for each matching symbol found.
 * @param[in] data      User parameter passed to callback.
 */
drsym_error_t
drsym_search_symbols(const char *modpath, const char *match, bool full,
                     drsym_enumerate_cb callback, void *data);
#endif

DR_EXPORT
/**
 * Returns true if the current standard error handle belongs to a console
 * window (viz., \p cmd).  DR's dr_printf() and dr_fprintf() do not work
 * with such console windows.  drsym_write_to_console() can be used instead.
 */
bool
drsym_using_console(void);

DR_EXPORT
/**
 * Writes a message to standard error in the current console window.
 * This can be used as a work around for Issue 261 where DR's
 * dr_printf() and dr_fprintf() do not work with console windows
 * (i.e., the \p cmd window).
 *
 * Unfortunately there are significant limitations to this console
 * printing support:
 * 
 *  - On Windows versions prior to Vista, it does not work from
 *    the exit event.  Once the application terminates its state with
 *    csrss (toward the very end of ExitProcess), no output will show
 *    up on the console.  We have no good solution here yet as exiting
 *    early is not ideal.
 *  - It does not work at all from graphical applications, even when they are
 *    launched from a console.
 *  - In the future, with earliest injection (Issue 234), writing to the
 *    console may not work from the client init event.
 *
 * @param[in] fmt Format string, followed by printf-style args to print.
 */
bool
drsym_write_to_console(const char *fmt, ...);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRSYMS_H_ */
