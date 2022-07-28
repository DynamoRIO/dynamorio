#include "globals.h"
#include "annotations.h"
#include "lib/dr_annotations.h"
#include "jit_opt.h"

#define DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME "dynamorio_annotate_manage_code_area"

#define DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME "dynamorio_annotate_unmanage_code_area"

#ifdef ANNOTATIONS
static void
annotation_manage_code_area(void *start, size_t size)
{
    LOG(GLOBAL, LOG_ANNOTATIONS, 2,
        "Add code area " PFX "-" PFX " to JIT managed regions\n", start,
        (app_pc)start + size);

    set_region_jit_managed(start, size);
}

static void
annotation_unmanage_code_area(void *start, size_t size)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (!is_jit_managed_area(start))
        return;

    LOG(GLOBAL, LOG_ANNOTATIONS, 2,
        "Remove code area " PFX "-" PFX " from JIT managed regions\n", start,
        (app_pc)start + size);

    d_r_mutex_lock(&thread_initexit_lock);
    flush_fragments_and_remove_region(dcontext, start, size, true /*own initexit_lock*/,
                                      false /*keep futures*/);
    d_r_mutex_unlock(&thread_initexit_lock);

    jitopt_clear_span(start, (app_pc)start + size);
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
    bb_node_t *nil;
    void *node_heap;  /* for quick allocation of nodes */
    void *trace_heap; /* for quick allocation of trace list items */
} fragment_tree_t;

static fragment_tree_t *
fragment_tree_create()
{
    fragment_tree_t *tree =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, fragment_tree_t, ACCT_OTHER, UNPROTECTED);
    memset(tree, 0, sizeof(fragment_tree_t));
    tree->node_heap = special_heap_init(sizeof(bb_node_t), true /* lock */,
                                        false /* -x */, true /* persistent */);
    tree->trace_heap = special_heap_init(sizeof(bb_node_t), true /* lock */,
                                         false /* -x */, true /* persistent */);
    tree->nil = special_heap_alloc(tree->node_heap);
    memset(tree->nil, 0, sizeof(bb_node_t));
    tree->root = tree->nil;
    return tree;
}

static void
fragment_tree_destroy(fragment_tree_t *tree)
{
    special_heap_free(tree->node_heap, tree->nil);
    special_heap_exit(tree->node_heap);
    special_heap_exit(tree->trace_heap);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, tree, fragment_tree_t, ACCT_OTHER, UNPROTECTED);
}

/* Return the first node in the tree overlapping the span. */
static bb_node_t *
fragment_tree_overlap_lookup(fragment_tree_t *tree, app_pc start, app_pc end)
{
    bb_node_t *walk = tree->root;

    ASSERT(start < end);

    while (walk != tree->nil) {
        if (start < walk->end && end > walk->start)
            return walk;
        if (start < walk->left->max)
            walk = walk->left;
        else
            walk = walk->right;
    }
    return tree->nil;
}

#ifdef STANDALONE_UNIT_TEST
/* Lookup a node in the tree by exact match. For testing only. */
static bb_node_t *
fragment_tree_lookup(fragment_tree_t *tree, app_pc start, app_pc end)
{
    bb_node_t *walk = tree->root;

    ASSERT(start < end);

    while (walk != tree->nil) {
        if (start < walk->start || (start == walk->start && end < walk->end))
            walk = walk->left;
        else if (start == walk->start && end == walk->end)
            return walk;
        else
            walk = walk->right;
    }
    return NULL;
}
#endif /* STANDALONE_UNIT_TEST */

/* Locally update the maximum end pc for the subtree defined by node (i.e., node->max),
 * assuming that the maximum of node's two children (including nil) are currently correct.
 */
static inline void
fragment_tree_update_node_max(fragment_tree_t *tree, bb_node_t *node)
{
    if (node != tree->nil)
        node->max = MAX(MAX(node->left->max, node->right->max), node->end);
}

