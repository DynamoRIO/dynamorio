#ifndef _JIT_OPT_H_
#define _JIT_OPT_H_ 1

void
jitopt_init();

void
jitopt_exit();

/* Account for a DGC basic block having the specified span in app space. */
void
jitopt_add_dgc_bb(app_pc start, app_pc end, bool is_trace_head);

/* Clear the fragment accounting structure within the specified span.
 * Returns the number of accounted basic blocks that were removed.
 */
uint
jitopt_clear_span(app_pc start, app_pc end);

#endif
