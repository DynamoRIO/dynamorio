/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

#ifndef _DR_CONFIG_H_
#define _DR_CONFIG_H_ 1

/****************************************************************************
 * Deployment API
 */
/**
 * @file dr_config.h
 * @brief Deployment API for DynamoRIO.  Use these functions to register
 * processes to run under DynamoRIO, unregister processes, obtain existing
 * registration information, and nudge running processes.
 * \note The dr_config library is currently not multi-thread safe. Users of
 * the library should ensure that no more then one thread accesses the library
 * at a time.  This limitation will be addressed in future releases.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of a registered process's options string */
#define DR_MAX_OPTIONS_LENGTH 2048

/** Specifies DynamoRIO's operation mode. */
typedef enum {

    /**
     * No mode.  Clients should not attempt to register a process in
     * this mode.
     */
    DR_MODE_NONE = 0,

    /**
     * Run DynamoRIO in Code Manipulation mode.
     * This does not preclude using the Probe API.
     */
    DR_MODE_CODE_MANIPULATION = 1,

    /**
     * Run DynamoRIO in Probe mode.  This mode has no code cache.
     * \warning This mode is not fully supported at this time.
     */
    DR_MODE_PROBE = 2,

    /**
     * Run DynamoRIO in security Memory Firewall mode.
     * \warning This mode is not fully supported at this time.
     */
    DR_MODE_MEMORY_FIREWALL = 3,

    /**
     * Do not run this application under DynamoRIO control.
     * Useful for following all child processes except a handful
     * (blocklist).
     */
    DR_MODE_DO_NOT_RUN = 4,

} dr_operation_mode_t;

/** Return status codes for process registration, unregistration and nudging. */
typedef enum {
    /** Operation succeeded. */
    DR_SUCCESS,

    /** Process registration failed due to an existing registration. */
    DR_PROC_REG_EXISTS,

    /** Operation failed because the supplied process is not registered. */
    DR_PROC_REG_INVALID,

    /** Client registration failed due to an invalid priority value. */
    DR_PRIORITY_INVALID,

    /** Client registration failed due to a conflicting ID. */
    DR_ID_CONFLICTING,

    /** Client operation failed due to an invalid client ID. */
    DR_ID_INVALID,

    /** Unknown failure. Check that caller has adequate permissions for this operation. */
    DR_FAILURE,

    /** Nudge operation failed because the specified process id is not under DR. */
    DR_NUDGE_PID_NOT_INJECTED,

    /** Nudge operation timed out waiting for target process to finish handling a nudge.*/
    DR_NUDGE_TIMEOUT,

    /** Field length exceeded, probably due to a too-long option string */
    DR_CONFIG_STRING_TOO_LONG,

    /** Failed to write to the config file. */
    DR_CONFIG_FILE_WRITE_FAILED,

    /** Nudge operation failed because the specified process id does not exist. */
    DR_NUDGE_PID_NOT_FOUND,

    /** Client options contain invalid characters (';' or all 3 quotes). */
    DR_CONFIG_OPTIONS_INVALID,

    /**
     * Failed to locate a valid config dir.
     * Consider calling dr_get_config_dir(, true, ,).
     */
    DR_CONFIG_DIR_NOT_FOUND,

    /**
     * A parameter was not set.
     */
    DR_CONFIG_INVALID_PARAMETER,

    /**
     * A primary client configuration was not found at the time an alternate bitwidth
     * client was attempted to be registered.
     */
    DR_CONFIG_CLIENT_NOT_FOUND,

} dr_config_status_t;

/** Allow targeting both 32-bit and native 64-bit processes separately. */
typedef enum {
    DR_PLATFORM_DEFAULT, /**< The platform this tool is compiled for. */
    DR_PLATFORM_32BIT,   /**< 32-bit settings (for 32-bit processes). */
    DR_PLATFORM_64BIT,   /**< 64-bit settings (for native 64-bit processes). */
    DR_PLATFORM_NONE,    /**< Invalid platform. */
} dr_platform_t;

/**
 * Information about a client library setup.
 */
