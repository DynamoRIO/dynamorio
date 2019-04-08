/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
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
 * Currently supports Windows PDB, ELF symtab, Windows PECOFF,
 * Mach-O symtab, and DWARF on Windows, Linux, and MacOS.
 * No stabs support yet.
 *
 * This API will eventually support both sideline (via a separate
 * process) and online use.  Today only online use is supported.
 * Although there are new proposals that do not need sideline and rely
 * on using pre-populated symbol caches for symbols needed to run correctly
 * combined with post-processing for symbols only needed for presentation
 * that are gaining favor, making sideline no longer a certainty.
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
    DRSYM_SUCCESS,                     /**< Operation succeeded. */
    DRSYM_ERROR,                       /**< Operation failed. */
    DRSYM_ERROR_INVALID_PARAMETER,     /**< Operation failed: invalid parameter */
    DRSYM_ERROR_INVALID_SIZE,          /**< Operation failed: invalid size */
    DRSYM_ERROR_LOAD_FAILED,           /**< Operation failed: unable to load symbols */
    DRSYM_ERROR_SYMBOL_NOT_FOUND,      /**< Operation failed: symbol not found */
    DRSYM_ERROR_LINE_NOT_AVAILABLE,    /**< Operation failed: line info not available */
    DRSYM_ERROR_NOT_IMPLEMENTED,       /**< Operation failed: not yet implemented */
    DRSYM_ERROR_FEATURE_NOT_AVAILABLE, /**< Operation failed: not available */
    DRSYM_ERROR_NOMEM,                 /**< Operation failed: not enough memory */
    DRSYM_ERROR_RECURSIVE, /**< Operation failed: unavailable when recursive */
} drsym_error_t;

/** Bitfield of options to each DRSyms operation. */
typedef enum {
    /**
     * Do not demangle C++ symbols.  This option is not available for
     * Windows PDB symbols.
     */
    DRSYM_LEAVE_MANGLED = 0x00,
    /**
     * Demangle C++ symbols, omitting templates and parameter types.
     * For all symbol types, templates are collapsed to <> while function
     * parameters are omitted entirely (without any parentheses).
     */
    DRSYM_DEMANGLE = 0x01,
    /**
     * Demangle template arguments and parameter types.  This option is not
     * available for Windows PDB symbols (except in drsym_demangle_symbol()).
     */
    DRSYM_DEMANGLE_FULL = 0x02,
    /** For Windows PDB, do not collapse templates to <>. */
    DRSYM_DEMANGLE_PDB_TEMPLATES = 0x04,
    /**
     * Windows-only, for drsym_search_symbols_ex().
     * Requests a full search for all symbols and not just functions.
     * This adds overhead: see drsym_search_symbols_ex() for details.
     */
    DRSYM_FULL_SEARCH = 0x08,
    DRSYM_DEFAULT_FLAGS = DRSYM_DEMANGLE, /**< Default flags. */
} drsym_flags_t;

/**
 * Bitfield indicating the availability of different kinds of debugging
 * information for a module.  The first 8 bits are reserved for platform
 * independent qualities of the debug info, while the rest indicate exactly
 * which kind of debug information is present.
 */
typedef enum {
    DRSYM_SYMBOLS = (1 << 0),   /**< Any symbol information beyond exports. */
    DRSYM_LINE_NUMS = (1 << 1), /**< Any line number info. */
    /* Platform-dependent types. */
    DRSYM_ELF_SYMTAB = (1 << 8),     /**< ELF .symtab symbol names. */
    DRSYM_DWARF_LINE = (1 << 9),     /**< DWARF line info. */
    DRSYM_PDB = (1 << 10),           /**< Windows PDB files. */
    DRSYM_PECOFF_SYMTAB = (1 << 11), /**< PE COFF (Cygwin or MinGW) symbol table names.*/
    DRSYM_MACHO_SYMTAB = (1 << 12),  /**< Mach-O symbol table names. */
} drsym_debug_kind_t;