/* Rotate the tree left around a node. */
static void
fragment_tree_rotate_left(fragment_tree_t *tree, bb_node_t *node)
{
    bb_node_t *pivot = node->right;

    node->right = pivot->left;    /* Remove the pivot from below the node;   */
    if (node->right != tree->nil) /* the pivot's child becomes node's child. */
        node->right->parent = node;

    pivot->parent = node->parent; /* Insert the pivot above the node;              */
    if (node == tree->root)       /* the node's parent becomes the pivot's parent. */
        tree->root = pivot;
    else if (node == node->parent->left)
        node->parent->left = pivot;
    else
        node->parent->right = pivot;
    pivot->left = node;
    node->parent = pivot;

    fragment_tree_update_node_max(tree, node);
    fragment_tree_update_node_max(tree, pivot);
}

/* Rotate the tree right around a node. */
static void
fragment_tree_rotate_right(fragment_tree_t *tree, bb_node_t *node)
{
    bb_node_t *pivot = node->left;

    node->left = pivot->right;   /* Remove the pivot from below the node;   */
    if (node->left != tree->nil) /* the pivot's child becomes node's child. */
        node->left->parent = node;

    pivot->parent = node->parent; /* Insert the pivot above the node;              */
    if (node == tree->root)       /* the node's parent becomes the pivot's parent. */
        tree->root = pivot;
    else if (node == node->parent->left)
        node->parent->left = pivot;
    else
        node->parent->right = pivot;
    pivot->right = node;
    node->parent = pivot;

    fragment_tree_update_node_max(tree, node);
    fragment_tree_update_node_max(tree, pivot);
}

/* Insert new_node without rebalancing. */
static inline void
fragment_tree_insert_unbalanced(fragment_tree_t *tree, bb_node_t *new_node)
{
    bb_node_t *walk = tree->root;

    if (tree->root == tree->nil) {
        tree->root = new_node;
    } else {
        while (true) {
            if (walk->max < new_node->end)
                walk->max = new_node->end;
            if (new_node->start < walk->start ||
                (new_node->start == walk->start && new_node->end < walk->end)) {
                if (walk->left == tree->nil) {
                    walk->left = new_node;
                    break;
                }
                walk = walk->left;
            } else {
                ASSERT(!(new_node->start == walk->start && new_node->end == walk->end));
                if (walk->right == tree->nil) {
                    walk->right = new_node;
                    break;
                }
                walk = walk->right;
            }
        }
    }
    new_node->parent = walk;
}

/* Rebalance the tree after the insertion of new_node. */
static inline void
fragment_tree_insert_rebalance(fragment_tree_t *tree, bb_node_t *new_node)
{
    bb_node_t *walk = new_node, *uncle;

    /* The new node is red, so it may be necessary to resolve consecutive red nodes. First
     * try to borrow adjacent blacks from higher up in new_node's path: walk up the tree
     * and recolor every other uncle black, recoloring its parent red instead. If a black
     * uncle is found, resolve locally by rotating the tree above and below that uncle.
     */

    while (walk->parent->red) {
        if (walk->parent == walk->parent->parent->left) {
            uncle = walk->parent->parent->right;
            if (uncle->red) { /* Easy case: recolor uncle and grandpa, ascend 2 levels */
                walk->parent->red = false;
                uncle->red = false;
                walk->parent->parent->red = true;
                walk = walk->parent->parent;
            } else {
                /* Walk's uncle is black, so recoloring is interrupted. Move parent's
                 * red out of walk's path by turning walk's grandparent red and
                 * rotating the grandparent down into the uncle's path.
                 */
                if (walk == walk->parent->right) {
                    /* But wait--walk is about to get pulled into uncle's path along
                     * with parent's red, which defeats the purpose. Adjust locally
                     * by rotating walk to the other side of parent.
                     */

                    ASSERT(walk->red); /* should preserve the local red-black scenario */

                    /* because it becomes walk->parent->right, and walk->parent is red */
                    ASSERT(!walk->left->red);

                    walk = walk->parent;
                    fragment_tree_rotate_left(tree, walk);
                }

                /* because these will become children of walk's now-red grandparent */
                ASSERT(!(walk->parent->right->red || walk->parent->parent->right->red));

                walk->parent->red = false; /* recolor and rotate grandparent right */
                walk->parent->parent->red = true;
                fragment_tree_rotate_right(tree, walk->parent->parent);
            }
        } else { /* mirror of above */
            uncle = walk->parent->parent->left;
            if (uncle->red) {
                walk->parent->red = false;
                uncle->red = false;
                walk->parent->parent->red = true;
                walk = walk->parent->parent;
            } else {
                if (walk == walk->parent->left) {
                    ASSERT(walk->red && !walk->right->red);
                    walk = walk->parent;
                    fragment_tree_rotate_right(tree, walk);
                }
                ASSERT(!(walk->parent->left->red || walk->parent->parent->left->red));
                walk->parent->red = false;
                walk->parent->parent->red = true;
                fragment_tree_rotate_left(tree, walk->parent->parent);
            }
        }
    }
    tree->root->red = false;
}

