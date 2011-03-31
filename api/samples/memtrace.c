/* ******************************************************************************
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * ******************************************************************************/

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

/* Code Manipulation API Sample:
 * memtrace.c
 * 
 * Collects the address of every memory reference and dumps the results to a file.
 * Illustrates how to create own code cache and perform lean procedure call.
 * (1) It fills a buffer and dumps the buffer when it is full.
 * (2) It inlines the buffer filling code to avoid full context switch.
 * (3) It uses lean procedure calling clean call to reduce code cache size.
 */
/* Known issues and possible solutions:
 * This sample is to demonstrate the basics of collecting memory reference address,
 * and in order to keep it simple, there are several issues of this sample
 * for real usage.
 * 1) far memory reference address:
 *    We did not calculate the real address for memory reference 
 *    explicitly using a segment register (i.e. far memory reference).
 *    We can can get segment base from opnd_compute_address()
 *    and either store for use in gencode or postprocess, w/
 *    complexities of monitoring changes in segment.
 *    A forthcoming Extension will provide a utility routine for this.
 * 2) memory reference size 
 *    We did not record the memory reference size.
 *    It is easy to add size for static but a little more work 
 *    for OP_enter and string ops.  For string ops a forthcoming
 *    Extension will support transforming string loops into
 *    regular loops.
 */

#include <string.h> /* for memset */
#include <stddef.h> /* for offsetof */
#include "dr_api.h"

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) buf[(sizeof(buf)/sizeof(buf[0])) - 1] = '\0'


/* Each mem_ref_t includes the type of reference (read or write), 
 * and the address referenced.
 */
typedef struct _mem_ref_t{
    bool  write;
    void *addr;
} mem_ref_t;

/* Control the format of memory trace: readable or hexl */
#define READABLE_TRACE 
/* Max number of mem_ref a buffer can have */
#define MAX_NUM_MEM_REFS 8192
/* The size of memory buffer for holding mem_refs. When it fills up, 
 * we dump data from the buffer to the file.
 */
#define MEM_BUF_SIZE (sizeof(mem_ref_t) * MAX_NUM_MEM_REFS)

/* thread private log file and counter */
typedef struct {
    char   *buf_ptr;
    char   *buf_base;
    /* buf_end holds the negative value of real address of buffer end. */
    ptr_int_t buf_end; 
    void   *cache;
    file_t  log;
    uint64  num_refs;
} per_thread_t;

static client_id_t client_id;
static app_pc code_cache;
static void  *mutex;    /* for multithread support */
static uint64 num_refs; /* keep a global memory reference count */

static void event_exit(void);
static void event_thread_init(void *drcontext);
static void event_thread_exit(void *drcontext);
static dr_emit_flags_t event_basic_block(void *drcontext, 
                                         void *tag, 
                                         instrlist_t *bb,
                                         bool for_trace, 
                                         bool translating);

static void clean_call(void);
static void memtrace(void *drcontext);
static void code_cache_init(void);
static void code_cache_exit(void);
static void instrument_mem(void        *drcontext, 
                           instrlist_t *ilist, 
                           instr_t     *where, 
                           int          pos, 
                           bool         write);

DR_EXPORT void 
dr_init(client_id_t id)
{
    client_id = id;
    mutex = dr_mutex_create();
    dr_register_exit_event(event_exit);
    dr_register_thread_init_event(event_thread_init);
    dr_register_thread_exit_event(event_thread_exit);
    dr_register_bb_event(event_basic_block);

    code_cache_init();
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, LOG_ALL, 1, "Client 'memtrace' initializing\n");
#ifdef SHOW_RESULTS
    if (dr_is_notify_on())
        dr_fprintf(STDERR, "Client memtrace is running\n");
#endif
}


static void
event_exit()
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "  saw %llu memory references\n",
                      num_refs);
    DR_ASSERT(len > 0);
    NULL_TERMINATE(msg);
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
    code_cache_exit();
    dr_mutex_destroy(mutex);
}