/** Data structure that holds symbol information */
typedef struct _drsym_info_t {
    /* INPUTS */
    /** Input: should be set by caller to sizeof(drsym_info_t) */
    size_t struct_size;
    /** Input: should be set by caller to the size of the name buffer, in bytes */
    size_t name_size;
    /** Input: should be set by caller to the size of the file buffer, in bytes */
    size_t file_size;

    /* OUTPUTS */
    /**
     * Output: size of data available for file (not including terminating null).
     * Only file_size bytes will be copied to file.
     */
    size_t file_available_size;
    /**
     * Output: file name (storage allocated by caller, of size file_size).
     * Guaranteed to be null-terminated.
     * Optional: can be set to NULL.
     */
    char *file;
    /** Output: line number */
    uint64 line;
    /** Output: offset from address that starts at line */
    size_t line_offs;

    /**
     * Output: offset from module base of start of symbol.
     * For Mach-O executables, the module base is after any __PAGEZERO segment.
     */
    size_t start_offs;
    /**
     * Output: offset from module base of end of symbol.
     * \note For DRSYM_PECOFF_SYMTAB (Cygwin or MinGW) or DRSYM_MACHO_SYMTAB (MacOS)
     * symbols, the end offset is not known precisely.
     * The start address of the subsequent symbol will be stored here.
     **/
    size_t end_offs;

    /** Output: type of the debug info available for this module */
    drsym_debug_kind_t debug_kind;
    /** Output: type id for passing to drsym_expand_type() */
    uint type_id;

    /**
     * Output: size of data available for name (not including terminating null).
     * Only name_size bytes will be copied to name.
     */
    size_t name_available_size;
    /**
     * Output: symbol name (storage allocated by caller, of size name_size).
     * Guaranteed to be null-terminated.
     * Optional: can be set to NULL.
     */
    char *name;

    /** Output: the demangling status of the symbol, as drsym_flags_t values. */
    uint flags;
} drsym_info_t;

#ifdef WINDOWS
#    define IF_WINDOWS_ELSE(x, y) x
#else
#    define IF_WINDOWS_ELSE(x, y) y
#endif

DR_EXPORT
/**
 * Initialize the symbol access library.
 * Can be called multiple times (by separate components, normally) but
 * each call must be paired with a corresponding call to
 * drsym_exit(), and only the symbol server parameter of the first
 * call will be honored.
 *
 * @param[in] shmid Identifies the symbol server for sideline operation.
 * \note Sideline operation is not yet implemented.
 */
drsym_error_t drsym_init(IF_WINDOWS_ELSE(const wchar_t *, int) shmid);

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
 *   For Mach-O executables, the module base is after any __PAGEZERO segment.
 * @param[in,out] info Information about the symbol at the queried address.
 * @param[in]  flags   Options for the operation as a combination of drsym_flags_t
 *    values.  Ignored for Windows PDB (DRSYM_PDB) except for
 *    DRSYM_DEMANGLE_PDB_TEMPLATES.
 */
drsym_error_t
drsym_lookup_address(const char *modpath, size_t modoffs, drsym_info_t *info /*INOUT*/,
                     uint flags);

enum {
    DRSYM_TYPE_OTHER,    /**< Unknown type, cannot downcast. */
    DRSYM_TYPE_INT,      /**< Integer, cast to drsym_int_type_t. */
    DRSYM_TYPE_PTR,      /**< Pointer, cast to drsym_ptr_type_t. */
    DRSYM_TYPE_FUNC,     /**< Function, cast to drsym_func_type_t. */
    DRSYM_TYPE_VOID,     /**< Void.  No further information. */
    DRSYM_TYPE_COMPOUND, /**< Struct, union, or class; cast to drsym_compound_type_t. */
    DRSYM_TYPE_ARRAY,    /** <Array cast to drsym_array_type_t. */
    /* Additional type kinds will be added as needed. */
};

/**
 * Base type information.
 * Use the 'kind' member to downcast to a more specific type.
 */
typedef struct _drsym_type_t {
    uint kind;   /**< Type kind, i.e. DRSYM_TYPE_INT or DRSYM_TYPE_PTR. */
    size_t size; /**< Type size. */
    uint id;     /**< Unique identifier to pass to drsym_expand_type(). */
} drsym_type_t;

/** Type information for a function. */
typedef struct _drsym_func_type_t {
    drsym_type_t type;
    drsym_type_t *ret_type;
    int num_args; /**< Number of arguments. */
    /** Separate array of size num_args.  May be NULL if the data was not requested. */
    drsym_type_t **arg_types;
} drsym_func_type_t;

/** Type information for a user-defined type: a struct, union, or class. */
typedef struct _drsym_compound_type_t {
    drsym_type_t type;
    char *name;     /**< Name of the type. */
    int num_fields; /**< Number of fields. */
    /** Separate array of size num_fields.  May be NULL if the data was not requested. */
    drsym_type_t **field_types;
} drsym_compound_type_t;