typedef struct _dr_config_client_t {
    /**
     * The size of this structure.  This field must be set when passing as an input
     * to functions like dr_get_client_info_ex().  Used for backward compatibility.
     */
    size_t struct_size;
    /**
     * This is a client_id_t uniquely identifying the client.  DynamoRIO provides the
     * client ID as a parameter to dr_client_main().  Clients use this ID to retrieve
     * client-specific path and option information. Outside entities also identify
     * the target of a nudge via this ID.
     */
    client_id_t id;
    /**
     * The client number, or priority.  Client registration includes a value
     * indicating the priority of a client relative to other clients.  In
     * multi-client settings, a client's priority influences event callback
     * ordering.  That is, higher priority clients can register their callbacks first;
     * DynamoRIO then calls these routines last.  Client priorities range
     * consecutively from 0 to N-1, where N is the number of registered clients.
     * Specify priority 0 to register a client with highest priority.
     */
    size_t priority;
    /**
     * A NULL-terminated string specifying the full path to a valid client library.
     * The string length cannot exceed #MAXIMUM_PATH.  The client path may not
     * include any semicolons and when combined with \p client_options may not
     * include all three quote characters (', ", `) simultaneously.
     *
     * When querying via dr_get_client_info_ex() or dr_client_iterator_next_ex(), the
     * caller must either set this to NULL if that data is not needed or point at a
     * caller-allocated array of length #MAXIMUM_PATH.
     */
    char *path;
    /**
     * A NULL-terminated string specifying options that are available to the client
     * as arguments of dr_client_main() or via dr_get_option_array().  The string
     * length cannot exceed #DR_MAX_OPTIONS_LENGTH.  The client options may not
     * include any semicolons and when combined with \p client_path may not include
     * all three quote characters (', ", `) simultaneously.
     *
     * When querying via dr_get_client_info_ex() or dr_client_iterator_next_ex(), the
     * caller must either set this to NULL if that data is not needed or point at a
     * caller-allocated array of length #DR_MAX_OPTIONS_LENGTH.
     */
    char *options;
    /**
     * Specifies whether this is a regular client registration for the target process
     * (this field is false) or whether this is an "alternate bitwidth" registration
     * which specifies the configuration to use if the target process creates a child
     * process that is a different bitwidth from itself.  The regular client must
     * first be registered on its own before the alternate client is registered, and
     * the alternate must have the same client ID.  If the target process creates a
     * child application of a different bitwdith (e.g., the target process is 64-bit
     * and it creates a 32-bit child), this registration allows a client of the
     * appropriate bitwidth to be loaded into the child.  Unregistering a client with
     * dr_unregister_client() will also unregister the alternate bitwidth client with
     * the same client ID.
     */
    bool is_alt_bitwidth;
} dr_config_client_t;

/* Note that we provide this as an inlined function for use by
 * dr_nudge_client and dr_nudge_client_ex without requiring drconfiglib
 * to be linked into a client.
 */
/**
 * Translates dr_config_status_t code to its corresponding message string.
 *
 * \param[in]  code        dr_config_status_t code to be translated.
 *
 * \return     The translated message string.
 */
static inline const char *
dr_config_status_code_to_string(dr_config_status_t code)
{
    const char *msg;
    /* XXX: should we have a string array for lookup instead?
     * If so, we must make sure the consistency between the array and
     * the status enum.
     */
    switch (code) {
    case DR_SUCCESS: msg = "success"; break;
    case DR_PROC_REG_EXISTS: msg = "registration already exists"; break;
    case DR_PROC_REG_INVALID: msg = "target process is not registered"; break;
    case DR_PRIORITY_INVALID: msg = "invalid priority value"; break;
    case DR_ID_CONFLICTING: msg = "conflicting ID"; break;
    case DR_ID_INVALID: msg = "invalid client ID"; break;
    case DR_NUDGE_PID_NOT_INJECTED: msg = "target process is not under DynamoRIO"; break;
    case DR_NUDGE_TIMEOUT: msg = "timed out"; break;
    case DR_CONFIG_STRING_TOO_LONG: msg = "config option string too long"; break;
    case DR_CONFIG_FILE_WRITE_FAILED: msg = "failed to write to the config file"; break;
    case DR_NUDGE_PID_NOT_FOUND: msg = "target process id does not exist"; break;
    case DR_CONFIG_DIR_NOT_FOUND:
        msg = "failed to locate a valid config directory";
        break;
    case DR_FAILURE: msg = "unknown failure"; break;
    default: msg = "invalid dr_config_status_t code"; break;
    }
    return msg;
}