#ifdef WINDOWS
# define IF_WINDOWS(x) x
#else
# define IF_WINDOWS(x) /* nothing */
#endif

static void 
event_thread_init(void *drcontext)
{
    char logname[MAXIMUM_PATH];
    char *dirsep;
    int len;
    per_thread_t *data;

    /* allocate thread private data */
    data = dr_thread_alloc(drcontext, sizeof(per_thread_t));
    dr_set_tls_field(drcontext, data);
    data->buf_base = dr_thread_alloc(drcontext, MEM_BUF_SIZE);
    data->buf_ptr  = data->buf_base;
    /* set buf_end to be negative of address of buffer end for the lea later */
    data->buf_end  = -(ptr_int_t)(data->buf_base + MEM_BUF_SIZE);
    data->num_refs = 0;

    /* We're going to dump our data to a per-thread file.
     * On Windows we need an absolute path so we place it in
     * the same directory as our library. We could also pass
     * in a path and retrieve with dr_get_options().
     */
    len = dr_snprintf(logname, sizeof(logname)/sizeof(logname[0]),
                      "%s", dr_get_client_path(client_id));
    DR_ASSERT(len > 0);
    for (dirsep = logname + len; *dirsep != '/' IF_WINDOWS(&& *dirsep != '\\'); dirsep--)
        DR_ASSERT(dirsep > logname);
    len = dr_snprintf(dirsep + 1,
                      (sizeof(logname) - (dirsep - logname))/sizeof(logname[0]),
                      "memtrace.%d.log", dr_get_thread_id(drcontext));
    DR_ASSERT(len > 0);
    NULL_TERMINATE(logname);
    data->log = dr_open_file(logname, 
                             DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(data->log != INVALID_FILE);
    dr_log(drcontext, LOG_ALL, 1, 
           "memtrace: log for thread %d is memtrace.%03d\n",
           dr_get_thread_id(drcontext), dr_get_thread_id(drcontext));
}


static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data;

    memtrace(drcontext);
    data = dr_get_tls_field(drcontext);
    dr_mutex_lock(mutex);
    num_refs += data->num_refs;
    dr_mutex_unlock(mutex);
    dr_close_file(data->log);
    dr_thread_free(drcontext, data->buf_base, MEM_BUF_SIZE);
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}


/* event_basic_block scans each basic block, and calls instrument_mem to 
 * instrument every application memory references.
 */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *instr;
    int i;

    for (instr  = instrlist_first(bb); 
         instr != NULL; 
         instr  = instr_get_next(instr)) {
        if (instr_get_app_pc(instr) == NULL)
            continue;
        if (instr_reads_memory(instr)) {
            for (i = 0; i < instr_num_srcs(instr); i++) {
                if (opnd_is_memory_reference(instr_get_src(instr, i))) {
                    instrument_mem(drcontext, bb, instr, i, false);
                }
            }
        }
        if (instr_writes_memory(instr)) {
            for (i = 0; i < instr_num_dsts(instr); i++) {
                if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
                    instrument_mem(drcontext, bb, instr, i, true);
                }
            }
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
memtrace(void *drcontext)
{
    per_thread_t *data;
    int num_refs;
    mem_ref_t *mem_ref;
#ifdef READABLE_TRACE
    int i;
#endif

    data      = dr_get_tls_field(drcontext);
    mem_ref   = (mem_ref_t *)data->buf_base;
    num_refs  = (int)((mem_ref_t *)data->buf_ptr - mem_ref);

#ifdef READABLE_TRACE
    for (i = 0; i < num_refs; i++) {
        dr_fprintf(data->log, "%c:"PFX"\n",
                   mem_ref->write ? 'w' : 'r', mem_ref->addr);
        ++mem_ref;
    }
#else
    dr_write_file(data->log, data->buf_base,
                  (size_t)(data->buf_ptr - data->buf_base));
#endif

    memset(data->buf_base, 0, MEM_BUF_SIZE);
    data->num_refs += num_refs;
    data->buf_ptr   = data->buf_base;
}

/* clean_call dumps the memory reference info to the log file */
static void
clean_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    memtrace(drcontext);
}

