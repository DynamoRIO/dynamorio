/* **********************************************************
 * Copyright (c) 2013-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 VMware, Inc.  All rights reserved.
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

#ifndef _DR_PROBE_H_
#define _DR_PROBE_H_ 1

/****************************************************************************
 * Probe API
 */
/**
 * @file dr_probe.h
 * @brief Support for the Probe API.
 *
 *  Describes all the data types and functions associated with the \ref
 * API_probe.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DR_API
#    define DR_API
#endif

/**
 * Describes the status of a probe at any given point.  The status is returned
 * by dr_register_probes() in the dr_probe_desc_t structure for each probe
 * specified.  It can be obtained for individual probes by calling
 * dr_get_probe_status().
 */
typedef enum {
    /* Error codes. */

    /** Exceptions during callback execution and other unknown errors. */
    DR_PROBE_STATUS_ERROR = 1,

    /** An invalid or unknown probe ID was specified with dr_get_probe_status(). */
    DR_PROBE_STATUS_INVALID_ID = 2,

    /* All the invalid states listed below may arise statically (at the
     * time of parsing the probes, i.e., inside dr_register_probes() or
     * dynamically (i.e., when modules are loaded or unloaded)).
     */

    /** The numeric virtual address specified for the probe insertion location
     * or the callback function is invalid.
     */
    DR_PROBE_STATUS_INVALID_VADDR = 3,

    /** The pointer to the name of the library containing the probe insertion
     * location or the callback function is invalid or the library containing
     * the callback function can't be loaded.
     */
    DR_PROBE_STATUS_INVALID_LIB = 4,

    /** The library offset for either the probe insertion location or the
     * callback function is invalid; for ex., if the offset is out of bounds.
     */
    DR_PROBE_STATUS_INVALID_LIB_OFFS = 5,

    /** The pointer to the name of the exported function, where the probe is to
     * be inserted or which is the callback function, is invalid or the exported
     * function doesn't exist.
     */
    DR_PROBE_STATUS_INVALID_FUNC = 6,

    /* Codes for pending cases, i.e., valid probes haven't been inserted
     * because certain events haven't transpired.
     */

    /** The numeric virtual address specified for the probe insertion location
     * or the callback function isn't executable.  This may be so at the time
     * of probe registration or latter if the memory protections are changed.
     * An inserted probe might reach this state if the probe insertion point or
     * the callback function is made non-executable after being executable.
     */
    DR_PROBE_STATUS_VADDR_NOT_EXEC = 7,

    /** The library where the probe is to be inserted isn't in the process. */
    DR_PROBE_STATUS_LIB_NOT_SEEN = 8,

    /** Execution hasn't reached the probe insertion point yet.  This is valid
     * for Code Manipulation mode only because in that mode probes are inserted
     * only in the dynamic instruction stream.
     */
    DR_PROBE_STATUS_WAITING_FOR_EXEC = 9,

    /** Either the library where the probe is to be inserted has been unloaded
     * or the library containing the callback function has been unloaded.
     */
    DR_PROBE_STATUS_LIB_UNLOADED = 10,

    /* Miscellaneous status codes. */

    /** Probe was successfully inserted. */
    DR_PROBE_STATUS_INJECTED = 11,

    /** One or more aspects of the probe aren't supported as of now. */
    DR_PROBE_STATUS_UNSUPPORTED = 12,

#ifdef DYNAMORIO_INTERNAL
    /* DON'T CHANGE THE VALUES OF THE DR_* CONSTANTS DEFINED ABOVE.  They are
     * exported to clients, whereas constants in this ifdef aren't.  Any change
     * to those values will likely break old clients with newer versions of DR
     * (backward compatibility).  New status codes should go after
     * DR_PROBE_STATUS_UNSUPPORTED.
     */
    /* Note: constants are numbered to prevent the compiler from resetting the
     * sequence based on the symbolic assignments below.  HOTP_INJECT_DETECT
     * ended up getting the same number as one the values above!  Ditto with
     * HOTP_INJECT_OFF.  Though these duplications only broke the tools build
     * they can cause subtle runtime errors, so forcing numbers.
     */

    /* The constants below are used only for LiveShields.  */

    /* Defines the current injection status of a policy, i.e., was it injected
     * or not, why and why not?  Today we don't distinguish the reasons for
     * error cases, i.e., all of them are rolled into one.
     *
     * Constants list from most important status to least, from a reporting
     * point of view; don't change this arbitrarily.
     *
     * CAUTION: Any changes to this will break drview, so they must be kept in
     * sync.
     */

    /* Deactivation, exception, error, etc. */
    HOTP_INJECT_ERROR = DR_PROBE_STATUS_ERROR,

    /* Completely injected in protect mode. */
    HOTP_INJECT_PROTECT = DR_PROBE_STATUS_INJECTED,

    /* Completely injected in detect mode.  Not applicable to probes as they
     * don't have detectors.  Restart numbering at 100 to give enough room for
     * future probe status constants.
     */
    HOTP_INJECT_DETECT = 100,

    /* One or more patch points in a vulnerability have been patched, but not
     * all, yet.  N/A to probes as they can't group multiple patch points.
     */
    HOTP_INJECT_IN_PROGRESS = 101,

    /* Vulnerability was matched and is ready for injection, but no patch point
     * has been seen yet.
     */
    HOTP_INJECT_PENDING = DR_PROBE_STATUS_WAITING_FOR_EXEC,

    /* Vulnerability hasn't been matched yet.  May mean that it is not yet
     * vulnerable (because all dlls haven't been loaded) or just not vulnerable
     * at all;  there is no way to distinguish between the two.
     */
    HOTP_INJECT_NO_MATCH = DR_PROBE_STATUS_LIB_NOT_SEEN,

    /*
    TODO: must distinguish between no match & vulnerable vs. no match & not
          vulnerable; future work if needed.
    HOTP_INJECT_NO_MATCH_VULNERABLE,
    HOTP_INJECT_NO_MATCH_NOT_VULNERABLE,
    */

    HOTP_INJECT_OFF = 102 /* Policy has been turned off, so no injection. */
#endif                    /* DYNAMORIO_INTERNAL */
} dr_probe_status_t;

