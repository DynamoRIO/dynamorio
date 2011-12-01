/* **********************************************************
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"

static
dr_emit_flags_t bb_event(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr, *next_next_instr;
    static app_pc target = NULL;
    
    /* Look for pattern: nop; nop; call direct; */
    for (instr = instrlist_first(bb);
         instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (next_instr != NULL)
            next_next_instr = instr_get_next(next_instr);
        else
            next_next_instr = NULL;

        if (instr_is_nop(instr) && 
            next_instr != NULL && instr_is_nop(next_instr)) {
            if (instr_is_call_direct(next_next_instr)) {
                /* The first nop followed by a direct call is a marker
                 * for the function we want to call.
                 */
                if (target == NULL) {
                    target = instr_get_branch_target_pc(next_next_instr);
                }
                /* The second nop followd by a direct call is a marker
                 * for the call we want to modify.
                 */
                else {
                    instr_set_branch_target_pc(next_next_instr, target);
                }
            } else if (target != NULL && instr_is_nop(next_next_instr)) {
                /* We do see code like:
                 * interp: start_pc = 0x000007fefd6f2054
                 * check_thread_vm_area: pc = 0x000007fefd6f2054
                 *   0x000007fefd6f2054  66 0f 1f 04 00       data16 nop    (%rax,%rax,1) 
                 *   0x000007fefd6f2059  90                   nop    
                 *   0x000007fefd6f205a  90                   nop    
                 *   0x000007fefd6f205b  90                   nop    
                 *   0x000007fefd6f205c  90                   nop    
                 *   0x000007fefd6f205d  90                   nop    
                 *   0x000007fefd6f205e  90                   nop    
                 *   0x000007fefd6f205f  90                   nop    
                 *   0x000007fefd6f2060  0f b6 03             movzx  (%rbx) -> %eax 
                 *   0x000007fefd6f2063  ff ca                dec    %edx -> %edx 
                 *   wrote overflow flag before reading it!
                 *   0x000007fefd6f2065  48 83 c7 02          add    $0x0000000000000002 %rdi -> %rdi 
                 *   wrote all 6 flags now!
                 *   0x000007fefd6f2069  0f b7 0c 46          movzx  (%rsi,%rax,2) -> %ecx 
                 *   0x000007fefd6f206d  48 ff c3             inc    %rbx -> %rbx 
                 *   0x000007fefd6f2070  66 89 4f fe          data16 mov    %cx -> 0xfffffffe(%rdi) 
                 *   0x000007fefd6f2074  85 d2                test   %edx %edx 
                 *   0x000007fefd6f2076  7f e8                jnle   $0x000007fefd6f2060 
                 * end_pc = 0x000007fefd6f2078
                 * So we use target value as help too.
                 */
                /* add a call */
                app_pc pc = instr_get_app_pc(next_next_instr);
                instr_t *call = INSTR_CREATE_call(drcontext,
                                                  opnd_create_pc(target));
                INSTR_XL8(call, pc);
                instrlist_postinsert(bb, next_next_instr, call);
                for (instr  = instr_get_next(call); 
                     instr != NULL;
                     instr  = instr_get_next(call)) {
                    instrlist_remove(bb, instr);
                    instr_destroy(drcontext, instr);
                }
                /* set return target to be the next instr of next_next_instr */
                instrlist_set_return_target(bb,
                                            pc + instr_length(drcontext,
                                                              next_next_instr));
                target = NULL;
            }
            break;
        }
    }

    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
}
