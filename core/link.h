/* **********************************************************
 * Copyright (c) 2012-2013 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * link.h - link exports
 */

#ifndef _CORE_LINK_H_ /* using different define than /usr/include/link.h */
#define _CORE_LINK_H_ 1


/* linkstub_t flags
 * WARNING: flags field is a ushort, so max flag is 0x8000!
 */
enum {
    /***************************************************/
    /* these first flags are also used on the instr_t data structure */

    /* Type of branch and thus which struct is used for this exit.
     * Due to a tight namespace (flags is a ushort field), we pack our
     * 3 types into these 2 bits, so the LINKSTUB_ macros are used to
     * distinguish, rather than raw bit tests:
     *
     *   name          LINK_DIRECT LINK_INDIRECT  struct
     *   ---------------  ---         ---         --------------------------
     *   (subset of fake)  0           0          linkstub_t          
     *   normal direct     1           0          direct_linkstub_t
     *   normal indirect   0           1          indirect_linkstub_t
     *   cbr fallthrough   1           1          cbr_fallthrough_linkstub_t
     *
     * Note that we can have fake linkstubs that should be treated as
     * direct or indirect, so LINK_FAKE is a separate flag.
     */
    LINK_DIRECT          = 0x0001,
    LINK_INDIRECT        = 0x0002,
    /* more specifics on type of branch
     * must check LINK_DIRECT vs LINK_INDIRECT for JMP and CALL.
     * absence of all of these is relied on as an indicator of shared_syscall
     * in indirect_linkstub_target(), so we can't get rid of LINK_RETURN
     * and use absence of CALL & JMP to indicate it.
     */
    LINK_RETURN          = 0x0004,
    /* JMP|CALL indicates JMP_PLT, so use EXIT_IS_{JMP,CALL} rather than these raw bits */
    LINK_CALL            = 0x0008,
    LINK_JMP             = 0x0010,

    /* Indicates a far cti which uses a separate ibl entry */
    LINK_FAR             = 0x0020,

#ifdef UNSUPPORTED_API
    LINK_TARGET_PREFIX   = 0x0040,
#endif
#ifdef X64
    /* PR 257963: since we don't store targets of ind branches, we need a flag
     * so we know whether this is a trace cmp exit, which has its own ibl entry
     */
    LINK_TRACE_CMP       = 0x0080,
#endif
    /* Flags that tell DR to take some action upon returning to dispatch.
     * This first one is multiplexed via .
     * All uses are assumed to be unlinkable.
     */
    LINK_SPECIAL_EXIT    = 0x0100,
#ifdef WINDOWS
    LINK_CALLBACK_RETURN = 0x0200,
#else
    /* PR 286922: we support both OP_sys{call,enter}- and OP_int-based system calls */
    LINK_NI_SYSCALL_INT  = 0x0200,
#endif
    /* indicates whether exit is before a non-ignorable syscall */
    LINK_NI_SYSCALL      = 0x0400,
    LINK_FINAL_INSTR_SHARED_FLAG = LINK_NI_SYSCALL,
    /* end of instr_t-shared flags  */
    /***************************************************/

    LINK_FRAG_OFFS_AT_END= 0x0800,

    LINK_END_OF_LIST     = 0x1000,

    LINK_FAKE            = 0x2000,

    LINK_LINKED          = 0x4000,

    LINK_SEPARATE_STUB   = 0x8000,

    /* WARNING: flags field is a ushort, so max flag is 0x8000! */
};

#ifdef UNIX
# define LINK_NI_SYSCALL_ALL (LINK_NI_SYSCALL | LINK_NI_SYSCALL_INT)
#else
# define LINK_NI_SYSCALL_ALL LINK_NI_SYSCALL
#endif

/* for easy printing */
#ifndef LINKCOUNT_64_BITS
#  define LINKCOUNT_FORMAT_STRING "%lu"
#else
#  define LINKCOUNT_FORMAT_STRING UINT64_FORMAT_STRING
#endif

