#ifndef MODULE_H
#define MODULE_H

/* used only in our own routines here which use PF_* converted to MEMPROT_* */
#define OS_IMAGE_READ (MEMPROT_READ)
#define OS_IMAGE_WRITE (MEMPROT_WRITE)
#define OS_IMAGE_EXECUTE (MEMPROT_EXEC)

/* i#160/PR 562667: support non-contiguous library mappings.  While we're at
 * it we go ahead and store info on each segment whether contiguous or not.
 */
typedef struct _module_segment_t {
    /* start and end are page-aligned beyond the section alignment */
    app_pc start;
    app_pc end;
    uint prot;
    bool shared; /* not unique to this module */
    uint64 offset;
} module_segment_t;

typedef struct _os_module_data_t {
    /* To compute the base address, one determines the memory address associated with
     * the lowest p_vaddr value for a PT_LOAD segment. One then obtains the base
     * address by truncating the memory load address to the nearest multiple of the
     * maximum page size and subtracting the truncated lowest p_vaddr value.
     * Thus, this is not the load address but the base address used in
     * address references within the file.
     */
    app_pc base_address;
    size_t alignment; /* the alignment between segments */

    /* Fields for pcaches (PR 295534) */
    size_t checksum;
    size_t timestamp;

    /* i#112: Dynamic section info for exported symbol lookup.  Not
     * using elf types here to avoid having to export those.
     */
    bool hash_is_gnu;     /* gnu hash function? */
    app_pc hashtab;       /* absolute addr of .hash or .gnu.hash */
    size_t num_buckets;   /* number of bucket entries */
    app_pc buckets;       /* absolute addr of hash bucket table */
    size_t num_chain;     /* number of chain entries */
    app_pc chain;         /* absolute addr of hash chain table */
    app_pc dynsym;        /* absolute addr of .dynsym */
    app_pc dynstr;        /* absolute addr of .dynstr */
    size_t dynstr_size;   /* size of .dynstr */
    size_t symentry_size; /* size of a .dynsym entry */
    /* for .gnu.hash */
    app_pc gnu_bitmask;
    ptr_uint_t gnu_shift;
    ptr_uint_t gnu_bitidx;

    /* i#160/PR 562667: support non-contiguous library mappings */
    bool contiguous;
    uint num_segments;   /* number of valid entries in segments array */
    uint alloc_segments; /* capacity of segments array */
    module_segment_t *segments;
} os_module_data_t;

#endif /* MODULE_H */