DR_EXPORT
/**
 * Register a process to run under DynamoRIO.
 * Note that this routine only sets the base options to run a process
 * under DynamoRIO.  To register one or more clients, call
 * dr_register_client() subsequently.
 *
 * In order to use local config files when the normal interactive user
 * home directory environment variable (HOME on Linux or MacOS;
 * USERPROFILE on Windows) is not set and when using one-step
 * configure-and-run (i.e., when \p pid != 0), call
 * dr_get_config_dir() prior to creating the child process (and prior
 * to calling this routine) and pass true for \p alternative_local.
 * For multi-step, the caller must set the DYNAMORIO_CONFIGDIR
 * environment variable before calling this routine.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If \p pid != 0, a one-time configuration is created
 *                              just for it.  If \p pid == 0, a general configuration
 *                              is created for all future instances of process_name.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_root_dir     A NULL-terminated string specifying the full
 *                              path to a valid DynamoRIO root directory.
 *                              The string length cannot exceed #MAXIMUM_PATH.
 *                              The path to the DynamoRIO library to be used will
 *                              be constructed from this path assuming a default
 *                              layout.  To precisely specify the path, call
 *                               dr_register_inject_paths() afterward.
 *
 * \param[in]   dr_mode         Specifies the mode under which DynamoRIO should
 *                              operate.  See dr_operation_mode_t.
 *
 * \param[in]   debug           If true, a DynamoRIO debug build will be used;
 *                              otherwise, a release build will be used.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to set.
 *
 * \param[in]   dr_options      A NULL-terminated string controlling
 *                              DynamoRIO's behavior.  Most users should
 *                              not need to specify options.  The total
 *                              string length cannot exceed #DR_MAX_OPTIONS_LENGTH.
 *
 * \return      A dr_config_status_t code indicating the result of
 *              registration.  Note that registration fails if the requested
 *              process is already registered.  To modify a process's
 *              registration, first call dr_unregister_process() to remove an
 *              existing registration.
 *
 * \remarks
 * After registration, a process will run under DynamoRIO when launched by the
 * drinject tool or using drinjectlib.  Note that some processes may require a
 * system reboot to restart.  Process registration that is not specific to one
 * pid (i.e., if \p pid == 0) persists across reboots until explicitly
 * unregistered.
 */
dr_config_status_t
dr_register_process(const char *process_name, process_id_t pid, bool global,
                    const char *dr_root_dir, dr_operation_mode_t dr_mode, bool debug,
                    dr_platform_t dr_platform, const char *dr_options);

DR_EXPORT
/**
 * Unregister a process from running under DynamoRIO.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the existing one-time configuration is
 *                              removed. If pid == 0, the general configuration
 *                              for process_name is removed.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to set.
 *
 * \return      A dr_config_status_t code indicating the result of
 *              unregistration.  Note that unregistration fails if the process
 *              is not currently registered to run under DynamoRIO.
 */
dr_config_status_t
dr_unregister_process(const char *process_name, process_id_t pid, bool global,
                      dr_platform_t dr_platform);

DR_EXPORT
/**
 * Sets the paths to the DynamoRIO library and the alternate-bitwidth DynamoRIO
 * library for a process that has already been registered via dr_register_process().
 * If this routine is not called, default library paths will be used which assume
 * a default path layout within the \p dr_root_dir passed to dr_register_process().
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be modified.  If pid == 0, the general
 *                              configuration for process_name will be modified.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to check.
 *
 * \param[in]   dr_lib_path     A NULL-terminated string specifying the full
 *                              path to a valid DynamoRIO library of the same
 *                              bitwidth as \p dr_platform.
 *                              The string length cannot exceed #MAXIMUM_PATH.
 *
 * \param[in]   dr_alt_lib_path A NULL-terminated string specifying the full
 *                              path to a valid DynamoRIO library of the alternative
 *                              bitwidth to \p dr_platform.
 *                              The string length cannot exceed #MAXIMUM_PATH.
 *
 * \return      A dr_config_status_t code indicating the result of
 *              registration.  Note that registration fails if the requested
 *              process is already registered.  To modify a process's
 *              registration, first call dr_unregister_process() to remove an
 *              existing registration.
 *
 * \remarks
 * After registration, a process will run under DynamoRIO when launched by the
 * drinject tool or using drinjectlib.  Note that some processes may require a
 * system reboot to restart.  Process registration that is not specific to one
 * pid (i.e., if \p pid == 0) persists across reboots until explicitly
 * unregistered.
 */
dr_config_status_t
dr_register_inject_paths(const char *process_name, process_id_t pid, bool global,
                         dr_platform_t dr_platform, const char *dr_lib_path,
                         const char *dr_alt_lib_path);

#ifdef WINDOWS

