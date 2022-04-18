/* **********************************************************
 * Copyright (c) 2013-2021 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _DR_FRONTEND_LIB_H_
#define _DR_FRONTEND_LIB_H_

/****************************************************************************
 * Tool front-end API
 */
/**
 * @file dr_frontend.h
 * @brief Tool front-end API.  Use these functions to search for and query
 * the properties of a target application file, check environment variables,
 * and perform other common actions in a tool front-end executable.
 * The library provides cross-platform utilities that support internationalization.
 *
 * The general usage model is for the front-end executable to always deal with
 * UTF-8 strings and let this front-end library perform conversion back and
 * forth to UTF-16 when interacting with Windows library.  The executable should
 * declare its main routine as follows:
 *
 *   int _tmain(int argc, TCHAR *targv[])
 *
 * And then invoke drfront_convert_args() to convert them to UTF-8.  All further
 * references to the arguments should use the UTF-8 versions.
 * On Linux or MacOS, _tmain and TCHAR will turn into regular symbols.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Support use independently from dr_api.h */
#if !defined(WINDOWS) && (defined(_WIN32) || defined(_WIN64))
#    define WINDOWS 1
#endif
#ifndef IN
#    define IN /* marks input param */
#endif
#ifndef OUT
#    define OUT /* marks output param */
#endif
#ifndef INOUT
#    define INOUT /* marks input+output param */
#endif
#ifndef WINDOWS
#    include <unistd.h> /* for ssize_t */
#endif
#if defined(WINDOWS) && !defined(_DR_API_H) && !defined(_SSIZE_T_DEFINED)
#    if defined(_WIN64)
typedef __int64 ssize_t;
#    else
typedef int ssize_t;
#    endif
#endif

#ifdef WINDOWS
#    include <tchar.h>
#else
/* XXX i#1079: We may want to share the define block with DR's libutil/our_tchar.h
 * b/c we'll want something similar for drdeploy and drinject libs and tools.
 */
#    define TCHAR char
#    define _tmain main
#    define _tcslen strlen
#    define _tcsstr strstr
#    define _tcscmp strcmp
#    define _tcsnicmp strnicmp
#    define _tcsncpy strncpy
#    define _tcscat_s strcat
#    define _tcsrchr strrchr
#    define _snprintf snprintf
#    define _sntprintf snprintf
#    define _ftprintf fprintf
#    define _tfopen fopen
#    define _T(s) s
#endif
#ifdef _UNICODE
#    define TSTR_FMT "%S"
#else
#    define TSTR_FMT "%s"
#endif

/** Status code for each DRFront operation */
typedef enum {
    DRFRONT_SUCCESS,                 /**< Operation succeeded */
    DRFRONT_ERROR,                   /**< Operation failed */
    DRFRONT_ERROR_INVALID_PARAMETER, /**< Operation failed: invalid parameter */
    DRFRONT_ERROR_INVALID_SIZE,      /**< Operation failed: invalid size */
    DRFRONT_ERROR_FILE_EXISTS,       /**< Operation failed: dir or file already
                                      *   exists */
    DRFRONT_ERROR_INVALID_PATH,      /**< Operation failed: wrong path */
    DRFRONT_ERROR_ACCESS_DENIED,     /**< Operation failed: access denied */
    DRFRONT_ERROR_LIB_UNSUPPORTED,   /**< Operation failed: old version
                                      *   or invalid library */
} drfront_status_t;

/** Permission modes for drfront_access() */
typedef enum {
    DRFRONT_EXIST = 0x00, /**< Test existence */
    DRFRONT_EXEC = 0x01,  /**< Test for execute access */
    DRFRONT_WRITE = 0x02, /**< Test for write access */
    DRFRONT_READ = 0x04,  /**< Test for read access */
} drfront_access_mode_t;

/**
 * Checks \p fname for the permssions specified by \p mode for the
 * current effective user.  If \p fname is a directory and \p mode includes \p
 * DRFRONT_WRITE, this function additionally attempts to create a
 * temporary file (by calling drfront_dir_try_writable()) to ensure
 * that the filesystem is not mounted read-only.
 * On Linux or Mac, if the current effective user is 0, this routine assumes
 * that the user has read and write access to every file and has execute
 * access to any file with at least one execute bit set.
 *
 * \note DRFRONT_EXEC is ignored on Windows because _waccess doesn't test it.
 *
 * @param[in]  fname    The filename to test permission to.
 * @param[in]  mode     The permission to test the file for.
 * @param[out] ret      True iff \p fname has the permission specified by \p mode.
 */
drfront_status_t
drfront_access(const char *fname, drfront_access_mode_t mode, OUT bool *ret);

