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

#ifndef _DR_MODULES_H_
#define _DR_MODULES_H_ 1

/**
 * @file dr_modules.h
 * @brief Application module (library) querying routines.
 */

/**************************************************
 * MODULE INFORMATION TYPES
 */

/**
 * Type used for dr_get_proc_address().  This can be obtained from the
 * #_module_data_t structure.  It is equivalent to the base address of
 * the module on both Windows and Linux.
 */
/* Rather than using a void * for the module base, we forward declare a struct
 * that we never define.  This prevents usage errors such as passing a
 * module_data_t* to dr_get_proc_address().
 */
struct _module_handle_t;
typedef struct _module_handle_t *module_handle_t;

#ifdef WINDOWS

#    define MODULE_FILE_VERSION_INVALID ULLONG_MAX

/**
 * Used to hold .rsrc section version number information. This number is usually
 * presented as p1.p2.p3.p4 by PE parsing tools.
 */
typedef union _version_number_t {
    uint64 version; /**< Representation as a 64-bit integer. */
    struct {
        uint ms;    /**< */
        uint ls;    /**< */
    } version_uint; /**< Representation as 2 32-bit integers. */
    struct {
        ushort p2;   /**< */
        ushort p1;   /**< */
        ushort p4;   /**< */
        ushort p3;   /**< */
    } version_parts; /**< Representation as 4 16-bit integers. */
} version_number_t;

#endif

/**
 * Holds the names of a module.  This structure contains multiple
 * fields corresponding to different sources of a module name.  Note
 * that some of these names may not exist for certain modules.  It is
 * highly likely, however, that at least one name is available.  Use
 * dr_module_preferred_name() on the parent _module_data_t to get the
 * preferred name of the module.
 */
typedef struct _module_names_t {
    const char *module_name; /**< On windows this name comes from the PE header exports
                              * section (NULL if the module has no exports section).  On
                              * Linux the name will come from the ELF DYNAMIC program
                              * header (NULL if the module has no SONAME entry). */
    const char *file_name; /**< The file name used to load this module. Note - on Windows
                            * this is not always available. */
#ifdef WINDOWS
    const char *exe_name;  /**< If this module is the main executable of this process then
                            * this is the executable name used to launch the process (NULL
                            * for all other modules). */
    const char *rsrc_name; /**< The internal name given to the module in its resource
                            * section. Will be NULL if the module has no resource section
                            * or doesn't set this field within it. */
#else                      /* UNIX */
    uint64 inode;      /**< The inode of the module file mapped in. */
#endif
} module_names_t;

/** For dr_module_iterator_* interface */
typedef void *dr_module_iterator_t;

#ifdef UNIX
/** Holds information on a segment of a loaded module. */
typedef struct _module_segment_data_t {
    app_pc start;  /**< Start address of the segment, page-aligned backward. */
    app_pc end;    /**< End address of the segment, page-aligned forward. */
    uint prot;     /**< Protection attributes of the segment */
    uint64 offset; /**< Offset of the segment from the beginning of the backing file */
} module_segment_data_t;
#endif

/* We export copies of DR's internal the module_area_t to clients (in the form
 * of a module_data_t defined below) to avoid locking issues.
 */
/**
 * Holds information about a loaded module. \note On Linux the start address can be
 * cast to an Elf32_Ehdr or Elf64_Ehdr. \note On Windows the start address can be cast to
 * an IMAGE_DOS_HEADER for use in finding the IMAGE_NT_HEADER and its OptionalHeader.
 * The OptionalHeader can be used to walk the module sections (among other things).
 * See WINNT.H. \note On MacOS the start address can be cast to mach_header or
 * mach_header_64.
 * \note When accessing any memory inside the module (including header fields)
 * user is responsible for guarding against corruption and the possibility of the module
 * being unmapped.
 */