DR_EXPORT
/**
 * Sets up systemwide injection so that registered applications will run under
 * DynamoRIO however they are launched (i.e., they do not need to be explicitly
 * invoked with the drrun or drinject tools).  This requires administrative
 * privileges and affects all users (though configurations remain private to
 * each user).  On Windows NT, a reboot is required for this to take effect.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              to use.
 *
 * \param[in]   dr_root_dir     The root DynamoRIO directory.
 *
 * \return      A dr_config_status_t code indicating the result of
 *              the operation.  The operation will fail if the caller does
 *              not have sufficient privileges.
 *
 * \note On Windows, an application that does not link with user32.dll will not be
 * run under control of DynamoRIO via systemwide injection.  Such applications
 * will only be under DynamoRIO control if launched by the drrun or drinject
 * tools or if the parent process (typically explorer.exe, for manually launched
 * applications) is already under DynamoRIO control (the parent can be in any
 * mode, but a 32-bit parent cannot inject into a 64-bit child). Only some small
 * non-graphical applications do not link with user32.dll.
 *
 * \note Not yet available on Linux or MacOS.
 */
dr_config_status_t
dr_register_syswide(dr_platform_t dr_platform, const char *dr_root_dir);

DR_EXPORT
/**
 * Disables systemwide injection.  Registered applications will not run
 * under DynamoRIO unless explicitly launched with the drrun or drinject
 * tools (or a custom tool that uses the drinjectlib library).
 * On Windows NT, a reboot is required for this to take effect.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              to use.
 *
 * \param[in]   dr_root_dir     The root DynamoRIO directory.
 *
 * \return      A dr_config_status_t code indicating the result of
 *              the operation.  The operation will fail if the caller does
 *              not have sufficient privileges.
 *
 * \note Not yet available on Linux or MacOS.
 */
dr_config_status_t
dr_unregister_syswide(dr_platform_t dr_platform, const char *dr_root_dir);

DR_EXPORT
/**
 * Returns whether systemwide injection is enabled.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              to use.
 *
 * \param[in]   dr_root_dir     The root DynamoRIO directory.
 *
 * \return      Whether systemwide injection is enabled.
 *
 * \note Not yet available on Linux or MacOS.
 */
bool
dr_syswide_is_on(dr_platform_t dr_platform, const char *dr_root_dir);

#endif /* WINDOWS */

DR_EXPORT
/**
 * Check if a process is registered to run under DynamoRIO.  To obtain client
 * information, use dr_get_client_info().
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be queried.  If pid == 0, the general
 *                              configuration for process_name will be queried.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to check.
 *
 * \param[out]  dr_root_dir     If the process is registered, the root DynamoRIO
 *                              directory provided at registration.  Callers can
 *                              pass NULL if this value is not needed.  Otherwise,
 *                              the parameter must be a caller-allocated array of
 *                              length #MAXIMUM_PATH.
 *
 * \param[out]  dr_mode         If the process is registered, the mode provided
 *                              at registration.  Callers can pass NULL if this
 *                              value is not needed.
 *
 * \param[out]  debug           If the process is registered, the debug flag
 *                              provided at registration.  Callers can pass NULL
 *                              if this value is not needed.
 *
 * \param[out]  dr_options      If the process is registered, the extra DynamoRIO
 *                              parameters provided at registration.  Callers can
 *                              pass NULL if this value is not needed.  Otherwise,
 *                              the parameter must be a caller-allocated array of
 *                              length #DR_MAX_OPTIONS_LENGTH.
 *
 * \return      true if the process is registered for the given platform.
 */
bool
dr_process_is_registered(const char *process_name, process_id_t pid, bool global,
                         dr_platform_t dr_platform, char *dr_root_dir /* OUT */,
                         dr_operation_mode_t *dr_mode /* OUT */, bool *debug /* OUT */,
                         char *dr_options /* OUT */);

#ifdef WINDOWS

typedef struct _dr_registered_process_iterator_t dr_registered_process_iterator_t;

DR_EXPORT
/**
 * Creates and starts an iterator for iterating over all processes registered for
 * the given platform and given global or local parameter.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to check.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \return      iterator for use with dr_registered_process_iterator_hasnext()and
 *              dr_registered_process_iterator_next().  Must be freed
 *              with dr_registered_process_iterator_stop()
 *
 * \note Not yet available on Linux or MacOS.
 */
dr_registered_process_iterator_t *
dr_registered_process_iterator_start(dr_platform_t dr_platform, bool global);

DR_EXPORT
/**
 * \param[in]    iter           A registered process iterator created with
 *                              dr_registered_process_iterator_start()
 *
 * \return      true if there are more registered processes to iterate over
 *
 * \note Not yet available on Linux or MacOS.
 */
bool
dr_registered_process_iterator_hasnext(dr_registered_process_iterator_t *iter);