/** Specifies what type of computation to use when computing the address of a
 * probe insertion point or callback function.
 */
typedef enum {
    /** Use the raw virtual address specified. */
    DR_PROBE_ADDR_VIRTUAL,

    /** Compute address by adding the offset specified to the base of the
     * library specified.
     *
     * For probe insertion, if library isn't loaded, the insertion will be
     * aborted.  For computing callback function address if the library isn't
     * loaded, it will be loaded and then the address computation will be done;
     * if it can't be loaded, probe request is aborted.
     */
    DR_PROBE_ADDR_LIB_OFFS,

    /** Compute address by getting the address of the specified exported
     * function from the specified library.
     *
     * If the exported function specified isn't available either for the probe
     * insertion point or for the callback function, the probe insertion is
     * aborted.  For computing callback function address if the library isn't
     * loaded, it will be loaded and then the address computation will be done;
     * if it can't be loaded, probe request is aborted. */
    DR_PROBE_ADDR_EXP_FUNC,
} dr_probe_addr_t;

/** Defines the location where a probe is to be inserted or callback function
 * defined as an offset within a library. */
typedef struct {
    /** IN - Full name of the library. */
    /* FIXME PR 533522: explicitly specify what type of name should be used
     * here: full path, dr_module_preferred_name(), pe (exports) name, what?
     * seems broken since need full path to load a lib but that won't match?
     */
    char *library;

    /** IN - Offset to use inside library.  If out of bounds of library, probe
     * request is aborted.  Offset can point to non text location as long as it
     * is marked executable (i.e., ..x). */
    app_rva_t offset;
} dr_probe_lib_offs_t;