/** Type information for an integer base type. */
typedef struct _drsym_int_type_t {
    drsym_type_t type;
    bool is_signed;
} drsym_int_type_t;

/** Type information for a pointer & array types. */
typedef struct _drsym_ptr_type_t {
    drsym_type_t type;
    drsym_type_t *elt_type;
} drsym_ptr_type_t;

DR_EXPORT
/**
 * Retrieves symbol type information for a given module offset.  After a
 * successful execution, \p *type points to the function type.  All memory
 * used to represent the types comes from \p buf, so the caller only needs to
 * dispose \p buf to free them.  Returns DRSYM_ERROR_NOMEM if the buffer is not
 * big enough.
 *
 * This routine expands arguments of function or fields compound types into
 * their child types as far as \p levels_to_expand levels deep.
 * The resulting expanded type tree may be recursive, so the caller should take
 * care when traversing it.  Any already-referenced type is guaranteed to have
 * a smaller pointer value than its parent.  This allows safe traversing without
 * recording the already-seen types.
 *
 * When querying type information for a symbol that has already been located
 * via drsym_lookup_address() or via enumeration or searching, it is better
 * to invoke drsym_expand_type() using the \p type_id parameter of #drsym_info_t
 * to properly handle cases where multiple symbols map to the same module offset.
 *
 * \note This function is currently implemented only for Windows PDB
 * symbols (DRSYM_PDB).
 *
 * @param[in] modpath    The full path to the module to be queried.
 * @param[in] modoffs    The offset from the base of the module specifying
 *                       the start address of the function.
 *                       For Mach-O, the module base is after any __PAGEZERO segment.
 * @param[in] levels_to_expand  The maximum levels of sub-types to expand.
 *                       Set to UINT_MAX for unlimited expansion.
 *                       Further expansion can be performed by calling
 *                       drsym_expand_type().
 * @param[out] buf       Memory used for the types.
 * @param[in] buf_sz     Number of bytes in \p buf.
 * @param[out] type      Pointer to the type of the function.
 */
drsym_error_t
drsym_get_type(const char *modpath, size_t modoffs, uint levels_to_expand, char *buf,
               size_t buf_sz, drsym_type_t **type /*OUT*/);

DR_EXPORT
/**
 * Retrieves symbol type information for a given \p type_name.  After a
 * successful execution, \p *type points to the type of the symbol.  All memory
 * used to represent the type comes from \p buf, so the caller only needs to
 * dispose \p buf to free everything.  Returns DRSYM_ERROR_NOMEM if the buffer is not
 * big enough.
 *
 * \note This function is currently implemented only for Windows PDB
 * symbols (DRSYM_PDB).
 *
 * @param[in] modpath    The full path to the module to be queried.
 * @param[in] type_name  The string name of the base type.
 * @param[out] buf       Memory used for the type.
 * @param[in] buf_sz     Number of bytes in \p buf.
 * @param[out] type      Pointer to the type for the symbol.
 */
drsym_error_t
drsym_get_type_by_name(const char *modpath, const char *type_name, char *buf,
                       size_t buf_sz, drsym_type_t **type /*OUT*/);

DR_EXPORT
/**
 * Retrieves function type information for a given module offset.  After a
 * successful execution, \p *func_type points to the function type.  All memory
 * used to represent the types comes from \p buf, so the caller only needs to
 * dispose \p buf to free them.  Returns DRSYM_ERROR_NOMEM if the buffer is not
 * big enough.
 *
 * This routine does not expand arguments of function or compound types into
 * their child types.
 *
 * \note This function is currently implemented only for Windows PDB
 * symbols (DRSYM_PDB).
 *
 * \note The public Windows symbol files typically do not contain type
 * information for function parameters.
 *
 * \note Despite the name, this routine will retrieve type information
 * for non-functions as well.
 *
 * @param[in] modpath    The full path to the module to be queried.
 * @param[in] modoffs    The offset from the base of the module specifying
 *                       the start address of the function.
 *                       For Mach-O, the module base is after any __PAGEZERO segment.
 * @param[out] buf       Memory used for the types.
 * @param[in] buf_sz     Number of bytes in \p buf.
 * @param[out] func_type Pointer to the type of the function.
 */
drsym_error_t
drsym_get_func_type(const char *modpath, size_t modoffs, char *buf, size_t buf_sz,
                    drsym_func_type_t **func_type /*OUT*/);