DR_EXPORT
/**
 * Return information about a registered process
 *
 * \param[in]    iter           A registered process iterator created with
 *                              dr_registered_process_iterator_start().
 *
 * \param[out]   process_name   The name of the registered process. Callers can
 *                              pass NULL if this value is not needed. Otherwise
 *                              the parameter must be a caller-allocated array
 *                              of length #MAXIMUM_PATH.
 *
 * \param[out]  dr_root_dir     The root DynamoRIO directory provided at registration.
 *                              Callers can pass NULL if this value is not needed.
 *                              Otherwise, the parameter must be a caller-allocated
 *                              array of length #MAXIMUM_PATH.
 *
 * \param[out]  dr_mode         If the process is registered, the mode provided
 *                              at registration.  Callers can pass NULL if this
 *                              value is not needed.
 *
 * \param[out]  debug           If the process is registered, the debug flag
 *                              provided at registration.  Callers can pass NULL
 *                              if this value is not needed.
 *
 * \param[out]  dr_options      If the process is registered, the extra DynamoRIO
 *                              parameters provided at registration.  Callers can
 *                              pass NULL if this value is not needed.  Otherwise,
 *                              the parameter must be a caller-allocated array of
 *                              length #DR_MAX_OPTIONS_LENGTH.
 *
 * \return      true if the information was successfully retrieved.
 *
 * \note Not yet available on Linux or MacOS.
 */
bool
dr_registered_process_iterator_next(dr_registered_process_iterator_t *iter,
                                    char *process_name /* OUT */,
                                    char *dr_root_dir /* OUT */,
                                    dr_operation_mode_t *dr_mode /* OUT */,
                                    bool *debug /* OUT */, char *dr_options /* OUT */);

DR_EXPORT
/**
 * Stops and frees a registered process iterator.
 *
 * \param[in]    iter           A registered process iterator created with
 *                              dr_registered_process_iterator_start()
 *
 * \note Not yet available on Linux or MacOS.
 */
void
dr_registered_process_iterator_stop(dr_registered_process_iterator_t *iter);

#endif /* WINDOWS */

DR_EXPORT
/**
 * Register a client for a particular process.  Note that the process must first
 * be registered via dr_register_process() before calling this routine.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be modified.  If pid == 0, the general
 *                              configuration for process_name will be modified.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to set.
 *
 * \param[in]   client_id       A client_id_t uniquely identifying the client.
 *                              DynamoRIO provides the client ID as a parameter
 *                              to dr_client_main().  Clients use this ID to retrieve
 *                              client-specific path and option information.
 *                              Outside entities also identify the target of
 *                              a nudge via this ID.
 *
 * \param[in]   client_pri      The client number, or priority.  Client registration
 *                              includes a value indicating the priority of a client
 *                              relative to other clients.  In multi-client settings,
 *                              a client's priority influences event callback
 *                              ordering.  That is, higher priority clients can
 *                              register their callbacks first; DynamoRIO then calls
 *                              these routines last.  Client priorities range
 *                              consecutively from 0 to N-1, where N is the number
 *                              of registered clients.  Specify priority
 *                              0 to register a client with highest priority.
 *
 * \param[in]   client_path     A NULL-terminated string specifying the full path
 *                              to a valid client library.  The string length
 *                              cannot exceed #MAXIMUM_PATH.  The client path may not
 *                              include any semicolons and when combined with
 *                              \p client_options may not include all
 *                              three quote characters (\', \", \`) simultaneously.
 *
 * \param[in]   client_options  A NULL-terminated string specifying options that
 *                              are available to the client as arguments of
 *                              dr_client_main() or via dr_get_option_array().
 *                              The string length cannot exceed #DR_MAX_OPTIONS_LENGTH.
 *                              The client options may not include any semicolons
 *                              and when combined with \p client_path may not include
 *                              all three quote characters (\', \", \`) simultaneously.
 *
 * \return      A dr_config_status_t code indicating the result of registration.
 */
dr_config_status_t
dr_register_client(const char *process_name, process_id_t pid, bool global,
                   dr_platform_t dr_platform, client_id_t client_id, size_t client_pri,
                   const char *client_path, const char *client_options);

DR_EXPORT
/**
 * Register a client for a particular process.  Note that the process must first
 * be registered via dr_register_process() before calling this routine.
 * The #dr_config_client_t structure allows specifying additional options beyond
 * what dr_register_client() supports, such as an alternate bitwidth client.
 * For an alternate bitwidth client, the main client must first be
 * registered by an earlier call.  Unregistering a client with
 * dr_unregister_client() will also unregister the alternate bitwidth
 * client.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be modified.  If pid == 0, the general
 *                              configuration for process_name will be modified.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to set.
 *
 * \param[in]   client          Defines the attributes of the client to be registered.
 *
 * \return      A dr_config_status_t code indicating the result of registration.
 */
