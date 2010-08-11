/* **********************************************************
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
} drsym_error_t;

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
 */
drsym_error_t
drsym_lookup_address(const char *modpath, size_t modoffs, drsym_info_t *info /*INOUT*/);

DR_EXPORT
/**
 * Retrieves the address for a given symbol name.
 *
 * @param[in] modpath The full path to the module to be queried.
 * @param[in] symbol The name of the symbol being queried.
 *   To specify a target module, pass "modulename!symbolname" as the symbol
 *   string to look up.
 * @param[out] modoffs The offset from the base of the module specifying the address
 *   of the specified symbol.
 */
drsym_error_t
drsym_lookup_symbol(const char *modpath, const char *symbol, size_t *modoffs /*OUT*/);

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
 *  - It does not work from the exit event.  Once the application terminates
 *    its state with csrss (toward the very end of ExitProcess), no output
 *    will show up on the console.  We have no good solution here yet as exiting
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

#endif /* _DRSYMS_H_ */