/**
 * Implements a normal path search for \p fname on the paths in \p env_var.
 * Resolves symlinks.
 *
 * @param[in]  fname             The filename to search for.
 * @param[in]  env_var           The environment variable that contains the paths to
 *                               search for \p fname.
 *                               Paths are ;-separated on Windows and :-separated on UNIX.
 * @param[out] full_path         The full path of \p fname if it is found.
 * @param[in]  full_path_size    The maximum size of \p full_path.
 * @param[out] ret               True iff \p fname is found.
 */
drfront_status_t
drfront_searchenv(const char *fname, const char *env_var, OUT char *full_path,
                  const size_t full_path_size, OUT bool *ret);

/**
 * Concatenate onto a buffer. The buffer is not resized if the content does not fit.
 *
 * @param[in,out] buf      The buffer to concatenate onto.
 * @param[in]     bufsz    The allocated size of \p buf.
 * @param[in,out] sofar    The number of bytes added so far. Cumulative between calls.
 * @param[out]    len      The number of bytes sucessfully written to \p buf this call.
 * @param[in]     fmt      The format string to be added to \p buf.
 * @param[in]     ...      Any data needed for the format string.
 */
drfront_status_t
drfront_bufprint(INOUT char *buf, size_t bufsz, INOUT size_t *sofar, OUT ssize_t *len,
                 const char *fmt, ...);

/**
 * Converts from UTF-16 to UTF-8.
 *
 * \note On UNIX the information is simply copied from \p wstr to \p buf.
 *
 * \note Always null-terminates.
 *
 * @param[in]  wstr      The UTF-16 string to be converted to UTF-8.
 * @param[out] buf       The destination of the new UTF-8 string.
 * @param[in]  buflen    The allocated size of \p buf in elements.
 */
drfront_status_t
drfront_tchar_to_char(const TCHAR *wstr, OUT char *buf, size_t buflen /*# elements*/);

/**
 * Gets the necessary size of a UTF-8 buffer to hold the content in \p wstr.
 *
 * \note needed includes the terminating null character.
 *
 * @param[in]  wstr      The UTF-16 string to be converted later.
 * @param[out] needed    The size a buffer has to be to hold \p wstr in UTF-8.
 */
drfront_status_t
drfront_tchar_to_char_size_needed(const TCHAR *wstr, OUT size_t *needed);

/**
 * Converts from UTF-8 to UTF-16.
 *
 * \note On UNIX the information is simply copied from \p str to \p wbuf.
 *
 * \note Always null-terminates.
 *
 * @param[in]  str        The UTF-8 string to be converted to UTF-16.
 * @param[out] wbuf       The destination of the new UTF-16 string.
 * @param[in]  wbuflen    The allocated size of \p wbuf in elements.
 */
drfront_status_t
drfront_char_to_tchar(const char *str, OUT TCHAR *wbuf, size_t wbuflen /*# elements*/);

/**
 * Stores the contents of the environment variable \p name in \p buf.
 *
 * @param[in]  name      The name of the environment variable.
 * @param[out] buf       The destination to store the contents of \p name.
 * @param[in]  buflen    The allocated size of \p buf in elements.
 */
drfront_status_t
drfront_get_env_var(const char *name, OUT char *buf, size_t buflen /*# elements*/);

/**
 * Gets the absolute path of \p src.
 *
 * @param[in]  src       The path to expand.
 * @param[out] buf       The buffer to place the absolute path in.
 * @param[in]  buflen    The allocated size of \p buf in elements.
 */
drfront_status_t
drfront_get_absolute_path(const char *src, OUT char *buf, size_t buflen /*# elements*/);

/**
 * Gets the full path of \p app, which is located by searching the PATH if necessary.
 *
 * @param[in]  app       The executable to get the full path of.
 * @param[out] buf       The buffer to place the full path in if found.
 * @param[in]  buflen    The allocated size of \p buf in elements.
 */
drfront_status_t
drfront_get_app_full_path(const char *app, OUT char *buf, size_t buflen /*# elements*/);

/**
 * Reads the file header to determine if \p exe is a 64-bit application.
 *
 * @param[in]  exe      The executable to check for the 64-bit flag.
 * @param[out] is_64    True if \p exe is a 64-bit application.
 * @param[out] also_32  True if \p exe is a multi-part binary that also contains
 *                      a 32-bit application.
 */
drfront_status_t
drfront_is_64bit_app(const char *exe, OUT bool *is_64, OUT bool *also_32);

/**
 * Reads the PE header to determine if \p exe has a GUI.
 *
 * \note This function always returns false on UNIX because it is not relevant.
 *
 * @param[in]  exe             The executable to check the subsystem of.
 * @param[out] is_graphical    True if \p exe's subsystem is graphical.
 */