/* linkstub_t heap layout is now quite variable.  linkstubs are laid out
 * after the fragment_t structure, which is itself variable.
 * The indirect_linkstub_t split was case 6468
 * The cbr_fallthrough_linkstub_t split was case 6528
 *
 *   fragment_t/trace_t
 *   array composed of three different sizes of linkstub_t subclasses:
 *     direct_linkstub_t
 *     cbr_fallthrough_linkstub_t
 *     indirect_linkstub_t
 *   post_linkstub_t
 *   
 * There are three types of specially-supported basic blocks that
 * have no post_linkstub_t:
 *   
 *   fragment_t
 *   indirect_linkstub_t
 *   
 *   fragment_t
 *   direct_linkstub_t
 *   direct_linkstub_t
 *
 *   fragment_t
 *   direct_linkstub_t
 *   cbr_fallthrough_linkstub_t
 */

/* To save memory, we have three types of linkstub structs: direct,
 * cbr_fallthrough, and indirect.  They each contain the flags field
 * at the same offset, so this struct can be used as a superclass.
 */
struct _linkstub_t {
    ushort         flags;          /* contains LINK_ flags above */

    /* We limit all fragment bodies to USHRT_MAX size
     * thus we can save space by making the branch ptr a ushort offset.
     * Do not directly access this field -- use EXIT_CTI_PC()
     */
    ushort         cti_offset;     /* offset from fragment start_pc of this cti */

#ifdef CUSTOM_EXIT_STUBS
    ushort         fixed_stub_offset;  /* offset in bytes of fixed part of exit stub from
                                          stub_pc, which points to custom prefix of stub */
#endif

#ifdef PROFILE_LINKCOUNT
    linkcount_type_t count;
#endif
};


/* linkage info common to all direct fragment exits */
typedef struct _common_direct_linkstub_t {
    linkstub_t       l;

    /* outgoing stubs of a fragment never change, and are allocated in
     * an array, but we walk them like a linked list since we don't want
     * to waste space storing the count and all of our access patterns
     * want to touch them all anyway.
     * use LINKSTUB_NEXT_EXIT() to access the next, and LINKSTUB_FINAL()
     * to test if the current guy is the final.
     *
     * Incoming stubs do change and we use this field to chain them:
     */
    linkstub_t       *next_incoming;

#ifdef TRACE_HEAD_CACHE_INCR
    /* for linking to trace head, we store the actual fragment_t target.
     * if the target is deleted the link will be unlinked, preventing
     * a stale fragment_t* pointer from sitting around
     */
    fragment_t       *target_fragment;
#endif
} common_direct_linkstub_t;


/* linkage info for each direct fragment exit */
typedef struct _direct_linkstub_t {
    common_direct_linkstub_t cdl;

    /* tag identifying the intended app target of the exit branch 
     * Do not directly access this field -- use EXIT_TARGET_TAG(), which
     * works for all linkstub types.
     */
    app_pc         target_tag;

    /* Must be absolute pc b/c we relocate some stubs away from fragment body.
     * Do not directly access this field -- use EXIT_STUB_PC(), which
     * works for all linkstub types.
     */
    cache_pc       stub_pc;
} direct_linkstub_t;


/* linkage info for cbr fallthrough exits that satisfy two conditions:
 *   1) separate stubs will not be individually freed -- we could
 *      have a fancy scheme that frees both at once, but we simply
 *      disallow the struct if any freeing will occur.
 *   2) the fallthrough target is within range of a signed short from the
 *      owning fragment's start pc (this is typically true even w/ eliding).
 *   3) the fallthrough exit immediately follows the cbr exit.
 * this direct stub split was case 6528.
 */
typedef struct _cbr_fallthrough_linkstub_t {
    /* We have no cti_offset as we assume this exit's cti immediately follows
     * the preceding cbr (see conditions above).
     *
     * Our target_tag uses the cti_offset field instead.  Since this struct is
     * only used for the 2nd (fallthrough) exit of a cbr whose target
     * is within range for a signed short from the owning fragment_t's tag,
     * we re-use the cti_offset field.
     *
     * This struct is also only used when its exit stub is adjacent to the
     * prior exit's, so we don't need to store stub_pc here.
     */
    common_direct_linkstub_t cdl;
} cbr_fallthrough_linkstub_t;


