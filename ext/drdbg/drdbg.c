/* **********************************************************
 * Copyright (c) 2016 Google, Inc.   All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* DynamoRIO Debugger Transparency Extension: an extension for
 * mantaining transparent debugging of targets running
 * under DynamoRIO.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drvector.h"
#include "drlist.h"
#include "drqueue.h"
#include "drdbg.h"
#include "drdbg_server_int.h"
#include "gdb/drdbg_srv_gdb.h"

#include <string.h> /* memcpy */

#include <sys/mman.h>

static drdbg_srv_int_t dbg_server;
static drlist_t *drdbg_memmaps;

void **drcontexts = NULL;
uint num_suspended;
uint num_unsuspended;
drdbg_stop_rsn_t stop_rsn;

static drdbg_handler_t *cmd_handlers;

bool pause_at_first_app_ins = true;
bool drdbg_break_on_entry = true;

drvector_t *drdbg_bps;
drlist_t *drdbg_bps_pending;

drqueue_t *drdbg_event_queue;
drqueue_t *drdbg_app_jobs;

drdbg_event_data_bp_t *current_bp_event;
drdbg_event_t *current_event;

/* DynamoRIO interactions */
drdbg_bp_t *
drdbg_bp_find_by_pc(void *pc)
{
    drdbg_bp_t *bp = NULL;
    int i = 0;

    drvector_lock(drdbg_bps);

    for (i = 0; i < drdbg_bps->entries; i++) {
        bp = drdbg_bps->array[i];
        if (bp->pc == pc) {
            drvector_unlock(drdbg_bps);
            return bp;
        }
    }

    drvector_unlock(drdbg_bps);
    return NULL;
}