/** Defines the location where a probe is to be inserted or callback function
 * defined as an exported function within a library. */
typedef struct {
    /** IN - Full name of the library. */
    /* FIXME PR 533522: explicitly specify what type of name should be used
     * here: full path, dr_module_preferred_name(), pe (exports) name, what?
     * seems broken since need full path to load a lib but that won't match?
     */
    char *library;

    /** IN - Name of exported function inside library.  If the function can't
     * be found in the library, then this probe request is aborted. */
    char *func;
} dr_probe_exp_func_t;

/** Defines a memory location where a probe is to be inserted or where a
 * callback function exists.  One of three types of address computation as
 * described by dr_probe_addr_t is used to identify the location.
 */
typedef struct {
    /** IN - Specifies the type of address computation to use. */
    dr_probe_addr_t type;

    union {
        /** IN - Raw virtual address in the process space. */
        app_pc vaddr;

        /** IN - Library offset in the process space. */
        dr_probe_lib_offs_t lib_offs;

        /** IN - Exported function in the process space. */
        dr_probe_exp_func_t exp_func;
    };
} dr_probe_location_t;

/* TODO: hotp_exec_status_t in hotpatch_interface.h is what's really used
 * in the code so once we start adding actual values here we should merge
 * the two.
 */
/**
 * Specifies what action to take upon return of a probe callback function.
 * Additional options will be added in future releases.
 */
typedef enum {
    /** Continue execution normally after the probe. */
    DR_PROBE_RETURN_NORMAL = 0,
} dr_probe_return_t;

/**
 * This structure describes the characteristics of a probe.  It is used to add,
 * update, and remove probes using dr_register_probes().
 */
typedef struct {
    /**
     * IN - Pointer to the name of the probe.  This string does not need
     * to be persistent beyond the call to dr_register_probes(): a copy will
     * be made.
     */
    char *name;

    /**
     * IN - Location where the probe is to be inserted.  If inserting inside a
     * library, the insertion will be done only if the library is loaded, the
     * location falls within its bounds and the location is marked executable.
     * If inserting outside a library the memory location should be allocated
     * and marked executable.  If neither, the probe will be put in a pending
     * state where its id will be NULL and its status reflecting the state.
     */
    dr_probe_location_t insert_loc;

    /**
     * IN - Location of the callback function.  If callback is inside a
     * library, the library location will be used if it is within its bounds
     * and is marked executable; if library isn't loaded, an attempt will be
     * made to load it.  If using a raw virtual address, then that location
     * should be mapped and marked executable.  If neither is true, the probe
     * insertion or update will be aborted and status updated accordingly.
     *
     * The callback function itself should have this signature:
     *
     *   dr_probe_return_t probe_callback(priv_mcontext_t *mc);
     *
     * Note that the \p xip field of the \p priv_mcontext_t passed in will
     * NOT be set.
     */
    dr_probe_location_t callback_func;

    /** OUT - Upon successful probe insertion a unique id will be created. */
    unsigned int id;

    /** OUT - Specifies the current status of the probe. */
    dr_probe_status_t status;
} dr_probe_desc_t;