/* linkage info for each indirect fragment exit (slit for case 6468) */
typedef struct _indirect_linkstub_t {
    linkstub_t       l;
} indirect_linkstub_t;


/* Data shared among all linkstubs for a particular fragment_t.
 * Kept at the end of the array of linkstubs, and not present for
 * certain common types of fragment_t.
 */
typedef struct _post_linkstub_t {
    /* fragment_t + linkstub_t heap size cannot exceed the maximum fragment
     * body size since max sizeof linkstub_t-subclass (16) < min sizeof
     * exit stub (15) + corresponding cti (at least 2).  Thus we can
     * use a ushort here, paving the way for an additional field in
     * the future (though right now the other 2 bytes are wasted as
     * this struct is aligned to 4 bytes ).
     */
    ushort         fragment_offset;
    /* We force the compiler to maintain our 4-byte-alignment for heap allocations.
     * FIXME: should we support un-aligned in non-special heaps?
     * or could use special heap if we really wanted to save this 2 bytes.
     * also see case 6518 for a way to use this for Traces.
     */
    ushort         padding;
} post_linkstub_t;

/* For chaining together a list of inter-coarse-unit incoming stubs.  To
 * eliminate the need for wrappers for a series of fine-grained linkstubs, we
 * directly chain those -- so when walking, walk a fine entry's linkstubs
 * completely before going to the next coarse_incoming_t entry.
 */
typedef struct _coarse_incoming_t {
    union {
        cache_pc stub_pc;
        linkstub_t *fine_l;
    } in;
    bool coarse;
    struct _coarse_incoming_t *next;
} coarse_incoming_t;

/* this one does not take in flags b/c historically we used other fields */
#define LINKSTUB_FAKE(l) (TEST(LINK_FAKE, (l)->flags))
/* direct includes normal direct and cbr fallthrough */
#define LINKSTUB_DIRECT(flags) (TEST(LINK_DIRECT, (flags)))
#define LINKSTUB_NORMAL_DIRECT(flags) \
    (TEST(LINK_DIRECT, (flags)) && !TEST(LINK_INDIRECT, (flags)))
#define LINKSTUB_INDIRECT(flags) \
    (!TEST(LINK_DIRECT, (flags)) && TEST(LINK_INDIRECT, (flags)))
#define LINKSTUB_CBR_FALLTHROUGH(flags) \
    (TEST(LINK_DIRECT, (flags)) && TEST(LINK_INDIRECT, (flags)))

/* used with both LINK_* and INSTR_*_EXIT flags */
#define EXIT_IS_CALL(flags) (TEST(LINK_CALL, flags) && !TEST(LINK_JMP, flags))
#define EXIT_IS_JMP(flags) (TEST(LINK_JMP, flags) && !TEST(LINK_CALL, flags))
#define EXIT_IS_IND_JMP_PLT(flags) (TESTALL(LINK_JMP | LINK_CALL, flags))

#define LINKSTUB_FINAL(l)  (TEST(LINK_END_OF_LIST, (l)->flags))

/* We assume this combination of flags is unique for coarse proxies. */
#define LINKSTUB_COARSE_PROXY(flags) \
    (TESTALL(LINK_FAKE|LINK_DIRECT|LINK_SEPARATE_STUB, (flags)))

/* could optimize to avoid redundant tests */
#define LINKSTUB_SIZE(l) \
    (LINKSTUB_NORMAL_DIRECT((l)->flags) ? sizeof(direct_linkstub_t) : \
     (LINKSTUB_INDIRECT((l)->flags) ? sizeof(indirect_linkstub_t) : \
      (LINKSTUB_CBR_FALLTHROUGH((l)->flags) ? sizeof(cbr_fallthrough_linkstub_t) : \
       sizeof(linkstub_t))))

#define LINKSTUB_NEXT_EXIT(l) \
    ((LINKSTUB_FINAL((l))) ? NULL : \
        ((linkstub_t *) (((byte*)(l)) + LINKSTUB_SIZE(l))))

