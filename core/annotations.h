/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

#ifndef _ANNOT_H_
#define _ANNOT_H_ 1

#define IS_DECODED_VALGRIND_ANNOTATION_TAIL(instr) \
    (instr_get_opcode(instr) == OP_xchg)

#ifdef X64
# define IS_ENCODED_VALGRIND_ANNOTATION_TAIL(instr_start_pc) \
    ((*(uint *) instr_start_pc & 0xffffff) == 0xdb8748)
# define IS_ENCODED_VALGRIND_ANNOTATION(xchg_start_pc) \
    ((*(uint64 *) (xchg_start_pc - 0x10) == 0xdc7c14803c7c148ULL) && \
     (*(uint64 *) (xchg_start_pc - 8) == 0x33c7c1483dc7c148ULL))

#else
# define IS_ENCODED_VALGRIND_ANNOTATION_TAIL(instr_start_pc) \
    (*(ushort *) instr_start_pc == 0xdb87)
# define IS_ENCODED_VALGRIND_ANNOTATION(xchg_start_pc) \
    ((*(uint *) (xchg_start_pc - 0xc) == 0xc103c7c1UL) && \
     (*(uint *) (xchg_start_pc - 8) == 0xc7c10dc7) && \
     (*(uint *) (xchg_start_pc - 4) == 0x13c7c11d))
#endif

#define IS_ANNOTATION_LABEL(instr) ((instr != NULL) && instr_is_label(instr) && \
    ((ptr_uint_t)instr_get_note(instr) == DR_NOTE_ANNOTATION))

#define IS_ANNOTATION_STACK_ARG(opnd) \
    opnd_is_base_disp(opnd) && (opnd_get_base(opnd) == REG_XSP)

#ifdef WINDOWS
# define IS_ANNOTATION_JUMP_OVER_DEAD_CODE(instr) \
    (instr_is_cbr(instr) && opnd_is_pc(instr_get_src(instr, 0)))
#else
# define IS_ANNOTATION_JUMP_OVER_DEAD_CODE(instr) \
    (instr_is_ubr(instr) && opnd_is_pc(instr_get_src(instr, 0)))
#endif

#define GET_ANNOTATION_PC(label_data) ((app_pc) label_data->data[2])

#define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO_NAME \
    "dynamorio_annotate_running_on_dynamorio"

#define CURRENT_API_VERSION VERSION_NUMBER_INTEGER

/* DR_API EXPORT TOFILE dr_annotation.h */
/* DR_API EXPORT BEGIN */

/*********************************************************
 * ROUTINES TO REGISTER ANNOTATION HANDLERS
 */
/**
 * @file dr_annotation.h
 * @brief Annotation handler registration routines.
 */

#define ANNOT_REGISTER_CALL_VARG(drcontext, handle, target_name, call, num_args, ...) \
do { \
    generic_func_t target = dr_get_proc_address(handle, target_name); \
    if (target != NULL) \
        annot_register_call_varg(drcontext, target, call, false, num_args, __VA_ARGS__); \
} while (0)

typedef enum _valgrind_request_id_t {
    VG_ID__RUNNING_ON_VALGRIND,
    VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
    VG_ID__LAST
} valgrind_request_id_t;

enum {
    VG_PATTERN_LENGTH = 5,
    VG_NUM_ARGS = 5,
};

typedef struct _vg_client_request_t {
    ptr_uint_t request;
    ptr_uint_t args[VG_NUM_ARGS];
    ptr_uint_t default_result;
} vg_client_request_t;

#ifndef X64
typedef enum _annotation_calling_convention_t {
    ANNOT_CALL_TYPE_FASTCALL,
    ANNOT_CALL_TYPE_STDCALL,
    ANNOT_CALL_TYPE_NONE
} annotation_calling_convention_t;
#endif

typedef enum _handler_type_t {
    ANNOT_HANDLER_CALL,
    ANNOT_HANDLER_RETURN_VALUE,
    ANNOT_HANDLER_VALGRIND,
    ANNOT_HANDLER_LAST
} handler_type_t;

typedef struct _annotation_receiver_t {
    client_id_t client_id;
    union { // per annotation_handler_t.type
        void *callback;
        void *return_value;
        ptr_uint_t (*vg_callback)(vg_client_request_t *request);
    } instrumentation;
    bool save_fpstate;
    struct _annotation_receiver_t *next;
} annotation_receiver_t;


typedef struct _annotation_handler_t {
    handler_type_t type;
    const char *symbol_name; // NULL if never registered by name
    union {
        app_pc annotation_func;
        valgrind_request_id_t vg_request_id;
    } id;
    annotation_receiver_t *receiver_list;
    uint num_args;
    opnd_t *args;
    uint arg_stack_space;
} annotation_handler_t;

/* DR_API EXPORT END */

typedef enum _annotation_call_t {
    ANNOT_NORMAL_CALL,
    ANNOT_TAIL_CALL
} annotation_call_t;


DR_API
void dr_annot_register_call_by_name(client_id_t client_id, const char *target_name,
                                    void *callee, bool save_fpstate, uint num_args
                                    _IF_NOT_X64(annotation_calling_convention_t call_type));

DR_API
void dr_annot_register_call(client_id_t client_id, void *annotation_func, void *callee,
                            bool save_fpstate, uint num_args
                            _IF_NOT_X64(annotation_calling_convention_t call_type));

DR_API
void dr_annot_register_call_ex(client_id_t client_id, void *annotation_func,
                               void *callee, bool save_fpstate, uint num_args, ...);

DR_API
void dr_annot_register_valgrind(client_id_t client_id, valgrind_request_id_t request,
    ptr_uint_t (*annotation_callback)(vg_client_request_t *request));

DR_API
void dr_annot_register_return_by_name(const char *target_name, void *return_value);

DR_API
void dr_annot_register_return(void *annotation_func, void *return_value);

DR_API
void dr_annot_unregister_call_by_name(client_id_t client_id, const char *target_name);

DR_API
void dr_annot_unregister_call(client_id_t client_id, void *annotation_func);

DR_API
void dr_annot_unregister_valgrind(client_id_t client_id, valgrind_request_id_t request);

DR_API
void dr_annot_unregister_return_by_name(const char *target_name);

DR_API
void dr_annot_unregister_return(void *annotation_func);

void
annot_init();

void
annot_exit();

instr_t *
annot_match(dcontext_t *dcontext, instr_t *instr);

/* Replace the Valgrind annotation code sequence with a clean call to
 * an internal function which will dispatch to registered handlers.
 *
 * Return true if the replacement occurred, and set next_instr to the first
 * instruction after the annotation sequence.
 *
 * Example Valgrind annotation sequence from annotations test (x86):
 * <C code to fill _zzq_args>
 * lea    0xffffffe4(%ebp) -> %eax      ; lea _zzq_args -> %eax
 * mov    0x08(%ebp) -> %edx            ; mov _zzq_default -> %edx
 * rol    $0x00000003 %edi -> %edi      ; Special sequence to replace
 * rol    $0x0000000d %edi -> %edi
 * rol    $0x0000001d %edi -> %edi
 * rol    $0x00000013 %edi -> %edi
 * xchg   %ebx %ebx -> %ebx %ebx
 */
bool
match_valgrind_pattern(dcontext_t *dcontext, instrlist_t *bb, instr_t *instr,
                       app_pc xchg_pc, uint bb_instr_count);

#ifdef WINDOWS
void
annot_module_load(module_handle_t base);

void
annot_module_unload(module_handle_t base, size_t size);
#endif

#endif