struct _module_data_t {
    union {
        app_pc start;           /**< starting address of this module */
        module_handle_t handle; /**< module_handle for use with dr_get_proc_address() */
    };                          /* anonymous union of start address and module handle */
    /**
     * Ending address of this module.  If the module is not contiguous
     * (which is common on MacOS, and can happen on Linux), this is the
     * highest address of the module, but there can be gaps in between start
     * and end that are either unmapped or that contain other mappings or
     * libraries.   Use the segments array to examine each mapped region,
     * and use dr_module_contains_addr() as a convenience routine, rather than
     * checking against [start..end).
     */
    app_pc end;

    app_pc entry_point; /**< entry point for this module as specified in the headers */

    uint flags; /**< Reserved, set to 0 */

    module_names_t names; /**< struct containing name(s) for this module; use
                           * dr_module_preferred_name() to get the preferred name for
                           * this module */

    char *full_path; /**< full path to the file backing this module */

#ifdef WINDOWS
    version_number_t file_version;    /**< file version number from .rsrc section */
    version_number_t product_version; /**< product version number from .rsrc section */
    uint checksum;                    /**< module checksum from the PE headers */
    uint timestamp;                   /**< module timestamp from the PE headers */
    /** Module internal size (from PE headers SizeOfImage). */
    size_t module_internal_size;
#else
    bool contiguous;   /**< whether there are no gaps between segments */
    uint num_segments; /**< number of segments */
    /**
     * Array of num_segments entries, one per segment.  The array is sorted
     * by the start address of each segment.
     */
    module_segment_data_t *segments;
    uint timestamp;             /**< Timestamp from ELF Mach-O headers. */
#    ifdef MACOS
    uint current_version;       /**< Current version from Mach-O headers. */
    uint compatibility_version; /**< Compatibility version from Mach-O headers. */
    byte uuid[16];              /**< UUID from Mach-O headers. */
#    endif
#endif
    app_pc preferred_base; /**< The preferred base address of the module. */
    /* We can add additional fields to the end without breaking compatibility. */
};

/**************************************************
 * MODULE INFORMATION ROUTINES
 */

DR_API
/**
 * Looks up the module containing \p pc.  If a module containing \p pc is found
 * returns a module_data_t describing that module.  Returns NULL if \p pc is
 * outside all known modules, which is the case for most dynamically generated
 * code.  Can be used to obtain a module_handle_t for dr_lookup_module_section()
 * or dr_get_proc_address() via the \p handle field inside module_data_t.
 *
 * \note Returned module_data_t must be freed with dr_free_module_data().
 */
module_data_t *
dr_lookup_module(byte *pc);

DR_API
/**
 * Looks up the module with name \p name ignoring case.  If an exact name match is found
 * returns a module_data_t describing that module else returns NULL.  User must call
 * dr_free_module_data() on the returned module_data_t once finished. Can be used to
 * obtain a module_handle_t for dr_get_proc_address().
 * \note Returned module_data_t must be freed with dr_free_module_data().
 */
module_data_t *
dr_lookup_module_by_name(const char *name);

DR_API
/**
 * Looks up module data for the main executable.
 * \note Returned module_data_t must be freed with dr_free_module_data().
 */
module_data_t *
dr_get_main_module(void);

DR_API
/**
 * Initialize a new module iterator.  The returned module iterator contains a snapshot
 * of the modules loaded at the time it was created.  Use dr_module_iterator_hasnext()
 * and dr_module_iterator_next() to walk the loaded modules.  Call
 * dr_module_iterator_stop() when finished to release the iterator. \note The iterator
 * does not prevent modules from being loaded or unloaded while the iterator is being
 * walked.
 */
dr_module_iterator_t *
dr_module_iterator_start(void);

DR_API
/**
 * Returns true if there is another loaded module in the iterator.
 */
bool
dr_module_iterator_hasnext(dr_module_iterator_t *mi);

DR_API
/**
 * Retrieves the module_data_t for the next loaded module in the iterator. User must call
 * dr_free_module_data() on the returned module_data_t once finished.
 * \note Returned module_data_t must be freed with dr_free_module_data().
 */
