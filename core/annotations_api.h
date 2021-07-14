/* ******************************************************
 * Copyright (c) 2014-2021 Google, Inc.  All rights reserved.
 * ******************************************************/

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

#ifndef _DR_ANNOTATION_H_
#define _DR_ANNOTATION_H_ 1

#include "dr_tools.h"

/*********************************************************
 * ROUTINES TO REGISTER ANNOTATION HANDLERS
 */
/**
 * @file dr_annotation.h
 * @brief Annotation handler registration routines.
 */

/**
 * Facilitates returning a value from an annotation invocation in the target app. This
 * function should be used within the annotation clean call, and the specified value will
 * be received in the target app by the annotation caller. This function may be invoked
 * multiple times, in which case only the last value will take effect.
 *
 * @param[in] value     The return value (unsigned, pointer-sized) that will be received
 *                      in the target app.
 */
static inline void
dr_annotation_set_return_value(reg_t value)
{
    dr_mcontext_t mcontext;
    void *dcontext = dr_get_current_drcontext();
    mcontext.size = sizeof(dr_mcontext_t);
    mcontext.flags = DR_MC_INTEGER;
    dr_get_mcontext(dcontext, &mcontext);
    mcontext.xax = value;
    dr_set_mcontext(dcontext, &mcontext);
}

/**
 * Synonyms for the Valgrind client request IDs (sequential from 0 for convenience).
 */
typedef enum _dr_valgrind_request_id_t {
    /**
     * Return true if running in DynamoRIO with Valgrind annotation support. No args.
     */
    DR_VG_ID__RUNNING_ON_VALGRIND,
    /**
     * Request an immediate memory scan to look for leaks.
     * Not currently implemented in core DR.
     */
    DR_VG_ID__DO_LEAK_CHECK,
    /**
     * Indicate that the specified range of addresses should be considered defined if
     * it is addressable. Not currently implemented in core DR.
     */
    DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
    /**
     * Request that DynamoRIO discard all fragments in the code cache that were
     * translated from the specified range of addresses. The request takes 2 args,
     * start and length. It is implemented in core DR with JIT optimization enabled,
     * though clients may implement additional functionality.
     */
    DR_VG_ID__DISCARD_TRANSLATIONS,
    /**
     * Sentinel value for iterator convenience.
     */
    DR_VG_ID__LAST
} dr_valgrind_request_id_t;

enum {
    /**
     * Defines the maximum number of arguments that can be passed to a Valgrind
     * annotation, and accordingly specifies the length of the array
     * vg_client_request_t.args.
     */
    DR_VG_NUM_ARGS = 5,
};

/**
 * Defines the Valgrind client request object, which is constructed by each instance of
 * a Valgrind annotation in the target app. An instance is passed to Valgrind annotation
 * callback functions to make the arguments available. Some arguments may be
 * undefined for some Valgrind client requests; see the Valgrind documentation for each
 * specific Valgrind client request for details about the arguments.
 */
typedef struct _dr_vg_client_request_t {
    ptr_uint_t request;
    ptr_uint_t args[DR_VG_NUM_ARGS];
    ptr_uint_t default_result;
} dr_vg_client_request_t;

/**
 * Defines the calling conventions that are supported for annotation functions
 * (as they appear in the target app).
 */
typedef enum _dr_annotation_calling_convention_t {
    DR_ANNOTATION_CALL_TYPE_FASTCALL, /**< calling convention "fastcall" */
#ifdef X64
    /**
     * The calling convention for vararg functions on x64, which must be "fastcall".
     */
    DR_ANNOTATION_CALL_TYPE_VARARG = DR_ANNOTATION_CALL_TYPE_FASTCALL,
#else
    DR_ANNOTATION_CALL_TYPE_STDCALL, /**< x86 calling convention "stdcall" */
    /**
     * The calling convention for vararg functions on x86, which must be "stdcall".
     */
    DR_ANNOTATION_CALL_TYPE_VARARG = DR_ANNOTATION_CALL_TYPE_STDCALL,
#endif
    DR_ANNOTATION_CALL_TYPE_LAST /**< Sentinel value for iterator convenience */
} dr_annotation_calling_convention_t;