drdbg_status_t
drdbg_bp_queue_ex(void *pc, bool flush_pc)
{
    drdbg_app_job_t *job;
    drdbg_app_job_data_flush_t *data;
    /* Check if breakpoint already exists, if so, enable it */
    drdbg_bp_t *bp;
    bp = drdbg_bp_find_by_pc(pc);
    if (bp != NULL) {
        bp->status = DRDBG_BP_ENABLED;
        return DRDBG_SUCCESS;
    }
    /*
     * Flush cache regions containing pc
     * XXX: We can't use dr_fragment_exists_at() since we can't guarantee
     * that pc is at the start of a BB, so we must always flush for now.
     */
    if (flush_pc) {
        /* Just realized that this doesn't work b/c it's getting my current threads
         * drcontext, which isn't valid, and flushing code in this thread is useless
         * anyway. Proposed solutions are to make a client job event queue that the
         * while loop in the client will handle (but it's suspended grrr), maybe
         * we can move away from the suspend/resume_all_other_threads model? Or somehow
         * use a different drcontext for the flush.
         */
        job = dr_global_alloc(sizeof(drdbg_app_job_t));
        job->type = DRDBG_JOB_FLUSH;
        data = dr_global_alloc(sizeof(drdbg_app_job_data_flush_t));
        data->pc = pc;
        data->size = 1;
        job->data = data;
        drqueue_push(drdbg_app_jobs, job); //  dr_flush_region(pc, 1);
    }
    /* Fill bp info */
    bp = dr_global_alloc(sizeof(drdbg_bp_t));
    if (bp == NULL)
        return DRDBG_ERROR;
    bp->pc = pc;
    bp->status = DRDBG_BP_PENDING;
    /* Push bp to pending queue */
    if (drlist_push_back(drdbg_bps_pending, bp) != true) {
        DR_ASSERT_MSG(false, "list pushback failed");
        dr_global_free(bp, sizeof(drdbg_bp_t));
        return DRDBG_ERROR;
    }
    /* XXX: Must remove from pending, can't assume it's the tail, either */
    if (drvector_append(drdbg_bps, bp) != true) {
        dr_global_free(bp, sizeof(drdbg_bp_t));
        return DRDBG_ERROR;
    }

    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_bp_queue(void *pc)
{
    return drdbg_bp_queue_ex(pc, true);
}

drdbg_status_t
drdbg_bp_disable(void *pc)
{
    drdbg_bp_t *bp = drdbg_bp_find_by_pc(pc);
    if (bp == NULL)
        return DRDBG_ERROR;
    bp->status = DRDBG_BP_DISABLED;
    return DRDBG_SUCCESS;
}

/* Clean call for triggering breakpoint event */
static void
drdbg_bp_cc_handler(drdbg_bp_t *bp)
{
    bool did_flush = false;
    drdbg_app_job_t *job = NULL;
    drdbg_event_t *event = NULL;
    drdbg_event_data_bp_t *data = NULL;
    if (bp == NULL)
        return;
    if (bp->status != DRDBG_BP_ENABLED)
        return;

    /* Create bp event and push into event queue */
    event = dr_global_alloc(sizeof(drdbg_event_t));
    event->event = DRDBG_EVENT_BP;
    event->drcontext = dr_get_current_drcontext();
    data = dr_global_alloc(sizeof(drdbg_event_data_bp_t));
    data->bp = bp;
    data->mcontext.flags = DR_MC_INTEGER|DR_MC_CONTROL|DR_MC_ALL;
    data->mcontext.size = sizeof(dr_mcontext_t);
    dr_get_mcontext(event->drcontext, &data->mcontext);
    data->mcontext.xip = bp->pc;
    data->keep_waiting = true;
    event->data = data;

    /* enter safe spot and wait */
    //dr_mark_safe_to_suspend(event->drcontext, true);

    drqueue_push(drdbg_event_queue, event);

    dr_suspend_all_other_threads(&drcontexts, &num_suspended, &num_unsuspended);
    while (data->keep_waiting) {
        dr_thread_yield();
        /* Handle drdbg events */
        if (!drqueue_isempty(drdbg_app_jobs)) {
            while ((job = drqueue_pop(drdbg_app_jobs)) != NULL) {
                switch (job->type) {
                case DRDBG_JOB_FLUSH:
                {
                    typedef drdbg_app_job_data_flush_t mydata_t;
                    dr_flush_region(((mydata_t *)job->data)->pc,
                                    ((mydata_t *)job->data)->size);
                    did_flush = true;
                    break;
                }
                default:
                    break;
                }
            }
        }
    }
    dr_resume_all_other_threads(drcontexts, num_suspended);
    if (did_flush)
        dr_redirect_execution(&data->mcontext);
}

drdbg_status_t
drdbg_bp_insert(void *drcontext, instrlist_t *bb, instr_t *instr, drdbg_bp_t *bp)
{
    if (bp == NULL)
        return DRDBG_ERROR;

    bp->bb = bb;
    bp->instr = instr;
    bp->status = DRDBG_BP_ENABLED;

    /* Insert clean-call to bp handler */
    dr_insert_clean_call(drcontext, bb, instr, drdbg_bp_cc_handler, false, 1, OPND_CREATE_INTPTR(bp));

    return DRDBG_SUCCESS;
}

static instr_t *
instrlist_find_pc(instrlist_t *ilist, app_pc pc)
{
    instr_t *next = instrlist_first_app(ilist);
    while (next != NULL) {
        if (instr_get_app_pc(next) == pc)
            return next;
        next = instr_get_next(next);
    }
    return NULL;
}

/* removes all instrs from start to the end of ilist
 * XXX: add to DR and export?
 */
static void
instrlist_truncate(void *drcontext, instrlist_t *ilist, instr_t *start)
{
    instr_t *next = start, *tmp;
    while (next != NULL) {
        tmp = next;
        next = instr_get_next(next);
        instrlist_remove(ilist, tmp);
        instr_destroy(drcontext, tmp);
    }
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                  bool for_trace, bool translating, void *user_data)
{
    drlist_node_t *node = NULL;
    drlist_node_t *del_node = NULL;
    //app_pc bb_first_pc = instr_get_app_pc(inst);

    /* Insert any pending breakpoints */
    node = drdbg_bps_pending->head;
    while (node != NULL) {
        if (node->data != NULL &&
            tag == ((drdbg_bp_t *)(node->data))->pc) {
            if (drdbg_bp_insert(drcontext, bb, inst, node->data) == DRDBG_SUCCESS) {
                del_node = node;
                node = node->next;
                drlist_remove(drdbg_bps_pending, del_node);
            }
        }
        if (del_node != NULL) {
            del_node = NULL;
        } else {
            node = node->next;
        }
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb,
                 bool for_trace, bool translating)
{
    if (pause_at_first_app_ins) {
        /*
         * Can't flush since we're in an event callback. Luckily, we
         * don't need to flush since this is the first block.
         */
        if (drdbg_bp_queue_ex(tag, false) != DRDBG_SUCCESS) {
            DR_ASSERT_MSG(false, "queue failed");
            return DR_EMIT_DEFAULT;
        }
    }
    /* If bb contains a pending breakpoint then split the bb on the
     * breakpoint address.
     */
    app_pc bb_first_pc = instr_get_app_pc(instrlist_first_app(bb));
    app_pc bb_last_pc = instr_get_app_pc(instrlist_last_app(bb));
    app_pc bp_pc;

    drlist_node_t *node = drdbg_bps_pending->head;
    while (node != NULL) {
        if (node->data != NULL) {
            bp_pc = ((drdbg_bp_t *)(node->data))->pc;
            if (bb_first_pc == bp_pc) {
                /* Remove the instructions after the bp */
                instrlist_truncate(drcontext, bb,
                                   instr_get_next(instrlist_first_app(bb)));

            } else if (bb_first_pc < bp_pc && bp_pc <= bb_last_pc) {
                /* Remove the instructions starting at the bp */
                instrlist_truncate(drcontext, bb, instrlist_find_pc(bb, bp_pc));
            }
        }
        node = node->next;
    }

    return DR_EMIT_DEFAULT;
}

/* Command handlers */
drdbg_status_t
drdbg_cmd_not_implemented(drdbg_srv_int_cmd_data_t *cmd_data)
{
    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_cmd_query_stop_rsn(drdbg_srv_int_cmd_data_t *cmd_data)
{
    typedef drdbg_cmd_data_query_stop_rsn_t mydata_t;
    mydata_t *data = (mydata_t *)calloc(1, sizeof(mydata_t));

    data->stop_rsn = DRDBG_STOP_RSN_RECV_SIG;
    data->signum = 5;
    cmd_data->cmd_data = data;

    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_cmd_reg_read(drdbg_srv_int_cmd_data_t *cmd_data)
{
    //dr_mcontext_t *mcontext = dr_global_alloc(sizeof(dr_mcontext_t));
    //mcontext->size = sizeof(dr_mcontext_t);
    //mcontext->flags = DR_MC_INTEGER|DR_MC_CONTROL;
    //dr_get_mcontext(drcontexts[0], mcontext);
    //*arg = (void *)mcontext;
    cmd_data->cmd_data = &current_bp_event->mcontext;
    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_cmd_reg_write(drdbg_srv_int_cmd_data_t *cmd_data)
{
    dr_mcontext_t *ctxt = &current_bp_event->mcontext;
    dr_mcontext_t *data = cmd_data->cmd_data;
    ctxt->xax = data->xax;
    ctxt->xbx = data->xbx;
    ctxt->xcx = data->xcx;
    ctxt->xdx = data->xdx;
    ctxt->xsi = data->xsi;
    ctxt->xdi = data->xdi;
    ctxt->xbp = data->xbp;
    ctxt->xsp = data->xsp;
#ifdef X64
    ctxt->r8 = data->r8;
    ctxt->r9 = data->r9;
    ctxt->r10 = data->r10;
    ctxt->r11 = data->r11;
    ctxt->r12 = data->r12;
    ctxt->r13 = data->r13;
    ctxt->r14 = data->r14;
    ctxt->r15 = data->r15;
#endif
    //ctxt->xip = data->xip;
    ctxt->xflags = data->xflags;
    if (!dr_set_mcontext(current_event->drcontext, ctxt)) {
        dr_fprintf(STDERR, "FAILED TO WRITE REG\n");
    }
    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_cmd_mem_read(drdbg_srv_int_cmd_data_t *cmd_data)
{
    typedef drdbg_cmd_data_mem_op_t mydata_t;
    mydata_t *data = (mydata_t *)cmd_data->cmd_data;
    char *read_data;
    size_t bytes_read;

    read_data = dr_global_alloc(data->len);
    memset(read_data, 0, data->len);

    //dr_switch_to_app_state(dr_get_current_drcontext());
    dr_safe_read(data->addr, data->len, read_data, &bytes_read);
    if (bytes_read != data->len) {
        dr_global_free(read_data, data->len);
        return DRDBG_ERROR;
    }
    //dr_switch_to_dr_state(dr_get_current_drcontext());

    data->data = read_data;
    data->len = bytes_read;
    cmd_data->cmd_data = data;
    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_cmd_mem_write(drdbg_srv_int_cmd_data_t *cmd_data)
{
    typedef drdbg_cmd_data_mem_op_t mydata_t;
    mydata_t *data = (mydata_t *)cmd_data->cmd_data;
    size_t bytes_wrote;

    //dr_switch_to_app_state(dr_get_current_drcontext());
    dr_safe_write(data->addr, data->len, data->data, &bytes_wrote);
    if (bytes_wrote != data->len) {
        return DRDBG_ERROR;
    }
    //dr_switch_to_dr_state(dr_get_current_drcontext());

    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_cmd_continue(drdbg_srv_int_cmd_data_t *cmd_data)
{
    if (current_bp_event == NULL)
        return DRDBG_ERROR;

    /* NB: This function is called to continue from drdbg_cmd_step to avoid code
     * reuse/divergence. If this function is updated in a way that may adversely affect
     * that, also update drdbg_cmd_step.
     */
    current_bp_event->keep_waiting = false;
    current_bp_event = NULL;
    //dr_resume_all_other_threads(drcontexts, num_suspended);

    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_cmd_step(drdbg_srv_int_cmd_data_t *cmd_data)
{
    instr_t i;
    app_pc tgt = NULL;
    size_t bread;

    if (current_bp_event == NULL)
        return DRDBG_ERROR;

    instr_init(current_event->drcontext, &i);
    decode(current_event->drcontext, current_bp_event->bp->pc, &i);
    if (instr_is_cti(&i)) {
        if (instr_is_return(&i)) {
            /* XXX: ARM uses lr register not the stack */
            dr_safe_read((void*)current_bp_event->mcontext.xsp, sizeof(void*), &tgt, &bread);
        } else {
            if (instr_is_cbr(&i) && !instr_jcc_taken(&i, current_bp_event->mcontext.xflags)) {
                tgt = current_bp_event->bp->pc + instr_length(current_event->drcontext, &i);
            } else {
                tgt = opnd_get_pc(instr_get_target(&i));
            }
        }
    } else {
        tgt = current_bp_event->bp->pc + instr_length(current_event->drcontext, &i);
    }
    dr_fprintf(STDERR, "Stepping to %p\n", tgt);

    drdbg_bp_queue(tgt);

    return drdbg_cmd_continue(cmd_data);
}

drdbg_status_t
drdbg_cmd_swbreak(drdbg_srv_int_cmd_data_t *cmd_data)
{
    typedef drdbg_cmd_data_swbreak_t mydata_t;
    mydata_t *data = (mydata_t *)cmd_data->cmd_data;
    drdbg_status_t ret = DRDBG_ERROR;
    if (((unsigned long)data->addr & 0xfff) == 0xca0)
        return DRDBG_SUCCESS;
    if (data->insert) {
        ret = drdbg_bp_queue(data->addr);
        return ret;
    } else {
        ret = drdbg_bp_disable(data->addr);
        return ret;
    }

}

drdbg_status_t
drdbg_cmd_kill(drdbg_srv_int_cmd_data_t *cmd_data)
{
    drdbg_exit();
    dr_exit_process(0);
    return DRDBG_ERROR;
}

/* drdbg managagement */
static
drdbg_status_t
drdbg_srv_start()
{
    return dbg_server.start(drdbg_options.port);
}

static
drdbg_status_t
drdbg_srv_accept()
{
    return dbg_server.accept();
}

static
drdbg_status_t
drdbg_srv_stop(void)
{
    return dbg_server.stop();
}

static
void
drdbg_server_loop(void *arg)
{
    drdbg_event_t *drdbg_event = NULL;
    drdbg_srv_int_cmd_data_t cmd_data;

    /* Command loop */
    while (1) {
        /* Get command from server */
        cmd_data.status = dbg_server.get_cmd(&cmd_data, current_bp_event ? true : false);
        if (cmd_data.status == DRDBG_SUCCESS) {
            cmd_data.status = cmd_handlers[cmd_data.cmd_id](&cmd_data);
            /* Send results to server */
            cmd_data.status = dbg_server.put_cmd(&cmd_data, false);
        }

        /* Handle drdbg events */
        if (!drqueue_isempty(drdbg_event_queue)) {
            while ((drdbg_event = drqueue_pop(drdbg_event_queue)) != NULL) {
                current_event = drdbg_event;
                switch (drdbg_event->event) {
                case DRDBG_EVENT_BP:
                    //dr_suspend_all_other_threads(&drcontexts, &num_suspended, &num_unsuspended);
                    if (drdbg_break_on_entry) {
                        drdbg_srv_accept();
                        drdbg_break_on_entry = false;
                    }
                    current_bp_event = drdbg_event->data;
                    /* If we just paused at the first app instruction disable
                     * the bp insertion in bb analysis, otherwise, send the
                     * debugger a stop signal.
                     */
                    if (pause_at_first_app_ins) {
                        pause_at_first_app_ins = false;
                    } else {
                        cmd_data.cmd_id = DRDBG_CMD_QUERY_STOP_RSN;
                        cmd_handlers[cmd_data.cmd_id](&cmd_data);
                        dbg_server.put_cmd(&cmd_data, false);
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    return;
}

static
void
drdbg_start_server(void *arg)
{
    drdbg_status_t ret;

    /* Mark thread as unsuspendable */
    //if (!dr_client_thread_set_suspendable(false))
        //return;

    /* Initialize server */
    ret = drdbg_srv_start();
    if (ret != DRDBG_SUCCESS)
        return;

    drdbg_server_loop(NULL);
}

static
drdbg_status_t
drdbg_init_data(void)
{
    drdbg_bps = dr_global_alloc(sizeof(drvector_t));
    if (drdbg_bps == NULL || !drvector_init(drdbg_bps, 10, true, NULL))
        return DRDBG_ERROR;

    drdbg_bps_pending = dr_global_alloc(sizeof(drlist_t));
    if (drdbg_bps_pending == NULL || !drlist_init(drdbg_bps_pending, true, NULL))
        return DRDBG_ERROR;

    drdbg_event_queue = dr_global_alloc(sizeof(drqueue_t));
    if (drdbg_event_queue == NULL || !drqueue_init(drdbg_event_queue, 10, true, NULL))
        return DRDBG_ERROR;

    drdbg_memmaps = dr_global_alloc(sizeof(drlist_t));
    if (drdbg_memmaps == NULL || !drlist_init(drdbg_memmaps, true, NULL))
        return DRDBG_ERROR;

    drdbg_app_jobs = dr_global_alloc(sizeof(drqueue_t));
    if (drdbg_app_jobs == NULL || !drqueue_init(drdbg_app_jobs, 10, true, NULL))
        return DRDBG_ERROR;

    return DRDBG_SUCCESS;
}

static
drdbg_status_t
drdbg_init_cmd_handlers(void)
{
    int i;
    cmd_handlers = (drdbg_handler_t *)calloc(DRDBG_CMD_NUM_CMDS, sizeof(drdbg_handler_t));

    /* Fill with not implemented */
    for (i = 0; i < DRDBG_CMD_NUM_CMDS; i++) {
        cmd_handlers[i] = drdbg_cmd_not_implemented;
    }

    /* Assign implemented command handlers */
    cmd_handlers[DRDBG_CMD_QUERY_STOP_RSN] = drdbg_cmd_query_stop_rsn;
    cmd_handlers[DRDBG_CMD_REG_READ] = drdbg_cmd_reg_read;
    //cmd_handlers[DRDBG_CMD_REG_WRITE] = drdbg_cmd_reg_write;
    cmd_handlers[DRDBG_CMD_MEM_READ] = drdbg_cmd_mem_read;
    cmd_handlers[DRDBG_CMD_MEM_WRITE] = drdbg_cmd_mem_write;
    cmd_handlers[DRDBG_CMD_SWBREAK] = drdbg_cmd_swbreak;
    cmd_handlers[DRDBG_CMD_CONTINUE] = drdbg_cmd_continue;
    cmd_handlers[DRDBG_CMD_STEP] = drdbg_cmd_step;
    cmd_handlers[DRDBG_CMD_KILL] = drdbg_cmd_kill;

    return DRDBG_SUCCESS;
}

static
drdbg_status_t
drdbg_init_events(void)
{
    if (!drmgr_init()) {
        return DRDBG_ERROR;
    }
    drmgr_register_bb_instrumentation_event(NULL, event_bb_analysis, NULL);
    drmgr_register_bb_app2app_event(event_bb_app2app, NULL);

    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_init(drdbg_options_t *ops_in)
{
    drdbg_status_t ret;

    /* Copy options */
    memcpy(&drdbg_options, ops_in, sizeof(drdbg_options_t));

    /*
     * Initialize server interface
     * XXX: Assume always gdb for now
     */
    ret = drdbg_srv_gdb_init(&dbg_server);
    if (ret != DRDBG_SUCCESS)
        return ret;

    /* Initialiation */
    if (drdbg_init_data() != DRDBG_SUCCESS ||
        drdbg_init_cmd_handlers() != DRDBG_SUCCESS ||
        drdbg_init_events() != DRDBG_SUCCESS) {
        dr_fprintf(STDERR, "Failed to initialize\n");
        return DRDBG_ERROR;
    }

    dr_flush_region(0, -1);

    /* Start server thread */
    if (!dr_create_client_thread(drdbg_start_server, NULL))
      return DRDBG_ERROR;

    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_exit(void)
{
    drdbg_status_t ret;

    ret = drdbg_srv_stop();
    if (ret != DRDBG_SUCCESS)
        return ret;

    return DRDBG_SUCCESS;
}

drdbg_status_t
drdbg_api_break(app_pc pc)
{
    // Setup bp structure
    drdbg_bp_t bp;
    bp.pc = pc;
    bp.status = DRDBG_BP_ENABLED;
    // Call bp cc handler
    dr_fprintf(STDERR, "SEMANTIC BREAKPOINT @%p\n", pc);
    drdbg_bp_cc_handler(&bp);
    return DRDBG_SUCCESS;
}