module_data_t *
dr_module_iterator_next(dr_module_iterator_t *mi);

DR_API
/**
 * User should call this routine to free the module iterator.
 */
void
dr_module_iterator_stop(dr_module_iterator_t *mi);

DR_API
/**
 * Makes a copy of \p data.  Copy must be freed with dr_free_module_data().
 * Useful for making persistent copies of module_data_t's received as part of
 * image load and unload event callbacks.
 */
module_data_t *
dr_copy_module_data(const module_data_t *data);

DR_API
/**
 * Frees a module_data_t returned by dr_module_iterator_next(), dr_lookup_module(),
 * dr_lookup_module_by_name(), or dr_copy_module_data(). \note Should NOT be used with
 * a module_data_t obtained as part of a module load or unload event.
 */
void
dr_free_module_data(module_data_t *data);

DR_API
/**
 * Returns the preferred name for the module described by \p data from
 * \p data->module_names.
 */
const char *
dr_module_preferred_name(const module_data_t *data);

DR_API
/**
 * Returns whether \p addr is contained inside any segment of the module \p data.
 * We recommend using this routine rather than checking against the \p start
 * and \p end fields of \p data, as modules are not always contiguous.
 */
bool
dr_module_contains_addr(const module_data_t *data, app_pc addr);

/**
 * Iterator over the list of modules that a given module imports from.  Created
 * by calling dr_module_import_iterator_start() and must be freed by calling
 * dr_module_import_iterator_stop().
 *
 * \note On Windows, delay-loaded DLLs are not included yet.
 *
 * \note ELF does not import directly from other modules.
 */
struct _dr_module_import_iterator_t;
typedef struct _dr_module_import_iterator_t dr_module_import_iterator_t;

/**
 * Descriptor used to iterate the symbols imported from a specific module.
 */
struct _dr_module_import_desc_t;
typedef struct _dr_module_import_desc_t dr_module_import_desc_t;

/**
 * Module import data returned from dr_module_import_iterator_next().
 *
 * String fields point into the importing module image.  Robust clients should
 * use DR_TRY_EXCEPT while inspecting the strings in case the module is
 * partially mapped or the app racily unmaps it.  The iterator routines
 * themselves handle faults by stopping the iteration.
 *
 * \note ELF does not import directly from other modules.
 */
typedef struct _dr_module_import_t {
    /**
     * Specified name of the imported module or API set.
     */
    const char *modname;

    /**
     * Opaque handle that can be passed to dr_symbol_import_iterator_start().
     * Valid until the original module is unmapped.
     */
    dr_module_import_desc_t *module_import_desc;
} dr_module_import_t;

DR_API
/**
 * Creates a module import iterator.  Iterates over the list of modules that a
 * given module imports from.
 *
 * \note ELF does not import directly from other modules.
 */
dr_module_import_iterator_t *
dr_module_import_iterator_start(module_handle_t handle);

DR_API
/**
 * Returns true if there is another module import in the iterator.
 *
 * \note ELF does not import directly from other modules.
 */
bool
dr_module_import_iterator_hasnext(dr_module_import_iterator_t *iter);

DR_API
/**
 * Advances the passed-in iterator and returns the current module import in the
 * iterator.  The pointer returned is only valid until the next call to
 * dr_module_import_iterator_next() or dr_module_import_iterator_stop().
 *
 * \note ELF does not import directly from other modules.
 */
dr_module_import_t *
dr_module_import_iterator_next(dr_module_import_iterator_t *iter);

DR_API
/**
 * Stops import iteration and frees a module import iterator.
 *
 * \note ELF does not import directly from other modules.
 */
void
dr_module_import_iterator_stop(dr_module_import_iterator_t *iter);

/**
 * Symbol import iterator data type.  Can be created by calling
 * dr_symbol_import_iterator_start() and must be freed by calling
 * dr_symbol_import_iterator_stop().
 */
struct _dr_symbol_import_iterator_t;
typedef struct _dr_symbol_import_iterator_t dr_symbol_import_iterator_t;

