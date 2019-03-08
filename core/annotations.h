/* ******************************************************
 * Copyright (c) 2014-2019 Google, Inc.  All rights reserved.
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

#ifndef _ANNOTATIONS_H_
#define _ANNOTATIONS_H_ 1

#ifdef ANNOTATIONS /* around whole file */

#    include "lib/instrument.h"

/* DR_API EXPORT TOFILE dr_annotation.h */
/* DR_API EXPORT BEGIN */

/*********************************************************
 * ROUTINES TO REGISTER ANNOTATION HANDLERS
 */
/**
 * @file dr_annotation.h
 * @brief Annotation handler registration routines.
 */

#    ifdef CLIENT_INTERFACE

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

#    endif /* CLIENT_INTERFACE */

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
#    ifdef X64
    /**
     * The calling convention for vararg functions on x64, which must be "fastcall".
     */
    DR_ANNOTATION_CALL_TYPE_VARARG = DR_ANNOTATION_CALL_TYPE_FASTCALL,
#    else
    DR_ANNOTATION_CALL_TYPE_STDCALL, /**< x86 calling convention "stdcall" */
    /**
     * The calling convention for vararg functions on x86, which must be "stdcall".
     */
    DR_ANNOTATION_CALL_TYPE_VARARG = DR_ANNOTATION_CALL_TYPE_STDCALL,
#    endif
    DR_ANNOTATION_CALL_TYPE_LAST /**< Sentinel value for iterator convenience */
} dr_annotation_calling_convention_t;

/* DR_API EXPORT END */

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

/*********************************************************
 * INTERNAL ANNOTATION OBJECTS AND ROUTINES
 */

#    ifdef X64
/* x64 encoding of "xchg %ebx,%ebx" (endian reversed) */
#        define ENCODED_VALGRIND_ANNOTATION_TAIL 0xdb8748
/* x64 encoding of "rol $0x3,%edi; rol $0xd,%edi; rol $0x1d,%edi; rol $0x13,%edi"
 * These constants facilitate efficient comparison of the raw bytes (endian
 * reversed).
 */
#        define ENCODED_VALGRIND_ANNOTATION_WORD_1 0xdc7c14803c7c148ULL
#        define ENCODED_VALGRIND_ANNOTATION_WORD_2 0x33c7c1483dc7c148ULL
#    else
/* x86 encoding of "xchg %ebx,%ebx" (endian reversed) */
#        define ENCODED_VALGRIND_ANNOTATION_TAIL 0xdb87
/* x86 encoding of "rol $0x3,%edi; rol $0xd,%edi; rol $0x1d,%edi; rol $0x13,%edi"
 * These constants facilitate efficient comparison of the raw bytes (endian
 * reversed).
 */
#        define ENCODED_VALGRIND_ANNOTATION_WORD_1 0xc103c7c1UL
#        define ENCODED_VALGRIND_ANNOTATION_WORD_2 0xc7c10dc7UL
#        define ENCODED_VALGRIND_ANNOTATION_WORD_3 0x13c7c11dUL
#    endif

#    if !(defined(WINDOWS) && defined(X64))
#        ifdef WINDOWS
#            define ANNOTATION_JUMP_OVER_LABEL_REFERENCE 0x06eb
#        else
#            ifdef X64
#                define ANNOTATION_JUMP_OVER_LABEL_REFERENCE 0x11eb
#            else
#                define ANNOTATION_JUMP_OVER_LABEL_REFERENCE 0x0ceb
#            endif
#        endif
#    endif

#    define GET_ANNOTATION_HANDLER(label_data) \
        ((dr_annotation_handler_t *)(label_data)->data[0])
#    define SET_ANNOTATION_HANDLER(label_data, pc)    \
        do {                                          \
            (label_data)->data[0] = (ptr_uint_t)(pc); \
        } while (0)

#    define GET_ANNOTATION_APP_PC(label_data) ((app_pc)(label_data)->data[1])
#    define SET_ANNOTATION_APP_PC(label_data, pc)     \
        do {                                          \
            (label_data)->data[1] = (ptr_uint_t)(pc); \
        } while (0)