static void
code_cache_init(void)
{
    void         *drcontext;
    per_thread_t *data;
    instrlist_t  *ilist;
    instr_t      *where;
    byte         *end;

    drcontext  = dr_get_current_drcontext();
    data       = dr_get_tls_field(drcontext);
    code_cache = dr_nonheap_alloc(PAGE_SIZE, 
                                  DR_MEMPROT_READ  |
                                  DR_MEMPROT_WRITE |
                                  DR_MEMPROT_EXEC);
    ilist = instrlist_create(drcontext);
    /* The lean procecure simply performs a clean call, and then jump back */
    /* jump back to the DR's code cache */
    where = INSTR_CREATE_jmp_ind(drcontext, opnd_create_reg(DR_REG_XCX));
    instrlist_meta_append(ilist, where);
    /* clean call */
    dr_insert_clean_call(drcontext, ilist, where, (void *)clean_call, false, 0);
    /* Encodes the instructions into memory and then cleans up. */
    end = instrlist_encode(drcontext, ilist, code_cache, false);
    DR_ASSERT((end - code_cache) < PAGE_SIZE);
    instrlist_clear_and_destroy(drcontext, ilist);
    /* set the memory as just +rx now */
    dr_memory_protect(code_cache, PAGE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_EXEC);
}


static void
code_cache_exit(void)
{
    void *drcontext;
    per_thread_t *data;

    drcontext = dr_get_current_drcontext();
    data = dr_get_tls_field(drcontext);
    dr_nonheap_free(code_cache, PAGE_SIZE);
}


/*
 * instrument_mem is called whenever a memory reference is identified.
 * It inserts code before the memory reference to to fill the memory buffer
 * and jump to our own code cache to call the clean_call when the buffer is full.
 */