/**
 * Symbol import data returned from dr_symbol_import_iterator_next().
 *
 * String fields point into the importing module image.  Robust clients should
 * use DR_TRY_EXCEPT while inspecting the strings in case the module is
 * partially mapped or the app racily unmaps it.
 */
typedef struct _dr_symbol_import_t {
    const char *name;    /**< Name of imported symbol, if available. */
    const char *modname; /**< Preferred name of module (Windows only). */
    bool delay_load;     /**< This import is delay-loaded (Windows only). */
    bool by_ordinal;     /**< Import is by ordinal, not name (Windows only). */
    ptr_uint_t ordinal;  /**< Ordinal value (Windows only). */
    /* We never ask the client to allocate this struct, so we can go ahead and
     * add fields here without breaking ABI compat.
     */
} dr_symbol_import_t;

DR_API
/**
 * Creates an iterator over symbols imported by a module.  If \p from_module is
 * NULL, all imported symbols are yielded, regardless of which module they were
 * imported from.
 *
 * On Windows, from_module is obtained from a \p dr_module_import_t and used to
 * iterate over all of the imports from a specific module.
 *
 * The iterator returned is invalid until after the first call to
 * dr_symbol_import_iterator_next().
 *
 * \note On Windows, symbols imported from delay-loaded DLLs are not included
 * yet.
 */
dr_symbol_import_iterator_t *
dr_symbol_import_iterator_start(module_handle_t handle,
                                dr_module_import_desc_t *from_module);

DR_API
/**
 * Returns true if there is another imported symbol in the iterator.
 */
bool
dr_symbol_import_iterator_hasnext(dr_symbol_import_iterator_t *iter);

DR_API
/**
 * Returns the next imported symbol.  The returned pointer is valid until the
 * next call to dr_symbol_import_iterator_next() or
 * dr_symbol_import_iterator_stop().
 */
dr_symbol_import_t *
dr_symbol_import_iterator_next(dr_symbol_import_iterator_t *iter);

DR_API
/**
 * Stops symbol import iteration and frees the iterator.
 */
void
dr_symbol_import_iterator_stop(dr_symbol_import_iterator_t *iter);

/* DR_API EXPORT BEGIN */
/**
 * Symbol export iterator data type.  Can be created by calling
 * dr_symbol_export_iterator_start() and must be freed by calling
 * dr_symbol_export_iterator_stop().
 */
struct _dr_symbol_export_iterator_t;
typedef struct _dr_symbol_export_iterator_t dr_symbol_export_iterator_t;

/**
 * Symbol export data returned from dr_symbol_export_iterator_next().
 *
 * String fields point into the exporting module image.  Robust clients should
 * use DR_TRY_EXCEPT while inspecting the strings in case the module is
 * partially mapped or the app racily unmaps it.
 *
 * On Windows, the address in \p addr may not be inside the exporting module if
 * it is a forward and has been patched by the loader.  In that case, \p forward
 * will be NULL.
 */
typedef struct _dr_symbol_export_t {
    const char *name;    /**< Name of exported symbol, if available. */
    app_pc addr;         /**< Address of the exported symbol. */
    const char *forward; /**< Forward name, or NULL if not forwarded (Windows only). */
    ptr_uint_t ordinal;  /**< Ordinal value (Windows only). */
    /**
     * Whether an indirect code object (see dr_export_info_t).  (Linux only).
     */
    bool is_indirect_code;
    bool is_code; /**< Whether code as opposed to exported data (Linux only). */
    /* We never ask the client to allocate this struct, so we can go ahead and
     * add fields here without breaking ABI compat.
     */
} dr_symbol_export_t;

DR_API
/**
 * Creates an iterator over symbols exported by a module.
 * The iterator returned is invalid until after the first call to
 * dr_symbol_export_iterator_next().
 *
 * \note To iterate over all symbols in a module and not just those exported,
 * use the \ref page_drsyms.
 */