#    define GET_ANNOTATION_INSTRUMENTATION_PC(label_data) ((app_pc)(label_data)->data[2])
#    define SET_ANNOTATION_INSTRUMENTATION_PC(label_data, pc) \
        do {                                                  \
            (label_data)->data[2] = (ptr_uint_t)(pc);         \
        } while (0)

typedef enum _dr_annotation_handler_type_t {
    DR_ANNOTATION_HANDLER_CALL,
    DR_ANNOTATION_HANDLER_RETURN_VALUE,
    DR_ANNOTATION_HANDLER_VALGRIND,
    DR_ANNOTATION_HANDLER_LAST
} dr_annotation_handler_type_t;

/* Each receiver represents one registered client (or core DR). */
typedef struct _dr_annotation_receiver_t {
    union { /* per annotation_handler_t.type */
        void *callback;
        void *return_value;
        ptr_uint_t (*vg_callback)(dr_vg_client_request_t *request);
    } instrumentation;
    bool save_fpstate;
    struct _dr_annotation_receiver_t *next;
} dr_annotation_receiver_t;

/* Each handler represents one distinct annotation name. */
typedef struct _dr_annotation_handler_t {
    dr_annotation_handler_type_t type;
    const char *symbol_name; /* not used for Valgrind annotations */
    dr_annotation_receiver_t *receiver_list;
    uint num_args;
    opnd_t *args;
    bool is_void;
} dr_annotation_handler_t;

void
annotation_init();

void
annotation_exit();

static inline bool
is_annotation_label(instr_t *instr)
{
    if (instr != NULL && instr_is_label(instr))
        return (ptr_uint_t)instr_get_note(instr) == DR_NOTE_ANNOTATION;

    return false;
}

static inline bool
is_annotation_return_placeholder(instr_t *instr)
{
    if (instr != NULL && instr_get_opcode(instr) == OP_mov_st)
        return (ptr_uint_t)instr_get_note(instr) == DR_NOTE_ANNOTATION;

    return false;
}

#    if defined(WINDOWS) && defined(X64)
/* Win64 uses a compiled annotation, so the jump over dead code is not constant. */
static inline bool
is_annotation_jump_over_dead_code(instr_t *instr)
{
    return instr_is_cbr(instr);
}
#    else
/* Other platforms use inline assembly, so the annotation starts with a constant jump
 * instruction for efficient identification.
 */
static inline bool
is_annotation_jump_over_dead_code(instr_t *instr)
{
    app_pc xl8 = instr_get_translation(instr);
    return xl8 != NULL && *(ushort *)xl8 == ANNOTATION_JUMP_OVER_LABEL_REFERENCE;
}
#    endif

static inline bool
is_decoded_valgrind_annotation_tail(instr_t *instr)
{
    return instr_get_opcode(instr) == OP_xchg;
}

#    ifdef X64
#        ifndef WINDOWS
/* Return true if instr_start_pc could be the last instruction of a Valgrind annotation,
 * or false if it definitely is not a Valgrind annotation.
 */
static inline bool
is_encoded_valgrind_annotation_tail(app_pc instr_start_pc)
{
    return (((*(uint *)instr_start_pc) & 0xffffff) == ENCODED_VALGRIND_ANNOTATION_TAIL);
}

/* Return true if xchg_start_pc is definitely the last instruction of a Valgrind
 * annotation.
 */
static inline bool
is_encoded_valgrind_annotation(app_pc xchg_start_pc, app_pc bb_start, app_pc page_start)
{
    uint64 word1, word2;
    /* It must be safe to read if the whole annotation is on a readable page, or falls
     * within the range of pcs that have already been decoded for this bb.
     */
    bool safe_to_read = (xchg_start_pc - bb_start) >= (2 * sizeof(uint64)) ||
        (xchg_start_pc - page_start) >= (2 * sizeof(uint64));
    if (safe_to_read) {
        word1 = *(uint64 *)(xchg_start_pc - (2 * sizeof(uint64)));
    } else {
        if (!d_r_safe_read(xchg_start_pc - (2 * sizeof(uint64)), sizeof(uint64), &word1))
            return false; /* If it's not safe to read, it must not be an annotation. */
    }
    /* This word must be safe to read because it lies directly between two pcs that are
     * safe to read, with nothing intervening.
     */
    word2 = *(uint64 *)(xchg_start_pc - sizeof(uint64));

    return word1 == ENCODED_VALGRIND_ANNOTATION_WORD_1 &&
        word2 == ENCODED_VALGRIND_ANNOTATION_WORD_2;
}
#        endif
#    else
/* Return true if instr_start_pc could be the last instruction of a Valgrind annotation,
 * or false if it definitely is not a Valgrind annotation.
 */