dr_config_status_t
dr_register_client_ex(const char *process_name, process_id_t pid, bool global,
                      dr_platform_t dr_platform, IN dr_config_client_t *client);

DR_EXPORT
/**
 * Unregister a client for a particular process.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be modified.  If pid == 0, the general
 *                              configuration for process_name will be modified.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to unset.
 *
 * \param[in]   client_id       The unique client ID provided at client
 *                              registration.
 *
 * \return      A dr_config_status_t code indicating the result of unregistration.
 */
dr_config_status_t
dr_unregister_client(const char *process_name, process_id_t pid, bool global,
                     dr_platform_t dr_platform, client_id_t client_id);

DR_EXPORT
/**
 * Retrieve the number of clients registered for a particular process for
 * the current user.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be queried.  If pid == 0, the general
 *                              configuration for process_name will be queried.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to unset.
 *
 * \return      The number of clients registered for the given process and platform.
 *              An alternative bitwidth client is counted as a separate client.
 */
size_t
dr_num_registered_clients(const char *process_name, process_id_t pid, bool global,
                          dr_platform_t dr_platform);

DR_EXPORT
/**
 * Retrieve client registration information for a particular process for the current
 * user.  If multiple clients are registered (alternative bitwidth clients are
 * considered separate), information on the highest-priority client is returned.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be queried.  If pid == 0, the general
 *                              configuration for process_name will be queried.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to unset.
 *
 * \param[in]   client_id       The unique client ID provided at client
 *                              registration.
 *
 * \param[out]  client_pri      The client's priority.
 *
 * \param[out]  client_path     The client's path provided at registration.
 *                              Callers can pass NULL if this value is not needed.
 *                              Otherwise, the parameter must be a caller-allocated
 *                              array of length #MAXIMUM_PATH.
 *
 * \param[out]  client_options  The client options provided at registration.
 *                              Callers can pass NULL if this value is not needed.
 *                              Otherwise, the parameter must be a caller-allocated
 *                              array of length #DR_MAX_OPTIONS_LENGTH.
 *
 * \return      A dr_config_status_t code indicating the result of the call.
 */
dr_config_status_t
dr_get_client_info(const char *process_name, process_id_t pid, bool global,
                   dr_platform_t dr_platform, client_id_t client_id,
                   size_t *client_pri, /* OUT */
                   char *client_path,  /* OUT */
                   char *client_options /* OUT */);

DR_EXPORT
/**
 * Retrieve client registration information for a particular process for the current
 * user.  If multiple clients are registered (alternative bitwidth clients are
 * considered separate), information on the highest-priority client is returned.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be queried.  If pid == 0, the general
 *                              configuration for process_name will be queried.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to unset.
 *
 * \param[out]  client          The returned information about the client.  On input,
 *                              the struct_size field must be set and the client_id
 *                              field set to the unique client ID provided at client
 *                              registration to be queried.  Furthermore, the client_path
 *                              and client_options fields must each either be NULL
 *                              if that data is not needed or point at a caller-allocated
 *                              array of length #MAXIMUM_PATH for client_path or
 *                              #DR_MAX_OPTIONS_LENGTH for client_options.
 *
 * \return      A dr_config_status_t code indicating the result of the call.
 */
dr_config_status_t
dr_get_client_info_ex(const char *process_name, process_id_t pid, bool global,
                      dr_platform_t dr_platform, dr_config_client_t *client /* IN/OUT */);

typedef struct _dr_client_iterator_t dr_client_iterator_t;

DR_EXPORT
/**
 * Creates and starts an iterator for iterating over all clients registered for
 * the given process.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable (e.g., calc.exe).
 *
 * \param[in]   pid             A process id of a target process, typically just
 *                              created and suspended via dr_inject_process_exit().
 *                              If pid != 0, the one-time configuration for that pid
 *                              will be queried.  If pid == 0, the general
 *                              configuration for process_name will be queried.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   dr_platform     Configurations are kept separate
 *                              for 32-bit processes and 64-bit processes.
 *                              This parameter allows selecting which of those
 *                              configurations to check.
 *
 * \return      iterator for use with dr_client_iterator_hasnext() and
 *              dr_client_iterator_next().  Must be freed with
 *              dr_client_iterator_stop()
 */