dr_symbol_export_iterator_t *
dr_symbol_export_iterator_start(module_handle_t handle);

DR_API
/**
 * Returns true if there is another exported symbol in the iterator.
 */
bool
dr_symbol_export_iterator_hasnext(dr_symbol_export_iterator_t *iter);

DR_API
/**
 * Returns the next exported symbol.  The returned pointer is valid until the
 * next call to dr_symbol_export_iterator_next() or
 * dr_symbol_export_iterator_stop().
 */
dr_symbol_export_t *
dr_symbol_export_iterator_next(dr_symbol_export_iterator_t *iter);

DR_API
/**
 * Stops symbol export iteration and frees the iterator.
 */
void
dr_symbol_export_iterator_stop(dr_symbol_export_iterator_t *iter);

#ifdef WINDOWS
DR_API
/**
 * Returns whether \p pc is within a section within the module in \p section_found and
 * information about that section in \p section_out. \note Not yet available on Linux.
 */
bool
dr_lookup_module_section(module_handle_t lib, byte *pc,
                         IMAGE_SECTION_HEADER *section_out);

#endif /* WINDOWS */

DR_API
/**
 * Set whether or not the module referred to by \p handle should be
 * instrumented.  If \p should_instrument is false, code from the module will
 * not be passed to the basic block event.  If traces are enabled, code from the
 * module will still reach the trace event.  Must be called from the module load
 * event for the module referred to by \p handle.
 * \return whether successful.
 *
 * \warning Turning off instrumentation for modules breaks clients and
 * extensions, such as drwrap, that expect to see every instruction.
 */
bool
dr_module_set_should_instrument(module_handle_t handle, bool should_instrument);

DR_API
/**
 * Return whether code from the module should be instrumented, meaning passed
 * to the basic block event.
 */
bool
dr_module_should_instrument(module_handle_t handle);

DR_API
/**
 * Returns the entry point of the exported function with the given
 * name in the module with the given base.  Returns NULL on failure.
 *
 * On Linux, when we say "exported" we mean present in the dynamic
 * symbol table (.dynsym).  Global functions and variables in an
 * executable (as opposed to a library) are not exported by default.
 * If an executable is built with the \p -rdynamic flag to \p gcc, its
 * global symbols will be present in .dynsym and dr_get_proc_address()
 * will locate them.  Otherwise, the drsyms Extension (see \ref
 * page_drsyms) must be used to locate the symbols.  drsyms searches
 * the debug symbol table (.symtab) in addition to .dynsym.
 *
 * \note On Linux this ignores symbol preemption by other modules and only
 * examines the specified module.
 * \note On Linux, in order to handle indirect code objects, use
 * dr_get_proc_address_ex().
 */
generic_func_t
dr_get_proc_address(module_handle_t lib, const char *name);

/**
 * Data structure used by dr_get_proc_address_ex() to retrieve information
 * about an exported symbol.
 */
typedef struct _dr_export_info_t {
    /**
     * The entry point of the export as an absolute address located
     * within the queried module.  This address is identical to what
     * dr_get_proc_address_ex() returns.
     */
    generic_func_t address;
    /**
     * Relevant for Linux only.  Set to true iff this export is an
     * indirect code object, which is a new ELF extension allowing
     * runtime selection of which implementation to use for an
     * exported symbol.  The address of such an export is a function
     * that takes no arguments and returns the address of the selected
     * implementation.
     */
    bool is_indirect_code;
} dr_export_info_t;

DR_API
/**
 * Returns information in \p info about the symbol \p name exported
 * by the module \p lib.  Returns false if the symbol is not found.
 * See the information in dr_get_proc_address() about what an
 * "exported" function is on Linux.
 *
 * \note On Linux this ignores symbol preemption by other modules and only
 * examines the specified module.
 */
bool
dr_get_proc_address_ex(module_handle_t lib, const char *name, dr_export_info_t *info OUT,
                       size_t info_len);

#endif /* _DR_MODULES_H_ */
