#ifndef _ANNOT_H_
#define _ANNOT_H_ 1

#define IS_VALGRIND_ANNOTATION_SHAPE(instr, count) \
    (instr_get_opcode(instr) == OP_xchg) && (count > 4)

#define CURRENT_API_VERSION VERSION_NUMBER_INTEGER

#ifdef CLIENT_INTERFACE

#define IS_ANNOTATION_LABEL(instr) \
    ((instr != NULL) && TEST(INSTR_ANNOTATION, instr->flags) && instr_is_label(instr))

#define IS_ANNOTATION_STACK_ARG(opnd) \
    opnd_is_base_disp(opnd) && (opnd_get_base(opnd) == REG_XSP)

/* DR_API EXPORT TOFILE dr_annot.h */
/* DR_API EXPORT BEGIN */

/*********************************************************
 * ROUTINES TO REGISTER ANNOTATION HANDLERS
 */
/**
 * @file dr_annot.h
 * @brief Annotation handler registration routines.
 */

#define ANNOT_REGISTER_CALL_VARG(drcontext, handle, target_name, call, num_args, ...) \
do { \
    generic_func_t target = dr_get_proc_address(handle, target_name); \
    if (target != NULL) \
        annot_register_call_varg(drcontext, target, call, false, num_args, __VA_ARGS__); \
} while (0)

enum {
    VG_PATTERN_LENGTH = 5,
    VG_NUM_ARGS = 5,
};

typedef enum _valgrind_request_id_t {
    VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
    VG_ID__LAST
} valgrind_request_id_t;

typedef struct _vg_client_request_t {
    ptr_uint_t request;
    ptr_uint_t args[VG_NUM_ARGS];
    ptr_uint_t default_result;
} vg_client_request_t;

#ifndef X64
typedef enum _annotation_call_type_t {
    ANNOT_FASTCALL,
    ANNOT_STDCALL
} annotation_call_type_t;
#endif

typedef enum _handler_type_t {
    ANNOT_HANDLER_CALL,
    ANNOT_HANDLER_RETURN_VALUE,
    ANNOT_HANDLER_VALGRIND,
    ANNOT_HANDLER_LAST
} handler_type_t;

typedef struct _annotation_handler_t {
    handler_type_t type;
    union {
        app_pc annotation_func;
        valgrind_request_id_t vg_request_id;
    } id;
    union { // per type
        void *callback;
        void *return_value;
        ptr_uint_t (*vg_callback)(vg_client_request_t *request);
    } instrumentation;
    bool save_fpstate;
    uint num_args;
    opnd_t *args;
    struct _annotation_handler_t *next_handler;
} annotation_handler_t;

/* DR_API EXPORT END */

DR_API
void annot_register_call_varg(void *drcontext, void *annotation_func,
                              void *callee, bool save_fpstate, uint num_args, ...);

DR_API
bool annot_find_and_register_call(void *drcontext, const module_data_t *module,
                                  const char *target_name, void *callee, uint num_args
                                  _IF_NOT_X64(annotation_call_type_t type));

DR_API
void annot_register_call(void *drcontext, void *annotation_func, void *callee,
                         bool save_fpstate, uint num_args
                         _IF_NOT_X64(annotation_call_type_t type));

DR_API
void annot_register_return(void *drcontext, void *annotation_func, void *return_value);

DR_API
void annot_register_valgrind(valgrind_request_id_t request,
    ptr_uint_t (*annotation_callback)(vg_client_request_t *request));

// TODO: deregister
// TODO: drcontext is only used for HEAP_ARRAY_ALLOC. Always use GLOBAL_DCONTEXT instead?

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
 * Example Valgrind annotation sequence from annotations test:
 * <C code to fill _zzq_args>
 * lea    0xffffffe4(%ebp) -> %eax      ; lea _zzq_args -> %eax
 * mov    0x08(%ebp) -> %edx            ; mov _zzq_default -> %edx
 * rol    $0x00000003 %edi -> %edi      ; Special sequence to replace
 * rol    $0x0000000d %edi -> %edi
 * rol    $0x0000001d %edi -> %edi
 * rol    $0x00000013 %edi -> %edi
 * xchg   %ebx %ebx -> %ebx %ebx
 *
 * FIXME: If the pattern gets split up by -max_bb_instrs, we will not be able to
 * match it.  If the application is built without optimizations, the client
 * request will not be inlined, so it is unlikely that it will be in a bb bigger
 * than 256 instrs.
 */
bool
match_valgrind_pattern(dcontext_t *dc, instrlist_t *bb, instr_t *instr);

#endif

#endif /* CLIENT_INTERFACE */