/* Create a node with the specified span. */
static bb_node_t *
fragment_tree_node_create(fragment_tree_t *tree, app_pc start, app_pc end)
{
    bb_node_t *new_node = special_heap_alloc(tree->node_heap);

    ASSERT(start < end);

    memset(new_node, 0, sizeof(bb_node_t));
    new_node->left = new_node->right = tree->nil;
    new_node->red = true;
    new_node->start = start;
    new_node->end = new_node->max = end;
    return new_node;
}

/* Destroy the node and its list of containing trace tags. */
static void
fragment_tree_node_destroy(fragment_tree_t *tree, bb_node_t *node)
{
    trace_list_t *walk = node->traces, *next;

    while (walk != NULL) {
        next = walk->next;
        special_heap_free(tree->trace_heap, walk);
        walk = next;
    }
    special_heap_free(tree->node_heap, node);
}

/* Create a node with the specified span and insert it, rebalancing as necessary. */
static bb_node_t *
fragment_tree_insert(fragment_tree_t *tree, app_pc start, app_pc end)
{
    bb_node_t *new_node = fragment_tree_node_create(tree, start, end);

    fragment_tree_insert_unbalanced(tree, new_node);
    fragment_tree_insert_rebalance(tree, new_node);
    return new_node;
}

/* Return the maximum node in the specified subtree. */
static inline bb_node_t *
fragment_subtree_max(fragment_tree_t *tree, bb_node_t *node)
{
    ASSERT(node != tree->nil);

    while (node->right != tree->nil) {
        node = node->right;
    }
    return node;
}

static void
fragment_tree_transplant(fragment_tree_t *tree, bb_node_t *eviction,
                         bb_node_t *transplant)
{
    if (eviction->parent == tree->nil)
        tree->root = transplant;
    else if (eviction == eviction->parent->left)
        eviction->parent->left = transplant;
    else
        eviction->parent->right = transplant;
    transplant->parent = eviction->parent;
}

