#include "globals.h"
#include "annotations.h"
#include "lib/dr_annotations.h"
#include "jit_opt.h"

#define DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_manage_code_area"

#define DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_unmanage_code_area"

#ifdef ANNOTATIONS
static void
annotation_manage_code_area(void *start, size_t size)
{
    LOG(GLOBAL, LOG_ANNOTATIONS, 2,
        "Add code area "PFX"-"PFX" to JIT managed regions\n",
        start, (app_pc) start + size);

    set_region_jit_managed(start, size);
}

static void
annotation_unmanage_code_area(void *start, size_t size)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (!is_jit_managed_area(start))
        return;

    LOG(GLOBAL, LOG_ANNOTATIONS, 2,
        "Remove code area "PFX"-"PFX" from JIT managed regions\n",
        start, (app_pc) start + size);

    mutex_lock(&thread_initexit_lock);
    flush_fragments_and_remove_region(dcontext, start, size,
                                      true/*own initexit_lock*/, false/*keep futures*/);
    mutex_unlock(&thread_initexit_lock);

    jitopt_clear_span(start, (app_pc) start + size);
}
#endif

/***************************************************************************
 * Fragment Tree
 */

/* list of traces that contain the associated bb_node_t (via its "traces" field) */
typedef struct _trace_list_t {
    app_pc trace_tag;
    struct _trace_list_t *next;
} trace_list_t;

/* tree node representing one JIT basic block */
typedef struct _bb_node_t {
    app_pc start; /* fragment start (in app space) */
    app_pc end;   /* fragment end (in app space) */
    app_pc max;   /* max fragment end in this subtree (in app space) */
    bool red;
    struct _bb_node_t *left;
    struct _bb_node_t *right;
    struct _bb_node_t *parent;
    trace_list_t *traces; /* list of traces containing this bb */
} bb_node_t;

/* red-black tree of JIT basic blocks */
typedef struct _fragment_tree_t {
    bb_node_t *root;
    bb_node_t *nil;     /* N.B.: nil's parent is used temporarily during delete() */
    void *special_heap; /* for quick allocation of nodes */
} fragment_tree_t;

static fragment_tree_t *
fragment_tree_create()
{
    fragment_tree_t *tree = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fragment_tree_t,
                                            ACCT_OTHER, UNPROTECTED);
    memset(tree, 0, sizeof(fragment_tree_t));
    tree->special_heap = special_heap_init(sizeof(bb_node_t), true /* lock */,
                                           false /* -x */, true /* persistent */);
    tree->nil = special_heap_alloc(tree->special_heap);
    memset(tree->nil, 0, sizeof(bb_node_t));
    tree->root = tree->nil;
    return tree;
}

static void
fragment_tree_destroy(fragment_tree_t *tree)
{
    special_heap_free(tree->special_heap, tree->nil);
    special_heap_exit(tree->special_heap);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, tree, fragment_tree_t, ACCT_OTHER, UNPROTECTED);
}

static bb_node_t *
fragment_tree_overlap_lookup(fragment_tree_t *tree, app_pc start, app_pc end)
{
    /* i#1114 NYI: return the first overlapping fragment found in an ordered descent */
    return NULL;
}

#if (0)
static bb_node_t *
fragment_tree_lookup(fragment_tree_t *tree, app_pc start, app_pc end)
{
    /* i#1114 NYI: return the fragment with bounds exactly matching start and end */
    return NULL;
}
#endif

static bb_node_t *
fragment_tree_insert(fragment_tree_t *tree, app_pc start, app_pc end)
{
    /* i#1114 NYI: insert a new node with the specified start and end and return it */
    return NULL;
}

static void
fragment_tree_delete(fragment_tree_t *tree, bb_node_t *node)
{
    /* i#1114 NYI: delete node from the tree (may crash if node is not in the tree) */
}

#if (0)
static void
fragment_tree_clear(fragment_tree_t *tree)
{
    /* i#1114 NYI: clear all nodes from the tree */
}
#endif

/***************************************************************************
 * Cache Consistency
 */

static fragment_tree_t *fragment_tree;

void
jitopt_init()
{
    if (DYNAMO_OPTION(opt_jit)) {
        fragment_tree = fragment_tree_create();

#ifdef ANNOTATIONS
        dr_annotation_register_call(DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME,
                                    (void *) annotation_manage_code_area, false, 2,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);
        dr_annotation_register_call(DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME,
                                    (void *) annotation_unmanage_code_area, false, 2,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);
#endif /* ANNOTATIONS */
    }
}

void
jitopt_exit()
{
    if (DYNAMO_OPTION(opt_jit))
        fragment_tree_destroy(fragment_tree);
}

void
jitopt_add_dgc_bb(app_pc start, app_pc end, bool is_trace_head)
{
    ASSERT(DYNAMO_OPTION(opt_jit));
    fragment_tree_insert(fragment_tree, start, end);
}

void
jitopt_clear_span(app_pc start, app_pc end)
{
    bb_node_t *overlap;

    ASSERT(DYNAMO_OPTION(opt_jit));

    do {
        /* XXX i#1114: maybe more efficient to delete deepest overlapping node first */
        overlap = fragment_tree_overlap_lookup(fragment_tree, start, end);
        if (overlap == fragment_tree->nil)
            break;
        else
            fragment_tree_delete(fragment_tree, overlap);
    } while (true);
}

#ifdef STANDALONE_UNIT_TEST
/***************************************************************************
 * Fragment Tree Unit Test
 */

void
unit_test_jit_fragment_tree()
{
    /* i#1114 NYI: fragment tree unit tests */
}

#endif /* STANDALONE_UNIT_TEST */