DR_EXPORT
/**
 * Retrieves symbol type information for all sub-types of the type
 * referenced by \p type_id.  After a
 * successful execution, \p *expanded_type points to the expanded type.  All memory
 * used to represent the types comes from \p buf, so the caller only needs to
 * dispose \p buf to free them.  Returns DRSYM_ERROR_NOMEM if the buffer is not
 * big enough.
 *
 * The initial type index to pass as \p type_id can be retrieved by
 * calling drsym_get_type() or drsym_get_func_type().
 *
 * The resulting expanded type tree may be recursive, so the caller should take
 * care when traversing it.  Any already-referenced type is guaranteed to have
 * a smaller pointer value than its parent.  This allows safe traversing without
 * recording the already-seen types.
 *
 * \note This function is currently implemented only for Windows PDB
 * symbols (DRSYM_PDB).
 *
 * \note The public Windows symbol files typically do not contain type
 * information for function parameters.
 *
 * @param[in] modpath    The full path to the module to be queried.
 * @param[in] type_id    The type index, acquired from a prior call to
 *                       drsym_get_type() or drsym_get_func_type().
 * @param[in] levels_to_expand  The maximum levels of sub-types to expand.
 *                       Set to UINT_MAX for unlimited expansion.
 * @param[out] buf       Memory used for the types.
 * @param[in] buf_sz     Number of bytes in \p buf.
 * @param[out] expanded_type Pointer to the expanded type tree for the symbol.
 */
drsym_error_t
drsym_expand_type(const char *modpath, uint type_id, uint levels_to_expand, char *buf,
                  size_t buf_sz, drsym_type_t **expanded_type /*OUT*/);

DR_EXPORT
/**
 * Retrieves the address for a given symbol name.
 *
 * For Windows PDB symbols (DRSYM_PDB), we don't support the
 * DRSYM_DEMANGLE_FULL flag.  Also for Windows PDB, if DRSYM_DEMANGLE is
 * set, \p symbol must include the template arguments.  Consider
 * using drsym_search_symbols_ex() instead with "<*>" for each template
 * instantation in order to locate all symbols regardless of template
 * arguments.
 *
 * @param[in] modpath The full path to the module to be queried.
 * @param[in] symbol The name of the symbol being queried.
 *   To specify a target module, pass "modulename!symbolname" as the symbol
 *   string to look up.
 * @param[out] modoffs The offset from the base of the module specifying the address
 *   of the specified symbol.
 *   For Mach-O, the module base is after any __PAGEZERO segment.
 * @param[in]  flags   Options for the operation as a combination of drsym_flags_t
 *    values.  Ignored for Windows PDB (DRSYM_PDB).
 */
drsym_error_t
drsym_lookup_symbol(const char *modpath, const char *symbol, size_t *modoffs /*OUT*/,
                    uint flags);

/**
 * Type for drsym_enumerate_symbols and drsym_search_symbols callback function.
 * Returns whether to continue the enumeration or search.
 *
 * @param[in]  name    Name of the symbol.
 * @param[in]  modoffs Offset of the symbol from the module base.
 *                     For Mach-O, the module base is after any __PAGEZERO segment.
 * @param[in]  data    User parameter passed to drsym_enumerate_symbols() or
 *                     drsym_search_symbols().
 */
typedef bool (*drsym_enumerate_cb)(const char *name, size_t modoffs, void *data);

/**
 * Type for drsym_enumerate_symbols_ex and drsym_search_symbols_ex callback function.
 * Returns whether to continue the enumeration or search.
 *
 * @param[in]  info    Information about the symbol.
 * @param[in]  status  Status of \p info: either DRSYM_SUCCESS to indicate that all
 *                     fields are filled in, or DRSYM_ERROR_LINE_NOT_AVAILABLE to
 *                     indicate that line number information was not obtained.  Line
 *                     information is currently not available for any debug type when
 *                     iterating.  This has no bearing on whether line information is
 *                     available when calling drsym_lookup_address().
 * @param[in]  data    User parameter passed to drsym_enumerate_symbols() or
 *                     drsym_search_symbols().
 */
typedef bool (*drsym_enumerate_ex_cb)(drsym_info_t *info, drsym_error_t status,
                                      void *data);

DR_EXPORT
/**
 * Enumerates all symbol information for a given module, including exports.
 * Calls the given callback function for each symbol.
 * If the callback returns false, the enumeration will end.
 *
 * @param[in] modpath   The full path to the module to be queried.
 * @param[in] callback  Function to call for each symbol found.
 * @param[in] data      User parameter passed to callback.
 * @param[in] flags     Options for the operation as a combination of drsym_flags_t
 *    values.  Ignored for Windows PDB (DRSYM_PDB) except for
 *    DRSYM_DEMANGLE_PDB_TEMPLATES.
 */