/* Rebalance the tree after the deletion of the specified node. */
static void
fragment_tree_delete_rebalance(fragment_tree_t *tree, bb_node_t *node)
{
    bool is_left, sibling_trio_has_red;
    bb_node_t *sibling;

    do { /* goal: pull an additional black into the simple path from root to node */
        is_left = node == node->parent->left;
        sibling = (is_left ? node->parent->right : node->parent->left);

        if (sibling->red) {
            node->parent->red = true; /* sibling can be the additional black, */
            sibling->red = false;     /* so recolor and pull it into the path */
            if (is_left) {
                sibling = sibling->left; /* next line changes node's sibling */
                fragment_tree_rotate_left(tree, node->parent);
            } else {
                sibling = sibling->right; /* next line changes node's sibling */
                fragment_tree_rotate_right(tree, node->parent);
            }
        }

        sibling_trio_has_red = sibling->red || sibling->left->red || sibling->right->red;
        if (node->parent->red || sibling_trio_has_red)
            break; /* can rebalance locally at this point */

        sibling->red = true;
        if (node->parent->parent == tree->nil)
            return; /* done because root's children are now red */
        node = node->parent;
    } while (true);

    /* node is still missing a black, but can gain one locally now */

    if (node->parent->red && !sibling_trio_has_red) {
        sibling->red = true;       /* sibling can take parent's red, such that */
        node->parent->red = false; /* parent becomes node's additional black   */
    } else {
        ASSERT(!sibling->red);

        /* sibling is black, so swap with parent, then pull parent into node's path */

        /* but wait--sibling's path will lose a black if its
         * opposite child (node's nephew) is also black, so...
         */
        if (is_left) {
            if (sibling->left->red && !sibling->right->red) {
                sibling->red = true;        /* ...make sibling's opposite child red  */
                sibling->left->red = false; /* w/o affecting black depth on any path */
                fragment_tree_rotate_right(tree, sibling);
                sibling = sibling->parent;
            }
        } else if (sibling->right->red && !sibling->left->red) {
            sibling->red = true;         /* ...make sibling's opposite child red  */
            sibling->right->red = false; /* w/o affecting black depth on any path */
            fragment_tree_rotate_left(tree, sibling);
            sibling = sibling->parent;
        }

        /* sibling's opposite child (node's nephew) is now certainly red */
        ASSERT((is_left && sibling->right->red) || (!is_left && sibling->left->red));

        sibling->red = node->parent->red;
        node->parent->red = false; /* add black to node's path but not sibling's path: */
        if (is_left) {             /* make parent black and rotate it into node's path */
            ASSERT(sibling->right->red);
            sibling->right->red = false;
            fragment_tree_rotate_left(tree, node->parent);
        } else {
            ASSERT(sibling->left->red);
            sibling->left->red = false;
            fragment_tree_rotate_right(tree, node->parent);
        }
    }
}

/* Remove the specified node from the tree and destroy it. */
static inline void
fragment_tree_delete(fragment_tree_t *tree, bb_node_t *node)
{
    bb_node_t *transplant, *deletion = node, *walk;

    if (node->left != tree->nil && node->right != tree->nil) {
        bb_node_t *predecessor = fragment_subtree_max(tree, node->left);
        trace_list_t *trace_swap = node->traces;
        node->start = predecessor->start;
        node->end = predecessor->end;
        node->traces = predecessor->traces;
        predecessor->traces = trace_swap;
        deletion = predecessor;
    }

    ASSERT(deletion->left == tree->nil || deletion->right == tree->nil);

    deletion->max = 0;
    walk = deletion->parent;
    while (walk != tree->nil) {
        fragment_tree_update_node_max(tree, walk);
        walk = walk->parent;
    }

    transplant = deletion->right == tree->nil ? deletion->left : deletion->right;
    if (!deletion->red) { /* losing a black node, so rebalance */
        deletion->red = transplant->red;
        if (deletion->parent != tree->nil)
            fragment_tree_delete_rebalance(tree, deletion);
    }
    fragment_tree_transplant(tree, deletion, transplant);
    if (deletion->parent == tree->nil && transplant != tree->nil) {
        transplant->red = false; /* transplanted into the root, so set it black */
    } else {
        walk = transplant->parent;
        while (walk != tree->nil) {
            fragment_tree_update_node_max(tree, walk);
            walk = walk->parent;
        }
    }

    if (deletion != node) {
        /* The deletion was swapped for its predecessor above--but the caller expects
         * node to be destroyed, not its predecessor. So swap again: copy predecessor's
         * data back into predecessor's original memory and re-link its tree limbs.
         */
        bb_node_t *restore = deletion;
        trace_list_t *trace_swap = restore->traces;

        memcpy(restore, node, sizeof(bb_node_t));
        if (node == tree->root)
            tree->root = restore;
        else if (node == restore->parent->left)
            node->parent->left = restore;
        else
            node->parent->right = restore;
        if (restore->right != tree->nil)
            restore->right->parent = restore;
        if (restore->left != tree->nil)
            restore->left->parent = restore;
        node->traces = trace_swap;
    }
    fragment_tree_node_destroy(tree, node);
}