/* we pay the cost of the check in release builds to have the
 * safety return value of NULL
 */
#define LINKSTUB_NEXT_INCOMING(l) \
    (LINKSTUB_NORMAL_DIRECT((l)->flags) ? \
     (((direct_linkstub_t *)(l))->cdl.next_incoming) : \
     (LINKSTUB_CBR_FALLTHROUGH((l)->flags) ? \
      (((cbr_fallthrough_linkstub_t *)(l))->cdl.next_incoming) : \
      (ASSERT(false && "indirect linkstub has no next_incoming"), NULL)))

/* if sharing a stub then no offs, else offs to get to subsequent stub */
#define CBR_FALLTHROUGH_STUB_OFFS(f) \
    (INTERNAL_OPTION(cbr_single_stub) ? 0 : DIRECT_EXIT_STUB_SIZE((f)->flags))

#define EXIT_CTI_PC_HELPER(f, l) \
    (ASSERT(LINKSTUB_NORMAL_DIRECT((l)->flags)), \
     ((f)->start_pc + (l)->cti_offset))

#define EXIT_CTI_PC(f, l) \
    (LINKSTUB_CBR_FALLTHROUGH((l)->flags) ? \
     (cbr_fallthrough_exit_cti(EXIT_CTI_PC_HELPER(f, FRAGMENT_EXIT_STUBS(f)))) : \
     ((f)->start_pc + (l)->cti_offset))

#define EXIT_STUB_PC_HELPER(dc, f, l) \
    (ASSERT(LINKSTUB_NORMAL_DIRECT((l)->flags)), \
     (((direct_linkstub_t *)(l))->stub_pc))

#define EXIT_STUB_PC(dc, f, l) \
    (LINKSTUB_NORMAL_DIRECT((l)->flags) ? (((direct_linkstub_t *)(l))->stub_pc) : \
     (LINKSTUB_CBR_FALLTHROUGH((l)->flags) ? \
      (EXIT_STUB_PC_HELPER(dc, f, FRAGMENT_EXIT_STUBS(f)) \
       + CBR_FALLTHROUGH_STUB_OFFS(f)) :                  \
       indirect_linkstub_stub_pc(dc, f, l)))

#define EXIT_TARGET_TAG(dc, f, l) \
    (LINKSTUB_NORMAL_DIRECT((l)->flags) ? (((direct_linkstub_t *)(l))->target_tag) : \
     (LINKSTUB_CBR_FALLTHROUGH((l)->flags) ? \
      ((f)->tag + ((short)(l)->cti_offset)) : \
       indirect_linkstub_target(dc, f, l)))

#ifdef CUSTOM_EXIT_STUBS
# define EXIT_FIXED_STUB_PC(dc, f, l) (EXIT_STUB_PC(dc, f, l) + (l)->fixed_stub_offset)
#endif

#ifdef WINDOWS
# define EXIT_TARGETS_SHARED_SYSCALL(flags) \
    (DYNAMO_OPTION(shared_syscalls) &&     \
     !TESTANY(LINK_RETURN|LINK_CALL|LINK_JMP, (flags)))
#else
# define EXIT_TARGETS_SHARED_SYSCALL(flags) (false)
#endif

/* indirect exits w/o inlining have no stub at all for -no_indirect_stubs */
#define EXIT_HAS_STUB(l_flags, f_flags)                                       \
    (DYNAMO_OPTION(indirect_stubs) || !LINKSTUB_INDIRECT((l_flags)) ||      \
     (!EXIT_TARGETS_SHARED_SYSCALL(l_flags) &&                                \
      ((DYNAMO_OPTION(inline_trace_ibl) && TEST(FRAG_IS_TRACE, (f_flags))) || \
       (DYNAMO_OPTION(inline_bb_ibl) && !TEST(FRAG_IS_TRACE, (f_flags))))))

/* two cases with no local stub: a separate stub or no stub at all */
#define EXIT_HAS_LOCAL_STUB(l_flags, f_flags) \
    (EXIT_HAS_STUB(l_flags, f_flags) && !TEST(LINK_SEPARATE_STUB, (l_flags)))