dr_client_iterator_t *
dr_client_iterator_start(const char *process_name, process_id_t pid, bool global,
                         dr_platform_t dr_platform);

DR_EXPORT
/**
 * \param[in]   iter            A client iterator created with dr_client_iterator_start()
 *
 * \return      true if there are more clients to iterate over
 */
bool
dr_client_iterator_hasnext(dr_client_iterator_t *iter);

DR_EXPORT
/**
 * Return information about a client.
 *
 * \param[in]   iter            A client iterator created with dr_client_iterator_start()
 *
 * \param[out]  client_id       The unique client ID provided at client registration.
 *
 * \param[out]  client_pri      The client's priority.
 *
 * \param[out]  client_path     The client's path provided at registration.
 *                              Callers can pass NULL if this value is not needed.
 *                              Otherwise, the parameter must be a caller-allocated
 *                              array of length #MAXIMUM_PATH.
 *
 * \param[out]  client_options  The client options provided at registration.
 *                              Callers can pass NULL if this value is not needed.
 *                              Otherwise, the parameter must be a caller-allocated
 *                              array of length #DR_MAX_OPTIONS_LENGTH.
 */
void
dr_client_iterator_next(dr_client_iterator_t *iter, client_id_t *client_id, /* OUT */
                        size_t *client_pri,                                 /* OUT */
                        char *client_path,                                  /* OUT */
                        char *client_options /* OUT */);

DR_EXPORT
/**
 * Return information about a client.
 *
 * \param[in]   iter            A client iterator created with dr_client_iterator_start()
 *
 * \param[out]  client          The returned information about the client.  On input,
 *                              the struct_size field must be set and the client_path
 *                              and client_options fields must each either be NULL
 *                              if that data is not needed or point at a caller-allocated
 *                              array of length #MAXIMUM_PATH for client_path or
 *                              #DR_MAX_OPTIONS_LENGTH for client_options.
 */
dr_config_status_t
dr_client_iterator_next_ex(dr_client_iterator_t *iter, dr_config_client_t *client
                           /* IN/OUT */);

DR_EXPORT
/**
 * Stops and frees a client iterator.
 *
 * \param[in]   iter            A client iterator created with dr_client_iterator_start()
 */
void
dr_client_iterator_stop(dr_client_iterator_t *iter);

#ifdef WINDOWS
DR_EXPORT
/**
 * Provides a mechanism for an external entity on the guest OS to
 * communicate with a client.  Requires administrative privileges.  A
 * process 'nudge' causes a client event handler to be invoked (use
 * dr_register_nudge_event() to register the handler function).  A
 * nudge is ignored if the process is not running under DynamoRIO,
 * the specified client is not loaded, or if the client does not
 * provide a handler.
 *
 * \param[in]   process_name    A NULL-terminated string specifying the name
 *                              of the target process.  The string should
 *                              identify the base name of the process, not the
 *                              full path of the executable.
 *
 * \param[in]   client_id       The unique client ID provided at client
 *                              registration.
 *
 * \param[in]   arg             An argument passed to the client's nudge
 *                              handler.
 *
 * \param[in]   timeout_ms      The number of milliseconds to wait for each
 *                              nudge to complete before continuing. If INFINITE
 *                              is supplied then the wait is unbounded. If 0
 *                              is supplied the no wait is performed.  If a
 *                              wait times out DR_NUDGE_TIMEOUT will be returned.
 *
 * \param[out]  nudge_count     Returns the number of processes nudged.
 *                              Client can supply NULL if this value is
 *                              not needed.
 *
 * \return      A dr_config_status_t code indicating the result of the nudge.
 *
 * \remarks
 * If there are multiple processes executing with the same name, all
 * of them are nudged.
 *
 * \remarks
 * A process nudge is a one way asynchronous communication from an
 * external entity to a client.  It does not allow a client to
 * return information back to the nudge originator.  To communicate
 * from a client to another process, use the channels specified
 * in \ref sec_comm.
 *
 * \note Not yet available on Linux or MacOS.
 */
dr_config_status_t
dr_nudge_process(const char *process_name, client_id_t client_id, uint64 arg,
                 uint timeout_ms, int *nudge_count /*OUT */);
#endif