#ifdef STANDALONE_UNIT_TEST
/* Remove and destroy all nodes from the tree. */
static void
fragment_tree_clear(fragment_tree_t *tree)
{
    if (tree->root != tree->nil) {
        bool is_left = false;
        bb_node_t *walk = tree->root;

        do {
            if (walk->left != tree->nil) {
                walk = walk->left;
                is_left = true;
            } else if (walk->right != tree->nil) {
                walk = walk->right;
                is_left = false;
            } else {
                walk = walk->parent;
                if (walk == tree->nil) {
                    tree->root = tree->nil;
                    break;
                } else if (is_left) {
                    fragment_tree_node_destroy(tree, walk->left);
                    walk->left = tree->nil;
                } else {
                    fragment_tree_node_destroy(tree, walk->right);
                    walk->right = tree->nil;
                }
                is_left = walk == walk->parent->left;
            }
        } while (true);
    }

    ASSERT(tree->root == tree->nil);
}
#endif /* STANDALONE_UNIT_TEST */

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
                                    (void *)annotation_manage_code_area, false, 2,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);
        dr_annotation_register_call(DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME,
                                    (void *)annotation_unmanage_code_area, false, 2,
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

uint
jitopt_clear_span(app_pc start, app_pc end)
{
    bb_node_t *overlap;
    uint removal_count = 0;

    ASSERT(DYNAMO_OPTION(opt_jit));

    do {
        /* XXX i#1114: maybe more efficient to delete deepest overlapping node first */
        overlap = fragment_tree_overlap_lookup(fragment_tree, start, end);
        if (overlap == fragment_tree->nil)
            break;
        fragment_tree_delete(fragment_tree, overlap);
        removal_count++;
    } while (true);

    return removal_count;
}

#ifdef STANDALONE_UNIT_TEST
/***************************************************************************
 * Fragment Tree Unit Test
 */

#    define FRAGMENT_TREE_TEST_NODE_COUNT 0x900

static void
unit_test_find_node_in_list(bb_node_t **node_list, bb_node_t *lookup, uint list_length)
{
    uint i;

    for (i = 0; i < list_length; i++) {
        if (node_list[i] == lookup)
            return;
    }
    ASSERT(false);
}

/* Verify the black depth along each path of the tree. */
static void
unit_test_verify_black_depth()
{
    ASSERT(!fragment_tree->root->red);

    if (fragment_tree->root != fragment_tree->nil) {
        int current_black_count = 0, tree_black_count = -1, dfs_queue_index = 0;
        bb_node_t *walk = fragment_tree->root;

        int dfs_queue_black_counts[FRAGMENT_TREE_TEST_NODE_COUNT];
        bb_node_t *dfs_queue[FRAGMENT_TREE_TEST_NODE_COUNT];

        while (true) {
            if (walk->red)
                ASSERT(!(walk->left->red || walk->right->red));
            else
                current_black_count++;
            if (walk->right == fragment_tree->nil) {
                if (tree_black_count < 0)
                    tree_black_count = current_black_count;
                else
                    ASSERT(current_black_count == tree_black_count);
            } else {
                dfs_queue[dfs_queue_index] = walk->right;
                dfs_queue_black_counts[dfs_queue_index] = current_black_count;
                dfs_queue_index++;
            }
            if (walk->left == fragment_tree->nil) {
                if (tree_black_count < 0)
                    tree_black_count = current_black_count;
                else
                    ASSERT(current_black_count == tree_black_count);
                if (dfs_queue_index == 0)
                    break;
                dfs_queue_index--;
                walk = dfs_queue[dfs_queue_index];
                current_black_count = dfs_queue_black_counts[dfs_queue_index];
            } else {
                walk = walk->left;
            }
        }
    }
}

static void
unit_test_lookup_all_nodes(bb_node_t **node_list, uint list_length)
{
    uint i, list_node_count = 0, tree_node_count = 0, dfs_queue_index = 0;
    bb_node_t *lookup, *dfs_queue[FRAGMENT_TREE_TEST_NODE_COUNT];

    for (i = 0; i < list_length; i++) {
        if (node_list[i] != NULL) {
            lookup = fragment_tree_lookup(fragment_tree, node_list[i]->start,
                                          node_list[i]->end);
            ASSERT(lookup == node_list[i]);
            list_node_count++;
        }
    }

    if (fragment_tree->root != fragment_tree->nil) {
        lookup = fragment_tree->root;
        while (true) {
            unit_test_find_node_in_list(node_list, lookup, list_length);
            tree_node_count++;
            if (lookup->right != fragment_tree->nil)
                dfs_queue[dfs_queue_index++] = lookup->right;
            if (lookup->left != fragment_tree->nil)
                lookup = lookup->left;
            else if (dfs_queue_index == 0)
                break;
            else
                lookup = dfs_queue[--dfs_queue_index];
        }
    }
    ASSERT(tree_node_count == list_node_count);

    unit_test_verify_black_depth();
}

static app_pc
unit_test_get_random_pc(app_pc range_start, uint max_range_size)
{
    return range_start + get_random_offset(max_range_size);
}

static bool
unit_test_insert_random_node(bb_node_t **node_list, app_pc random_base, uint random_span,
                             uint index)
{
    app_pc random_start = unit_test_get_random_pc(random_base, random_span);
    app_pc random_end = unit_test_get_random_pc(random_start + 2, 0x40);

    if (fragment_tree_lookup(fragment_tree, random_start, random_end) == NULL) {
        node_list[index] = fragment_tree_insert(fragment_tree, random_start, random_end);
        return true;
    } else {
        d_r_set_random_seed((uint)query_time_millis());
        return false;
    }
}

/* Inserts the specified number of new random nodes. */
static void
unit_test_insert_random_nodes(bb_node_t **node_list, uint insert_count)
{
    uint i;

    ASSERT(fragment_tree->root == fragment_tree->nil);

    for (i = 0; i < insert_count; i++) {
        if (unit_test_insert_random_node(node_list, 0, 0xffffffff, i)) {
            if ((i + 1) % 20 == 0)
                unit_test_lookup_all_nodes(node_list, i + 1);
        } else {
            i--; /* found exact match, so rewind and try another */
        }
    }
}

/* returns the number of nodes removed */
static uint
unit_test_remove_occupied_span(bb_node_t **node_list, uint list_length)
{
    uint i, list_removal_count = 0, tree_removal_count;
    app_pc start, end;
    bb_node_t *first = NULL, *second = NULL, *swap;

    /* randomly choose a span containing some nodes and clear it */
    for (i = 0; i < list_length; i++) {
        if (node_list[i] != NULL) {
            first = node_list[i++];
            break;
        }
    }
    for (; i < list_length; i++) {
        if (node_list[i] != NULL) {
            second = node_list[i];
            break;
        }
    }

    ASSERT(first != NULL && second != NULL);

    if (second->start < first->start ||
        (second->start == first->start && second->end < first->end)) {
        swap = first;
        first = second;
        second = swap;
    }

    /* randomly pick before, on, or after an occupied pc, to test all overlap cases */
    start = unit_test_get_random_pc(first->start == 0 ? 0 : first->start - 1, 2);
    end = unit_test_get_random_pc(second->end - 1,
                                  (ptr_uint_t)second->end < 0xffffffff ? 2 : 1);

    for (i = 0; i < list_length; i++) { /* walk and clear the span from the list */
        if (node_list[i] != NULL && start < node_list[i]->end &&
            end > node_list[i]->start) {
            list_removal_count++;
            node_list[i] = NULL;
        }
    }

    tree_removal_count = jitopt_clear_span(start, end); /* test the deployed code */
    ASSERT(list_removal_count == tree_removal_count);
    return tree_removal_count;
}

/* Randomly removes spans of nodes until at most 1 node remains. */
static void
unit_test_remove_random_spans(bb_node_t **node_list, uint list_length)
{
    uint i, node_count = list_length, verify_counter = 0;
    app_pc start, end;
    bool overlap;

    while (node_count > 1) {
        verify_counter++;
        /* attempt to remove a random unoccupied span */
        while (true) {
            /* fish for an unoccupied span */
            start = unit_test_get_random_pc(0, 0xffffffff);
            end = unit_test_get_random_pc(start + 0x10, 0x40);
            overlap = false;
            for (i = 0; i < list_length; i++) {
                if (node_list[i] != NULL && start < node_list[i]->end &&
                    end > node_list[i]->start) {
                    overlap = true;
                    break;
                }
            }
            if (!overlap) {
                jitopt_clear_span(start, end); /* test the deployed code */
                if (verify_counter % 20 == 0)
                    unit_test_lookup_all_nodes(node_list, list_length); /* verify */
                break;
            } /* else fish again for an unoccupied span */
        }

        node_count -= unit_test_remove_occupied_span(node_list, list_length);

        if (verify_counter % 20 == 0)
            unit_test_lookup_all_nodes(node_list, list_length); /* verify */
    }
}

/* Packs a small span with overlapping nodes, then removes all nodes from a subspan
 * and adds that many new random nodes back.
 */
static void
unit_test_churn_narrow_span(bb_node_t **node_list, uint list_length)
{
    uint i, j, k, node_count, random_span = list_length * 8;
    app_pc random_base = (app_pc)get_random_offset(0xf0000000);

    ASSERT(fragment_tree->root == fragment_tree->nil);

    for (i = 0; i < list_length; i++) { /* pack a small span */
        if (unit_test_insert_random_node(node_list, (app_pc)random_base, 10 + random_span,
                                         i)) {
            if ((i + 1) % 20 == 0)
                unit_test_lookup_all_nodes(node_list, i + 1);
        } else {
            i--; /* found exact match, so rewind and try another */
        }
    }

    for (i = 0; i < 10; i++) {
        node_count = unit_test_remove_occupied_span(node_list, list_length);
        for (j = 0; j < node_count; j++) {
            for (k = 0; k < list_length; k++) { /* find an empty slot */
                if (node_list[k] == NULL)
                    break;
            }
            if (unit_test_insert_random_node(node_list, (app_pc)random_base,
                                             10 + random_span, k)) {
                if ((i + 1) % 20 == 0)
                    unit_test_lookup_all_nodes(node_list, list_length);
            } else {
                j--; /* found exact match, so rewind and try another */
            }
        }
    }
}

void
unit_test_jit_fragment_tree()
{
    uint i;
    bb_node_t *node_list[FRAGMENT_TREE_TEST_NODE_COUNT]; /* N.B.: may contain NULLs */

    print_file(STDERR, "test DGC fragment tree: ");

    dynamo_options.opt_jit = true;

    fragment_tree = fragment_tree_create();
    d_r_set_random_seed((uint)query_time_millis());

    for (i = 0; i < 3; i++) {
        print_file(STDERR, "pass %d... ", i + 1);

        unit_test_insert_random_nodes(node_list, FRAGMENT_TREE_TEST_NODE_COUNT);
        unit_test_remove_random_spans(node_list, FRAGMENT_TREE_TEST_NODE_COUNT);
        fragment_tree_clear(fragment_tree);
        unit_test_churn_narrow_span(node_list, FRAGMENT_TREE_TEST_NODE_COUNT);
        fragment_tree_clear(fragment_tree);
    }

    fragment_tree_destroy(fragment_tree);

    print_file(STDERR, "\n");
}

#endif /* STANDALONE_UNIT_TEST */