drsym_error_t
drsym_enumerate_symbols(const char *modpath, drsym_enumerate_cb callback, void *data,
                        uint flags);

DR_EXPORT
/**
 * Enumerates all symbol information for a given module, including exports.
 * Calls the given callback function for each symbol, passing full information
 * about the symbol (as opposed to selected information returned by
 * drsym_enumerate_symbols()).
 * If the callback returns false, the enumeration will end.
 *
 * @param[in] modpath   The full path to the module to be queried.
 * @param[in] callback  Function to call for each symbol found.
 * @param[in] info_size The size of the drsym_info_t struct to pass to \p callback.
 *                      Enough space for each name will be allocated automatically.
 * @param[in] data      User parameter passed to callback.
 * @param[in] flags     Options for the operation as a combination of drsym_flags_t
 *    values.  Ignored for Windows PDB (DRSYM_PDB) except for
 *    DRSYM_DEMANGLE_PDB_TEMPLATES.
 */
drsym_error_t
drsym_enumerate_symbols_ex(const char *modpath, drsym_enumerate_ex_cb callback,
                           size_t info_size, void *data, uint flags);

DR_EXPORT
/**
 * Given a mangled or decorated C++ symbol, outputs the source name into \p dst.
 * If the unmangled name requires more than \p dst_sz bytes, it is truncated and
 * null-terminated to fit into \p dst.  If the unmangling fails, \p symbol is
 * copied as-is into \p dst, and truncated and null-terminated to fit.
 * Returns zero if the name could not be unmangled; else returns the number of
 * characters in the unmangled name, including the terminating null.
 *
 * If the unmangling is successful but \p dst is too small to hold it, returns
 * the number of characters required to store the name, including the
 * terminating null, just as in a successful case.  On Linux or for Windows PDB,
 * \p symbol is copied as-is into \p dst just like for unmangling failure; for
 * Windows PECOFF, the unmangled name is copied, truncated to fit, and
 * null-terminated.
 *
 * If there was overflow, the return value may be an estimate of the required
 * size, so a second attempt with the return value is not guaranteed to be
 * successful.  If the caller needs the full name, they may need to make
 * multiple attempts with a larger buffer.
 *
 * \note If the passed-in mangled symbol is truncated already, there
 * is no guarantee that the demangling will even partically succeed.
 *
 * @param[out] dst      Output buffer for demangled name.
 * @param[in]  dst_sz   Size of the output buffer in bytes.
 * @param[in]  mangled  Mangled C++ symbol to demangle.
 * @param[in]  flags    Options for the operation as a combination of drsym_flags_t
 *    values.  DRSYM_DEMANGLE is implied.
 */
size_t
drsym_demangle_symbol(char *dst, size_t dst_sz, const char *mangled, uint flags);

DR_EXPORT
/**
 * Outputs the kind of debug information available for the module \p modpath in
 * \p *kind if the operation succeeds.
 *
 * @param[in]  modpath   The full path to the module to be queried.
 * @param[out] kind      The kind of debug information available.
 */
drsym_error_t
drsym_get_module_debug_kind(const char *modpath, drsym_debug_kind_t *kind /*OUT*/);

DR_EXPORT
/**
 * Returns whether debug information is available for the module \p modpath.
 * This can be faster than calling drsym_get_module_debug_kind(), but it
 * should be equivalent to calling drsym_get_module_debug_kind() and
 * looking for DRSYM_SYMBOLS in the output.
 *
 * @param[in]  modpath   The full path to the module to be queried.
 */
drsym_error_t
drsym_module_has_symbols(const char *modpath);