drfront_status_t
drfront_is_graphical_app(const char *exe, OUT bool *is_graphical);

/**
 * Converts the command-line arguments to UTF-8.
 *
 * \note This function allocates memory on the heap that must be freed by
 *       calling drfront_cleanup_args().
 *
 * \note On UNIX the data is simply copied.
 *
 * @param[in]  targv    The original command-line arguments.
 * @param[out] argv     The original command-line arguments in UTF-8.
 * @param[in]  argc     The number of command-line arguments.
 */
drfront_status_t
drfront_convert_args(const TCHAR **targv, OUT char ***argv, int argc);

/**
 * Frees the UTF-8 array of command-line arguments.
 *
 * @param[in] argv    The array of command-line arguments.
 * @param[in] argc    The number of command-line arguments.
 */
drfront_status_t
drfront_cleanup_args(char **argv, int argc);

/**
 * If a tool is installed into a "Program Files" directory on Windows,
 * or into "/usr/..." on Linux, it needs to store its log files
 * elsewhere.  This utility function helps to select that alternative
 * location.  First, it checks whether \p root is in a location where
 * log files should not be created, and returns that result in \p
 * use_root.  If \p use_root is false, this function returns a
 * suggested alternative directory for log files in \p buf.  It looks
 * in standard locations such as "$APPDATA" or
 * "$USERPROFILE/Application Data" on Windows or in temp directories
 * if those fail or if on Linux.  It appends \p subdir to the base
 * application data or temp directory.  It is up to the caller to
 * create the returned directory if it does not exist.
 *
 * @param[in]  root      The location where the tool's binaries are installed.
 * @param[in]  subdir    A directory to append to the application data directory
 *                       to form a result in \p buf.
 * @param[out] use_root  Returns whether \p root is suitable for storing log files.
 * @param[out] buf       If \p use_root is false, this buffer is filled with a
 *                       suggested directory for storing log files.  The directory
 *                       ends with \p subdir.
 *                       If \p use_root is true, \p buf's contents are undefined.
 * @param[in]  buflen    The maximum capacity of \p buf, in elements.
 */
drfront_status_t
drfront_appdata_logdir(const char *root, const char *subdir, OUT bool *use_root,
                       OUT char *buf, size_t buflen /*# elements*/);

/**
 * Replace occurences of \p old_char with \p new_char in \p str.  Typically used to
 * canonicalize Windows paths into using forward slashes.
 *
 * @param[out] str       The string whose characters should be replaced.
 * @param[in]  old_char  Old character to be replaced.
 * @param[in]  new_char  New character to use.
 */
void
drfront_string_replace_character(OUT char *str, char old_char, char new_char);

/**
 * Replace occurences of \p old_char with \p new_char in TCHAR \p str.
 * Typically used to canonicalize Windows paths into using forward slashes.
 *
 * @param[out] str       A string whose characters should be replaced.
 * @param[in]  old_char  Old character to be replaced.
 * @param[in]  new_char  New character to use.
 */
void
drfront_string_replace_character_wide(OUT TCHAR *str, TCHAR old_char, TCHAR new_char);

/**
 * Sets the environment variable _NT_SYMBOL_PATH and the dbghelp
 * search path for symbol lookup in a client, without any network
 * symbol server component (such components are unsafe in a client).
 *
 * If the _NT_SYMBOL_PATH is already specified, this routine validates it and
 * if invalid replaces it.
 *
 * This routine also takes the client symbol lookup path and adds the Microsoft
 * symbol server for use in a frontend itself (not in a client) and returns that
 * path in \p symsrv_path.  The frontend can enable use of this path by calling
 * drfront_set_symbol_search_path().
 *
 * drfront_sym_init() must be called before calling this routine.
 *
 * \note This routine requires DbgHelp.dll 6.0 or later.
 *
 * \warning This routine will fail when using the system copy of dbghelp.dll on
 * Windows XP or 2003.  The client should use its own copy of dbghelp.dll
 * version 6.0 or later.
 *
 * @param[in]  symdir       A local symbol cache directory.
 *                          It can be passed without srv* prepended.
 *                          It will have "/symbols" appended to it.
 * @param[in]  ignore_env   If TRUE, any existing _NT_SYMBOL_PATH value is ignored.
 * @param[out] symsrv_path  Returns a symbol path that includes the Microsoft
 *                          symbol server.
 * @param[in]  symsrv_path_sz  The maximum length, in characters, of \p symsrv_path.
 */
drfront_status_t
drfront_set_client_symbol_search_path(const char *symdir, bool ignore_env,
                                      OUT char *symsrv_path, size_t symsrv_path_sz);