void link_init(void);
void link_exit(void);
void link_reset_init(void);
void link_reset_free(void);
void link_thread_init(dcontext_t *dcontext);
void link_thread_exit(dcontext_t *dcontext);

/* coarse-grain support */

cache_pc
coarse_stubs_create(coarse_info_t *info, cache_pc pc, size_t size);

void
coarse_stubs_delete(coarse_info_t *info);

uint
coarse_stub_alignment(coarse_info_t *info);

void
coarse_mark_trace_head(dcontext_t *dcontext, fragment_t *f,
                       coarse_info_t *info, cache_pc stub, cache_pc body);

void
coarse_unit_unlink(dcontext_t *dcontext, coarse_info_t *info);

void
coarse_unit_unlink_outgoing(dcontext_t *dcontext, coarse_info_t *info);

#ifdef DEBUG
bool
coarse_unit_outgoing_linked(dcontext_t *dcontext, coarse_info_t *info);
#endif

cache_pc
coarse_stub_lookup_by_target(dcontext_t *dcontext, coarse_info_t *info,
                             cache_pc target_tag);

void
coarse_lazy_link(dcontext_t *dcontext, fragment_t *targetf);

cache_pc
fcache_return_coarse_prefix(cache_pc stub_pc, coarse_info_t *info /*OPTIONAL*/);

cache_pc
trace_head_return_coarse_prefix(cache_pc stub_pc, coarse_info_t *info /*OPTIONAL*/);

cache_pc
get_coarse_ibl_prefix(dcontext_t *dcontext, cache_pc stub_pc,
                      ibl_branch_type_t branch_type);

bool
in_coarse_stubs(cache_pc pc);

bool
in_coarse_stub_prefixes(cache_pc pc);

cache_pc
coarse_deref_ibl_prefix(dcontext_t *dcontext, cache_pc target);

coarse_info_t *
get_stub_coarse_info(cache_pc pc);

/* Initializes an array of linkstubs beginning with first */
void linkstubs_init(linkstub_t *first, int num_direct, int num_indirect, fragment_t *f);

bool is_linkable(dcontext_t *dcontext, fragment_t *from_f, linkstub_t *l,
                 fragment_t *targetf, bool have_link_lock, bool mark_new_trace_head);

uint linkstub_size(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);

uint linkstub_propagatable_flags(uint flags);

bool linkstub_frag_offs_at_end(uint flags, int direct_exits, int indirect_exits);

bool use_cbr_fallthrough_short(uint flags, int direct_exits, int indirect_exits);

uint linkstubs_heap_size(uint flags, int direct_exits, int indirect_exits);

fragment_t *linkstub_fragment(dcontext_t *dcontext, linkstub_t *l);

#ifdef X64
/* Converts the canonical empty fragment to an empty fragment marked FRAG_32_BIT */
fragment_t *empty_fragment_mark_x86(fragment_t *f);
#endif

#ifdef DEBUG
bool linkstub_owned_by_fragment(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);
#endif

void set_last_exit(dcontext_t *dcontext, linkstub_t *l);
void last_exit_deleted(dcontext_t *dcontext);
void set_coarse_ibl_exit(dcontext_t *dcontext);
int get_last_linkstub_ordinal(dcontext_t *dcontext);

linkstub_t * get_deleted_linkstub(dcontext_t *dcontext);
const linkstub_t * get_starting_linkstub(void);
const linkstub_t * get_reset_linkstub(void);
const linkstub_t * get_syscall_linkstub(void);
const linkstub_t * get_selfmod_linkstub(void);
const linkstub_t * get_ibl_deleted_linkstub(void);
#ifdef UNIX
const linkstub_t * get_sigreturn_linkstub(void);
#else /* WINDOWS */
const linkstub_t * get_asynch_linkstub(void);
#endif
const linkstub_t * get_native_exec_linkstub(void);
const linkstub_t * get_native_exec_syscall_linkstub(void);
#ifdef HOT_PATCHING_INTERFACE
const linkstub_t * get_hot_patch_linkstub(void);
#endif
#ifdef CLIENT_INTERFACE
const linkstub_t * get_client_linkstub(void);
bool is_client_ibl_linkstub(const linkstub_t *l);
const linkstub_t * get_client_ibl_linkstub(uint link_flags, uint frag_flags);
#endif
const linkstub_t * get_ibl_sourceless_linkstub(uint link_flags, uint frag_flags);
bool is_ibl_sourceless_linkstub(const linkstub_t *l);
const linkstub_t * get_coarse_exit_linkstub(void);
const linkstub_t * get_coarse_trace_head_exit_linkstub(void);