DR_EXPORT
/**
 * Provides a mechanism for an external entity on the guest OS to
 * communicate with a client.  Requires administrative privileges.  A
 * process 'nudge' causes a client event handler to be invoked (use
 * dr_register_nudge_event() to register the handler function).  A
 * nudge is ignored if the process is not running under DynamoRIO,
 * the specified client is not loaded, or if the client does not
 * provide a handler.
 *
 * \param[in]   process_id      The system id of the process to nudge
 *                              (see dr_get_process_id())
 *
 * \param[in]   client_id       The unique client ID provided at client
 *                              registration.
 *
 * \param[in]   arg             An argument passed to the client's nudge
 *                              handler.
 *
 * \param[in]   timeout_ms      The number of milliseconds to wait for the
 *                              nudge to complete before returning. If INFINITE
 *                              is supplied then the wait is unbounded. If 0
 *                              is supplied the no wait is performed.  If the
 *                              wait times out DR_NUDGE_TIMEOUT will be returned.
 *
 * \return      A dr_config_status_t code indicating the result of the nudge.
 *
 * \remarks
 * A process nudge is a one way asynchronous communication from an
 * external entity to a client.  It does not allow a client to
 * return information back to the nudge originator.  To communicate
 * from a client to another process, use the channels specified
 * in \ref sec_comm.
 *
 * \note Not yet available on MacOS.
 */
dr_config_status_t
dr_nudge_pid(process_id_t process_id, client_id_t client_id, uint64 arg, uint timeout_ms);

#ifdef WINDOWS
DR_EXPORT
/**
 * Provides a mechanism for an external entity on the guest OS to
 * communicate with a client.  Requires administrative privileges.  A
 * process 'nudge' causes a client event handler to be invoked (use
 * dr_register_nudge_event() to register the handler function).  A
 * nudge is ignored if the process is not running under DynamoRIO,
 * the specified client is not loaded, or if the client does not
 * provide a handler.  Nudges are attempted to all processes running
 * on the system.
 *
 * \param[in]   client_id       The unique client ID provided at client
 *                              registration.
 *
 * \param[in]   arg             An argument passed to the client's nudge
 *                              handler.
 *
 * \param[in]   timeout_ms      The number of milliseconds to wait for each
 *                              nudge to complete before continuing. If INFINITE
 *                              is supplied then the wait is unbounded. If 0
 *                              is supplied the no wait is performed.  If a
 *                              wait times out DR_NUDGE_TIMEOUT will be returned.
 *
 * \param[out]  nudge_count     Returns the number of processes nudged.
 *                              Client can supply NULL if this value is
 *                              not needed.
 *
 * \return      A dr_config_status_t code indicating the result of the nudge.
 *
 * \remarks
 * A process nudge is a one way asynchronous communication from an
 * external entity to a client.  It does not allow a client to
 * return information back to the nudge originator.  To communicate
 * from a client to another process, use the channels specified
 * in \ref sec_comm.
 *
 * \note Not yet available on Linux or MacOS.
 */
dr_config_status_t
dr_nudge_all(client_id_t client_id, uint64 arg, uint timeout_ms,
             int *nudge_count /*OUT */);
#endif /* WINDOWS */

DR_EXPORT
/**
 * Returns in \p config_dir the configuration directory used to store config
 * files.  In order to use local config files when the normal interactive user
 * home directory environment variable (HOME on Linux or MacOS; USERPROFILE on Windows)
 * is not set and when using one-step configure-and-run, call this routine prior
 * to creating the child process and pass true for \p alternative_local.  For
 * multi-step, the caller must set the DYNAMORIO_CONFIGDIR environment variable
 * (in the child process) to point at the resulting \p config_dir.
 *
 * \param[in]   global          Whether to use global or user-local config
 *                              files.  On Windows, global config files are
 *                              stored in a dir pointed at by the DYNAMORIO_HOME
 *                              registry key.  On Linux, they are in
 *                              /etc/dynamorio.  Administrative privileges may
 *                              be needed if global is true.  Note that
 *                              DynamoRIO gives local config files precedence
 *                              when both exist.  The caller must separately
 *                              create the global directory.
 *
 * \param[in]   alternative_local Whether to locate a temporary directory to
 *                                use for user-local config files and to set
 *                                the DYNAMORIO_CONFIGDIR environment variable
 *                                to point at it, if the regular local
 *                                config dir is not found.
 *
 * \param[out]  config_dir      Receives the full path to the config dir.
 *
 * \param[in]   config_dir_sz   The capacity, in characters, of \p config_dir.
 *
 * \return      A dr_config_status_t code indicating the result of the request.
 */
dr_config_status_t
dr_get_config_dir(bool global, bool alternative_local, char *config_dir /* OUT */,
                  size_t config_dir_sz);

#ifdef __cplusplus
}
#endif

#endif /* _DR_CONFIG_H_ */