#ifdef WINDOWS
DR_EXPORT
/**
 * Enumerates all symbol information (including exports) matching a
 * pattern for a given module.
 * Calls the given callback function for each matching symbol.
 * If the callback returns false, the enumeration will end.
 *
 * This routine is only supported for PDB symbols (DRSYM_PDB).
 *
 * Modifying the demangling is currently not supported:
 * DRSYM_DEFAULT_FLAGS is used.
 *
 * Due to dbghelp leaving template parameters in place (they are removed
 * for DRSYM_DEMANGLE in the drsyms library itself), searching may find
 * symbols that do not appear to match due to drsyms having removed that
 * part of the symbol name.
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

DR_EXPORT
/**
 * Enumerates all symbol information (including exports) matching a
 * pattern for a given module.
 * Calls the given callback function for each symbol, passing full information
 * about the symbol (as opposed to selected information returned by
 * drsym_search_symbols()).
 * If the callback returns false, the enumeration will end.
 *
 * This routine is only supported for PDB symbols (DRSYM_PDB).
 *
 * Modifying the demangling is currently not supported:
 * DRSYM_DEFAULT_FLAGS is used.
 *
 * Due to dbghelp leaving template parameters in place (they are removed
 * for DRSYM_DEMANGLE in the drsyms library itself), searching may find
 * symbols that do not appear to match due to drsyms having removed that
 * part of the symbol name.
 *
 * \note The performance note for drsym_search_symbols() applies here
 * as well.
 *
 * @param[in] modpath   The full path to the module to be queried.
 * @param[in] match     Regular expression describing the names of the symbols
 *                      to be enumerated.  To specify a target module, use the
 *                      "module_pattern!symbol_pattern" format.
 * @param[in] flags     Options for the operation as a combination of drsym_flags_t
 *                      values.  DRSYM_LEAVE_MANGLED and DRSYM_DEMANGLE_FULL are
 *                      ignored.  DRSYM_FULL_SEARCH, if set, requests to search all
 *                      symbols as opposed to the default of just functions.  A full
 *                      search takes significantly more time and memory and
 *                      eliminates the performance advantage over other lookup
 *                      methods.  A full search requires dbghelp.dll version 6.6 or
 *                      higher.
 * @param[in] callback  Function to call for each matching symbol found.
 * @param[in] info_size The size of the drsym_info_t struct to pass to \p callback.
 *                      Enough space for each name will be allocated automatically.
 * @param[in] data      User parameter passed to callback.
 */
drsym_error_t
drsym_search_symbols_ex(const char *modpath, const char *match, uint flags,
                        drsym_enumerate_ex_cb callback, size_t info_size, void *data);
#endif

DR_EXPORT
/**
 * Indicates that no further symbol queries will be performed and that
 * this module's debug information can be de-allocated.
 * A later query will result in a new load.
 * The typical usage is to call this after a series of queries at module
 * load.  For large applications with many libraries, calling this
 * can save hundreds of MB of memory.
 *
 * @param[in] modpath   The full path to the module to be unloaded.
 *
 * \note When called from within a callback for drsym_enumerate_symbols()
 * or drsym_search_symbols(), will fail with DRSYM_ERROR_RECURSIVE as it
 * is not safe to free resources while iterating.
 */
drsym_error_t
drsym_free_resources(const char *modpath);

/***************************************************************************
 * Line iteration
 */

/**
 * Line information returned by drsym_enumerate_lines().
 * This information should not be relied upon to be accessible
 * beyond the return of the callback routine to which it is passed.
 *
 * \note On Windows, we have observed abnormally large values for
 * \p line and \p line_addr (e.g., 0xfeefee or 0xe0b80000) in some
 * symbol files.  Be prepared for such values.
 */
typedef struct _drsym_line_info_t {
    /** Compilation unit (object file) name */
    const char *cu_name;
    /** Source file name */
    const char *file;
    /** Line number */
    uint64 line;
    /**
     * Offset from module base of the first instruction of the line.
     * For Mach-O executables, the module base is after any __PAGEZERO segment.
     */
    size_t line_addr;
} drsym_line_info_t;

/**
 * Type for drsym_enumerate_lines callback function.
 * Returns whether to continue the enumeration.
 *
 * @param[in]  info    Information about the line.
 * @param[in]  data    User parameter passed to drsym_enumerate_lines().
 */
typedef bool (*drsym_enumerate_lines_cb)(drsym_line_info_t *info, void *data);

DR_EXPORT
/**
 * Enumerates all source lines for a given module.
 * Calls the given callback function for each line.
 * If the callback returns false, the enumeration will end.
 *
 * If an individual compilation unit is missing line number information,
 * the callback will still be called once for that unit, with the fields other
 * than \p cu_name set to NULL or 0.
 *
 * @param[in] modpath   The full path to the module to be queried.
 * @param[in] callback  Function to call for each line.
 * @param[in] data      User parameter passed to callback.
 */
drsym_error_t
drsym_enumerate_lines(const char *modpath, drsym_enumerate_lines_cb callback, void *data);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRSYMS_H_ */