static void
instrument_mem(void *drcontext, instrlist_t *ilist, instr_t *where, 
               int pos, bool write)
{
    instr_t *instr, *call, *restore;
    opnd_t   ref, opnd1, opnd2;
    reg_id_t reg1 = DR_REG_XBX; /* We can optimize it by picking dead reg */
    reg_id_t reg2 = DR_REG_XCX; /* reg2 must be ECX or RCX for jecxz */
    per_thread_t *data;
    
    data = dr_get_tls_field(drcontext);

    /* Steal the register for memory reference address *
     * We can optimize away the unnecessary register save and restore 
     * by analyzing the code and finding the register is dead. 
     */
    dr_save_reg(drcontext, ilist, where, reg1, SPILL_SLOT_2);
    dr_save_reg(drcontext, ilist, where, reg2, SPILL_SLOT_3);

    if (write)
       ref = instr_get_dst(where, pos);
    else
       ref = instr_get_src(where, pos);
    /* Using either lea or mov to load memory reference address
     * XXX: some far memory reference need special handling.
     * See notes at top of file.
     */
    opnd1 = opnd_create_reg(reg1);
    if (opnd_is_base_disp(ref)) {
        /* lea [ref] => reg */
        opnd2 = ref; 
        opnd_set_size(&opnd2, OPSZ_lea);
        instr = INSTR_CREATE_lea(drcontext, opnd1, opnd2);
    } else if(IF_X64(opnd_is_rel_addr(ref) ||) opnd_is_abs_addr(ref)) {
        /* mov addr => reg */
        opnd2 = OPND_CREATE_INTPTR(opnd_get_addr(ref));
        instr = INSTR_CREATE_mov_imm(drcontext, opnd1, opnd2);
    } else {
        instr = NULL;
        DR_ASSERT_MSG(false, "Unhandled instructions");
    }
    instrlist_meta_preinsert(ilist, where, instr);
    
    /* The following assembly performs the following instructions
     * buf_ptr->write = write;
     * buf_ptr->addr  = addr;
     * buf_ptr++;
     * if (buf_ptr >= buf_end_ptr) 
     *    clean_call();
     */
    dr_insert_read_tls_field(drcontext, ilist, where, reg2);
    /* Load data->buf_ptr into reg2 */
    opnd1 = opnd_create_reg(reg2);
    opnd2 = OPND_CREATE_MEMPTR(reg2, offsetof(per_thread_t, buf_ptr));
    instr = INSTR_CREATE_mov_ld(drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(ilist, where, instr);

    /* Move write/read to write field */
    opnd1 = OPND_CREATE_MEM32(reg2, offsetof(mem_ref_t, write));
    opnd2 = OPND_CREATE_INT32(write);
    instr = INSTR_CREATE_mov_imm(drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(ilist, where, instr);

    /* Store address in memory ref */
    opnd1 = OPND_CREATE_MEMPTR(reg2, offsetof(mem_ref_t, addr));
    opnd2 = opnd_create_reg(reg1);
    instr = INSTR_CREATE_mov_st(drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(ilist, where, instr);

    /* Increment reg value by pointer size using lea instr */
    opnd1 = opnd_create_reg(reg2);
    opnd2 = opnd_create_base_disp(reg2, DR_REG_NULL, 0, 
                                  sizeof(mem_ref_t), 
                                  OPSZ_lea);
    instr = INSTR_CREATE_lea(drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(ilist, where, instr);

    /* Update the data->buf_ptr */
    dr_insert_read_tls_field(drcontext, ilist, where, reg1);
    opnd1 = OPND_CREATE_MEMPTR(reg1, offsetof(per_thread_t, buf_ptr));
    opnd2 = opnd_create_reg(reg2);
    instr = INSTR_CREATE_mov_st(drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(ilist, where, instr);

    /* we use lea + jecxz trick for better performance
     * lea and jecxz won't disturb the eflags, so we won't insert
     * code to save and restore application's eflags. 
     */
    /* lea [reg2 - buf_end] => reg2 */
    opnd1 = opnd_create_reg(reg1);
    opnd2 = OPND_CREATE_MEMPTR(reg1, offsetof(per_thread_t, buf_end));
    instr = INSTR_CREATE_mov_ld(drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(ilist, where, instr);
    opnd1 = opnd_create_reg(reg2);
    opnd2 = opnd_create_base_disp(reg1, reg2, 1, 0, OPSZ_lea);
    instr = INSTR_CREATE_lea(drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(ilist, where, instr);
    
    /* jecxz call */
    call  = INSTR_CREATE_label(drcontext);
    opnd1 = opnd_create_instr(call);
    instr = INSTR_CREATE_jecxz(drcontext, opnd1);
    instrlist_meta_preinsert(ilist, where, instr);

    /* jump restore to skip clean call */
    restore = INSTR_CREATE_label(drcontext);
    opnd1 = opnd_create_instr(restore);
    instr = INSTR_CREATE_jmp(drcontext, opnd1);
    instrlist_meta_preinsert(ilist, where, instr);

    /* clean call */
    /* We jump to lean procedure which performs full context switch and 
     * clean call invocation. This is to reduce the code cache size. 
     */
    instrlist_meta_preinsert(ilist, where, call);
    /* mov restore DR_REG_XCX */
    opnd1 = opnd_create_reg(reg2);
    /* this is the return address for jumping back from lean procedure */
    opnd2 = opnd_create_instr(restore);
    instr = INSTR_CREATE_mov_st(drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(ilist, where, instr);
    /* jmp code_cache */
    opnd1 = opnd_create_pc(code_cache);
    instr = INSTR_CREATE_jmp(drcontext, opnd1);
    instrlist_meta_preinsert(ilist, where, instr);

    /* restore %reg */
    instrlist_meta_preinsert(ilist, where, restore);
    dr_restore_reg(drcontext, ilist, where, reg1, SPILL_SLOT_2);
    dr_restore_reg(drcontext, ilist, where, reg2, SPILL_SLOT_3);
}

