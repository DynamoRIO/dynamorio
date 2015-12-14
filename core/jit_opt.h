#ifndef _JIT_OPT_H_
#define _JIT_OPT_H_ 1

void
jitopt_init();

void
jitopt_exit();

void
jitopt_add_dgc_bb(app_pc start_pc, app_pc end_pc, bool is_trace_head);

void
jitopt_clear_span(app_pc start, app_pc end);

#endif