DR_API
/**
 * Register a handler for a DR annotation. When the annotation is encountered, it will
 * be replaced with a clean call to the specified callee, which must have the specified
 * number of arguments. Returns true on successful registration.
 *
 * @param[in] annotation_name  The name of the annotation function as it appears in the
 *                             target app's source code (unmangled).
 * @param[in] callee           The client function to call when an instance of the
 *                             specified annotation is executed.
 * @param[in] save_fpstate     Indicates whether to save floating point state before
 *                             making the clean call to the callee.
 * @param[in] num_args         The number of arguments in the annotation function (must
 *                             also match the number of arguments in the callee).
 * @param[in] call_type        The calling convention of the annotation function as
 *                             compiled in the target app (x86 only).
 */
bool
dr_annotation_register_call(const char *annotation_name, void *callee, bool save_fpstate,
                            uint num_args, dr_annotation_calling_convention_t call_type);

DR_API
/**
 * Register a callback for a Valgrind client request id. When the request is encountered,
 * the specified callback will be invoked by an internal routing function. Returns true
 * on successful registration.
 *
 * @param[in] request_id            The Valgrind request id for which to register.
 * @param[in] annotation_callback   The client function to call when an instance of the
 *                                  specified Valgrind client request is executed.
 */
bool
dr_annotation_register_valgrind(
    dr_valgrind_request_id_t request_id,
    ptr_uint_t (*annotation_callback)(dr_vg_client_request_t *request));

DR_API
/**
 * Register a return value substitution for a DR annotation. When the annotation is
 * encountered, it will be replaced with the specified return value. This function
 * returns true on successful registration.
 *
 * @param[in] annotation_name  The name of the annotation function as it appears in the
 *                             target app's source code (unmangled).
 * @param[in] return_value     The value to return from every instance of the annotation.
 */
bool
dr_annotation_register_return(const char *annotation_name, void *return_value);

DR_API
/**
 * Can only be called on an annotation already registered with
 * dr_annotation_register_call().  When the annotation is encountered, the PC of
 * the annotation interruption point will be available in DR scratch slot #SPILL_SLOT_2,
 * which can be read with dr_read_saved_reg().
 *
 * @param[in] annotation_name  The name of the annotation function as it appears in the
 *                             target app's source code (unmangled).
 */
bool
dr_annotation_pass_pc(const char *annotation_name);

DR_API
/**
 * Unregister the specified handler from a DR annotation. Instances of the annotation that
 * have already been substituted with a clean call to the registered callee will remain in
 * the code cache, but any newly encountered instances of the annotation will no longer be
 * substituted with this callee. This function does nothing in the case that the
 * specified callee was never registered for this annotation (or has already been
 * unregistered). Returns true if the registration was successfully unregistered.
 *
 * @param[in] annotation_name  The annotation function for which to unregister.
 * @param[in] callee           The callee to unregister.
 */
bool
dr_annotation_unregister_call(const char *annotation_name, void *callee);

DR_API
/**
 * Unregister the (universal) return value from a DR annotation. Instances of the
 * annotation that have already been substituted with the return value will remain in the
 * code cache, but any newly encountered instances of the annotation will no longer be
 * substituted. This function does nothing in the case that no return value is currently
 * registered for the specified annotation (or has already been unregistered). Note that
 * if another client has registered a return value, this function will remove that other
 * client's registration. Returns true if the registration was successfully unregistered.
 *
 * @param[in] annotation_name  The annotation function for which to unregister.
 */
bool
dr_annotation_unregister_return(const char *annotation_name);

DR_API
/**
 * Unregister the specified callback from a Valgrind client request. The registered
 * callback will not be invoked in association with this client request again (though if
 * the same callback function is also registered for other Valgrind client requests, it
 * will still be invoked in association with those client requests). This function does
 * nothing in the case that no handler was ever registered for this callback function.
 * Returns true if the registration was found and successfully unregistered.
 *
 * @param[in] request_id              The request id for which to unregister (DR_VG__*).
 * @param[in] annotation_callback     The callback function to unregister.
 */
bool
dr_annotation_unregister_valgrind(
    dr_valgrind_request_id_t request_id,
    ptr_uint_t (*annotation_callback)(dr_vg_client_request_t *request));

#endif /* _DR_ANNOTATION_H_ */