# define IS_COARSE_LINKSTUB(l) ((l) == get_coarse_exit_linkstub() || \
    (l) == get_coarse_trace_head_exit_linkstub())

#ifdef WINDOWS
const linkstub_t * get_shared_syscalls_unlinked_linkstub(void);
# define IS_SHARED_SYSCALLS_UNLINKED_LINKSTUB(l)       \
    ((l) == get_shared_syscalls_unlinked_linkstub())

const linkstub_t * get_shared_syscalls_trace_linkstub(void);
const linkstub_t * get_shared_syscalls_bb_linkstub(void);
# define IS_SHARED_SYSCALLS_LINKSTUB(l)               \
      ((l) == get_shared_syscalls_trace_linkstub() || \
       (l) == get_shared_syscalls_bb_linkstub() ||    \
       (l) == get_shared_syscalls_unlinked_linkstub())
# define IS_SHARED_SYSCALLS_TRACE_LINKSTUB(l)         \
      ((l) == get_shared_syscalls_trace_linkstub())
#else
# define IS_SHARED_SYSCALLS_UNLINKED_LINKSTUB(l) false
# define IS_SHARED_SYSCALLS_LINKSTUB(l) false
# define IS_SHARED_SYSCALLS_TRACE_LINKSTUB(l) false
#endif

bool should_separate_stub(dcontext_t *dcontext, app_pc target, uint fragment_flags);
int local_exit_stub_size(dcontext_t *dcontext, app_pc target, uint fragment_flags);
void separate_stub_create(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);
void linkstub_free_exitstubs(dcontext_t *dcontext, fragment_t *f);

void linkstubs_shift(dcontext_t *dcontext, fragment_t *f, ssize_t shift);

void link_new_fragment(dcontext_t *dcontext, fragment_t *f);
void link_fragment_outgoing(dcontext_t *dcontext, fragment_t *f, bool new_fragment);
void unlink_fragment_outgoing(dcontext_t *dcontext, fragment_t *f);
void link_fragment_incoming(dcontext_t *dcontext, fragment_t *f, bool new_fragment);
void unlink_fragment_incoming(dcontext_t *dcontext, fragment_t *f);

void shift_links_to_new_fragment(dcontext_t *dcontext, fragment_t *old_f,
                                 fragment_t *new_f);
future_fragment_t * incoming_remove_fragment(dcontext_t *dcontext, fragment_t *f);

/* if this linkstub shares the stub with the next linkstub, returns the
 * next linkstub; else returns NULL
 */
linkstub_t *
linkstub_shares_next_stub(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);

/* Returns the total size needed for stubs if info is frozen. */
size_t
coarse_frozen_stub_size(dcontext_t *dcontext, coarse_info_t *info,
                        uint *num_fragments, uint *num_stubs);

/* Removes any incoming data recording the outgoing link from stub */
void
coarse_remove_outgoing(dcontext_t *dcontext, cache_pc stub, coarse_info_t *src_info);

void
coarse_update_outgoing(dcontext_t *dcontext, cache_pc old_stub, cache_pc new_stub,
                       coarse_info_t *src_info, bool replace);

void
coarse_unit_shift_links(dcontext_t *dcontext, coarse_info_t *info);

/* Updates the info pointers embedded in the coarse_stub_areas vector */
void
coarse_stubs_set_info(coarse_info_t *info);

/* Sets the final used pc in a frozen stub region */
void
coarse_stubs_set_end_pc(coarse_info_t *info, byte *end_pc);

#endif /* _CORE_LINK_H_ */