/**
 * Sets the symbol search path for this frontend process to the specified value.
 * Typically this would be used with the output value from
 * drfront_set_client_symbol_search_path().
 *
 * drfront_sym_init() must be called before calling this routine.
 *
 * \note The routine requires DbgHelp.dll 6.0 or later.
 *
 * \warning The routine will fail when using the system copy of dbghelp.dll on
 * Windows XP or 2003.  The client should use its own copy of dbghelp.dll
 * version 6.0 or later.
 *
 * @param[in]  symsrv_path  The symbol search path to use.
 */
drfront_status_t
drfront_set_symbol_search_path(const char *symsrv_path);

/**
 * This routine initializes the symbol handler for the current process. Should be called
 * before drfront_set_symbol_search_path() and drfront_fetch_module_symbols().
 *
 * \note The routine requires DbgHelp.dll 6.0 or later.
 *
 * \warning The routine will fail when using the system copy of dbghelp.dll on
 * Windows XP or 2003.  The client should use its own copy of dbghelp.dll
 * version 6.0 or later.
 *
 * @param[in] wsymsrv_path   The path, or series of paths separated by a semicolon (;),
 *                           that is used to search for symbol files.  If this parameter
 *                           is NULL, the library attempts to form a symbol path from
 *                           the following sources: the current working directory,
 *                           _NT_SYMBOL_PATH, _NT_ALTERNATE_SYMBOL_PATH.
 * @param[in] dbghelp_path   The path to dbghelp.dll.  If the string specifies a full
 *                           path, the routine looks only in that path for the module.
 *                           If the string specifies a relative path or a module name
 *                           without a path, the function uses a standard Windows library
 *                           search strategy to find the module.
 */
drfront_status_t
drfront_sym_init(const char *wsymsrv_path, const char *dbghelp_path);

/**
 * This routine deallocates all symbol-related resources associated with the current
 * process.
 */
drfront_status_t
drfront_sym_exit(void);

/**
 * This routine tries to fetch all missed symbols for module specified in \p modpath
 * using _NT_SYMBOL_PATH environment var.  User should call \p drfront_sym_init,
 * drfront_sym_set_search_path() and drfront_sym_set_search_path() before calling
 * this routine.  If success function returns full path to fetched symbol file in
 * \p symbol_path.
 *
 * \note The routine will fetch symbols from remote MS Symbol Server only if symbols
 *  don't exist in the search paths and _NT_SYMBOL_PATH has right srv* path & link.
 *  The routine requires DbgHelp.dll 6.0 or later.
 *
 * \warning The routine will fail when using the system copy of dbghelp.dll on
 * Windows XP or 2003.  The client should use its own copy of dbghelp.dll
 * version 6.0 or later.
 *
 * @param[in] modpath    The name of the image to be loaded. This name can contain
 *                       a partial path, a full path, or no path at all.  If the file
 *                       cannot be located by the name provided, the routine returns
 *                       DRFRONT_OBJ_NOEXIST.
 * @param[in] symbol_path    The full path to fetched symbols file.
 * @param[in] symbol_path_sz  Size of \p symbol_path argument in characters.
 */
drfront_status_t
drfront_fetch_module_symbols(const char *modpath, OUT char *symbol_path,
                             size_t symbol_path_sz);

/**
 * This routine creates the directory specified in \p dir.
 *
 * @param[in] dir      New directory name.
 */
drfront_status_t
drfront_create_dir(const char *dir);

/**
 * This routine removes the empty directory specified in \p dir.
 *
 * @param[in] dir      Name of directory to remove.
 */
drfront_status_t
drfront_remove_dir(const char *dir);

/**
 * This routine checks whether \p path is a valid directory.
 *
 * @param[in]  path      The path to be checked
 * @param[out] is_dir    Returns whether \p path is a valid directory.
 */
drfront_status_t
drfront_dir_exists(const char *path, OUT bool *is_dir);

/**
 * This routine checks whether a file can be created inside the
 * directory specified by \p path.
 *
 * @param[in]  path         The path to be checked
 * @param[out] is_writable  Returns whether files can be created in \p path.
 */
drfront_status_t
drfront_dir_try_writable(const char *path, OUT bool *is_writable);

/**
 * Sets the verbosity level for additional diagnostics from the drfrontendlib
 * library.  The default level is 0 which is quiet.  Diagnostics are printed
 * to stderr.
 *
 * @param[in] verbosity  The new verbosity level.  Typical values are 0 through 3.
 */
drfront_status_t
drfront_set_verbose(int verbosity);

#ifdef __cplusplus
}
#endif

#endif