static inline bool
is_encoded_valgrind_annotation_tail(app_pc instr_start_pc)
{
    return (*(ushort *)instr_start_pc == ENCODED_VALGRIND_ANNOTATION_TAIL);
}

/* Return true if xchg_start_pc is definitely the last instruction of a Valgrind
 * annotation.
 */
static inline bool
is_encoded_valgrind_annotation(app_pc xchg_start_pc, app_pc bb_start, app_pc page_start)
{
    uint64 word1, word2, word3;
    /* It must be safe to read if the whole annotation is on a readable page, or falls
     * within the range of pcs that have already been decoded for this bb.
     */
    bool safe_to_read = (xchg_start_pc - bb_start) >= (3 * sizeof(uint)) ||
        (xchg_start_pc - page_start) >= (3 * sizeof(uint));
    if (safe_to_read) {
        word1 = *(uint *)(xchg_start_pc - (3 * sizeof(uint)));
    } else {
        if (!d_r_safe_read(xchg_start_pc - (3 * sizeof(uint)), sizeof(uint), &word1))
            return false; /* If it's not safe to read, it must not be an annotation. */
    }
    /* These words must be safe to read because they lie directly between two pcs that are
     * safe to read, with nothing intervening.
     */
    word2 = *(uint *)(xchg_start_pc - (2 * sizeof(uint)));
    word3 = *(uint *)(xchg_start_pc - sizeof(uint));
    return word1 == ENCODED_VALGRIND_ANNOTATION_WORD_1 &&
        word2 == ENCODED_VALGRIND_ANNOTATION_WORD_2 &&
        word3 == ENCODED_VALGRIND_ANNOTATION_WORD_3;
}
#    endif

/* Instrument the DR annotation at `start_pc` with an instruction sequence `substitution`,
 * or return false if there is no DR annotation at `start_pc`. For Windows x64,
 * `hint_is_safe` indicates whether the hint byte following `start_pc` is already known
 * to be safe for reading. On successful instrumentation, `start_pc` is replaced with the
 * pc following the annotation, where decoding of app instructions should resume.
 */
bool
instrument_annotation(dcontext_t *dcontext, IN OUT app_pc *start_pc,
                      OUT instr_t **substitution _IF_WINDOWS_X64(IN bool hint_is_safe));

#    if !(defined(WINDOWS) && defined(X64))
/* Replace the Valgrind annotation code sequence with a clean call to
 * an internal function which will dispatch to registered handlers.
 *
 * Return true if the replacement occurred, and set next_instr to the first
 * instruction after the annotation sequence.
 *
 * Example Valgrind annotation sequence from 'vg-annot' test (x86):
 * <C code to fill _zzq_args>
 * lea    0xffffffe4(%ebp) -> %eax      ; lea _zzq_args -> %eax
 * mov    0x08(%ebp) -> %edx            ; mov _zzq_default -> %edx
 * rol    $0x00000003 %edi -> %edi      ; Special sequence to replace
 * rol    $0x0000000d %edi -> %edi
 * rol    $0x0000001d %edi -> %edi
 * rol    $0x00000013 %edi -> %edi
 * xchg   %ebx %ebx -> %ebx %ebx
 */
void
instrument_valgrind_annotation(dcontext_t *dcontext, instrlist_t *bb, instr_t *xchg_instr,
                               app_pc xchg_pc, app_pc next_pc, uint bb_instr_count);
#    endif /* !(defined (WINDOWS) && defined (X64)) */

#endif /* ANNOTATIONS */

#endif