/******************************************************************************/
DR_API
/**
 * Allows the caller to insert probes into specified executable memory
 * locations in the process address space.  Upon subsequent execution of
 * instructions at these memory locations the appropriate probes will be
 * triggered and the corresponding callback functions will be invoked.
 * Subsequent calls to dr_register_probes() will allow the caller to remove,
 * update and add more probes.
 *
 * @param       probes  Pointer to an array of probe descriptors of type
 *                      dr_probe_desc_t.  Each descriptor describes the
 *                      location of the probe, the callback function and other
 *                      data.  This is an in/out parameter, see dr_probe_desc_t
 *                      for details.  If probes isn't NULL, points to valid
 *                      memory and num_probes is greater than 0, id and status
 *                      for each probe are set in the corresponding
 *                      dr_probe_desc_t.  If probes is NULL or num_probes is 0,
 *                      nothing is set in probes.  Invalid memory may trigger
 *                      an exception.
 *
 * @param [in]  num_probes  Specifies the number of probe descriptors in
 *                          the array pointed to by probes.
 *
 * \remarks
 * If a probe definition is invalid, it will not be registered, i.e. DynamoRIO
 *  will not retain its definition.  The error code will be returned in the
 *  status field in of that probe's dr_probe_desc_t element and the
 *  corresponding id field in dr_probe_desc_t is set to NULL.
 *
 * \remarks
 * When a client calls dr_register_probes() from dr_client_main() (which is the
 *  earliest dr_register_probes() can be called), not all valid probes are
 *  guaranteed to be inserted upon retrun from dr_register_probes().  Some
 *  valid probes may not be inserted if the target module has not been loaded,
 *  the insertion point is not executable, or the memory is otherwise
 *  inaccessible.  In such cases, DynamoRIO retains all valid probe
 *  information and inserts these probes when the memory locations become
 *  available.
 *
 * \remarks
 * When dr_register_probes() is called after dr_client_main(), the behavior is
 *  identical to being called from dr_client_main() with one caveat: even valid probes
 *  aren't guaranteed to be registered when dr_register_probes() returns.
 *  However, valid probes will be registered within a few milliseconds usually.
 *  To prevent performance and potential deadlock issues, it is recommended
 *  that a client shouldn't wait in a loop till the probe status changes.
 *  Instead, clients should check probe status at a later callback.
 *
 * \remarks
 * A client can determine the status of a registered probe in one of two ways.
 *  1) Read it from the status field in the associated dr_probe_desc_t element
 *  when dr_register_probes() returns, or 2) call dr_get_probe_status() with
 *  the id of the probe.
 *
 * \remarks
 * To add, remove or update currently registered probes dr_register_probes()
 *  should be called again with a new set of probe definitions.  The new list
 *  of probes completely replaces the existing probes.  That is, existing
 *  probes not specified in the new list are removed from the process.
 *
 * \remarks
 * The probe insertion address has limitations.  5 bytes beginning at the start
 *  of a probe insertion address should have specific characteristics.  No
 *  instruction should straddle this start of this region, i.e., the probe
 *  insertion address should be the beginning of an instruction.  Also, no flow
 *  of control should jump into the middle of this 5 byte region beginning at
 *  the probe insertion address.  Further, there should be no int instructions
 *  in this region.  call and jump instructions are allowed in this region as
 *  long as they don't terminate before the end of the region, i.e., no other
 *  instruction should start after them in this region (as it will allow
 *  control flow to return to the middle of this region).  If these rules are
 *  not adhered to the results are be unspecified; may result in process crash.
 *  The above mentioned restrictions hold only when using \ref API_probe not
 *  when using API_BT or both simultaneously.
 *
 * \remarks
 * If only the \ref API_probe is used 5 bytes starting at the probe insertion
 *  address will be modified.  If the process will read this memory then the
 *  probe should be moved to another location or removed to avoid unknown
 *  changes in process behavior.  If the client will read this memory, then it
 *  has to do so before requesting probe insertion.
 *
 * \see dr_get_probe_status().
 */
void
dr_register_probes(dr_probe_desc_t *probes, unsigned int num_probes);

DR_API
/** Used to get the current status of a probe.
 *
 * \param[in]   id  Unique identifier of the probe for which status is desired.
 *
 * \param[out]  status  Pointer to user allocated data of type dr_probe_status_t
 *                      in which the status of the probe specified by id is
 *                      returned.  If id matches and status isn't NULL, the
 *                      status for the matching probe is returned.  If id
 *                      doesn't match or if status is NULL, nothing is
 *                      returned in status.
 *
 * \return      If id matches and status is copied successfully, 1 is returned,
 *              else 0 is returned.
 */
int
dr_get_probe_status(unsigned int id, dr_probe_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* _DR_PROBE_H_ */
