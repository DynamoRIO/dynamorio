/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2009 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2004-2007 Determina Corp. */

/*
 * rct.c
 * Routines for the security features related to indirect calls and indirect jumps
 * in a platform independent manner.
 *
 */

#include "globals.h"
#ifdef RCT_IND_BRANCH

#    include "fragment.h"
#    include "rct.h"
#    include "module_shared.h"

#    ifdef WINDOWS
#        include "nudge.h" /* for generic_nudge_target() */
#    endif

#    ifdef X64
#        include "instr.h" /* for instr_raw_is_rip_rel_lea */
#    endif

/* General assumption all indirect branch targets on X86 will have an
 * absolute address encoded in the code or data sections of the binary
 * (e.g. address taken functions)
 *
 * Should go through each module's (non-zero) image sections meaning
 * [module_base,+modulesize) and look for any address pointing to the
 * code section(s) [baseof_code_section,+sizeof_code_section) for each
 * code section.
 *
 * FIXME: (optimization) Since there can be multiple code sections we
 * have to do this multiple times for each one in a module.  While
 * this may result in multiple passes with the current interface,
 * don't optimize before shown to be a hit.
 *
 * FIXME: (optimization) In case there are heavy-weight resource
 * sections (dialogues, etc.) we may want to skip them.
 *
 */

/* Only a single thread should be traversing new modules */
/* Currently this overlaps with the table_rwlock of global_rct_ind_targets */
DECLARE_CXTSWPROT_VAR(mutex_t rct_module_lock, INIT_LOCK_FREE(rct_module_lock));

/* look in text[start,end) memory range for any reference to values
 *  in referto[start,end) address range
 * Caller is responsible for ensuring that [start,end) is readable.
 *
 * TODO: add any such addresses to an OUT parameter hashtable
 * returns number of added references (for diagnostic purposes)
 */
uint
find_address_references(dcontext_t *dcontext, app_pc text_start, app_pc text_end,
                        app_pc referto_start, app_pc referto_end)
{
    uint references_found = 0; /* only for debugging  */
    DEBUG_DECLARE(uint references_already_known = 0;)

    app_pc cur_addr;
    app_pc last_addr = text_end - sizeof(app_pc); /* inclusive */

    LOG(GLOBAL, LOG_RCT, 2,
        "find_address_references: text[" PFX ", " PFX "), referto[" PFX ", " PFX ")\n",
        text_start, text_end, referto_start, referto_end);

    ASSERT(text_start <= text_end);       /* empty ok */
    ASSERT(referto_start <= referto_end); /* empty ok */

    ASSERT(sizeof(app_pc) == IF_X64_ELSE(8, 4));
    ASSERT((ptr_uint_t)(last_addr + 1) ==
           (((ptr_uint_t)last_addr) + 1)); /* byte increments */

    ASSERT(is_readable_without_exception(text_start, text_end - text_start));

    /* FIXME: could try to read dword[pc] dword[pc+4] and then merging them with shifts
     * and | to get dword[pc+1] dword[pc+2] dword[pc+3]  instead of reading memory
     * but of course only if KSTAT says the latter is indeed faster!
     */

    KSTART(rct_no_reloc);
    for (cur_addr = text_start; cur_addr <= last_addr; cur_addr++) {
        DEBUG_DECLARE(bool known_ref = false;)

        app_pc ref = *(app_pc *)cur_addr; /* note dereference here */
        if (rct_check_ref_and_add(dcontext, ref, referto_start,
                                  referto_end _IF_DEBUG(cur_addr) _IF_DEBUG(&known_ref)))
            references_found++;
        else {
            DODEBUG({
                if (known_ref)
                    references_already_known++;
            });
        }
#    ifdef X64
        /* PR 215408: look for "lea reg, [rip+disp]" */
        ref = instr_raw_is_rip_rel_lea(cur_addr, text_end);
        if (ref != NULL) {
            LOG(GLOBAL, LOG_RCT, 4,
                "find_address_references: rip-rel @" PFX " => " PFX "\n", cur_addr, ref);
            if (rct_check_ref_and_add(dcontext, ref, referto_start,
                                      referto_end _IF_DEBUG(cur_addr)
                                          _IF_DEBUG(&known_ref))) {
                STATS_INC(rct_ind_rip_rel_scan_new);
                references_found++;
            } else {
                DODEBUG({
                    if (known_ref) {
                        references_already_known++;
                        STATS_INC(rct_ind_rip_rel_scan_old);
                    } else
                        STATS_INC(rct_ind_rip_rel_scan_data);
                });
            }
        }
#    endif
    }
    KSTOP(rct_no_reloc);

    LOG(GLOBAL, LOG_RCT, 2,
        "find_address_references: scanned %u addresses, touched %u pages, "
        "added %u new, %u duplicate ind targets\n",
        (last_addr - text_start), (last_addr - text_start) / PAGE_SIZE, references_found,
        references_already_known);

    return references_found;
}

/* Check if the given address refers to [referto_start, refereto_end) and
 * add it to a hashtable of valid indirect branches.  Returns true if ref
 * is added to hashtable, false otherwise.
 * Optional OUT:  known_ref -- true if ref was already known
 *
 * FIXME: called from find_relocation_addresses, if perf critical make
 * this static inline.
 * FIXME: make find_address_references call this once static inlined
 */

bool
rct_check_ref_and_add(dcontext_t *dcontext, app_pc ref, app_pc referto_start,
                      app_pc referto_end _IF_DEBUG(app_pc addr)
                          _IF_DEBUG(bool *known_ref /* OPTIONAL OUT */))
{
    /* can be null when called from find_address_references */
    if (ref == NULL)
        return false;

    DODEBUG({
        if (known_ref != NULL)
            *known_ref = false;
    });

    /* reference outside range */
    if (ref < referto_start || ref >= referto_end) {
        return false;
    }

    /* indeed points to a code section */
    DOLOG(3, LOG_RCT, {
        char symbuf[MAXIMUM_SYMBOL_LENGTH];
        LOG(GLOBAL, LOG_RCT, 3,
            "rct_check_ref_and_add:  " PFX " addr taken reference at " PFX "\n", ref,
            addr);
        print_symbolic_address(ref, symbuf, sizeof(symbuf), true);
        LOG(GLOBAL, LOG_SYMBOLS, 3, "\t%s\n", symbuf);
    });

    if (rct_add_valid_ind_branch_target(dcontext, ref)) {
        STATS_INC(rct_ind_branch_valid_targets);
        LOG(GLOBAL, LOG_RCT, 3, "\t " PFX " added\n", ref);

        return true; /* ref added, known_ref == false */
    } else {
        STATS_INC(rct_ind_branch_existing_targets);
        LOG(GLOBAL, LOG_RCT, 3, "\t known\n");

        DODEBUG({
            if (known_ref != NULL)
                *known_ref = true;
        });

        return false; /* ref not added, known_ref == true */
    }
    ASSERT_NOT_REACHED();
}

/* There are several alternative solutions to keeping the information
 * about known valid indirect branch targets: a single global
 * hashtable, per-module hashtables.  We can use the latter for fast flushes
 * yet we'd need a binary search to find the right module hashtable,
 * or we can merge all module hashtables into the global one for lookups.
 *
 * Considering that we will want to have hashtables that provide the
 * intersection of the above allowed targets with existing traces (or
 * in the near future bbs), having yet another middle level table may
 * not be worth it.  If we do have bb2bb then we'd have to do this
 * much less often and the cost of the module search will not be
 * exposed, and in case it is we can at least cache the last two
 * looked up entries (like vmareas caches last_area).
 *
 * FIXME: for now using a single global hashtable and flushing of the whole table
 * is the only provided implementation
 */

/* TODO: pass the necessary bookkeeping hashtable if done per-module */
static inline bool
is_address_taken(dcontext_t *dcontext, app_pc target)
{
    return rct_ind_branch_target_lookup(dcontext, target) != NULL;
}

static inline bool
is_address_after_call(dcontext_t *dcontext, app_pc target)
{
    /* FIXME: practically equivalent to find_call_site() */
    return fragment_after_call_lookup(dcontext, target) != NULL;
}

/* Restricted control transfer check on indirect branches called by
 * d_r_dispatch after inlined indirect branch lookup routine has failed
 *
 * function does not return if a security violation is blocked
 * FIXME: return value is ignored
 */
int
rct_ind_branch_check(dcontext_t *dcontext, app_pc target_addr, app_pc src_addr)
{
    bool is_ind_call = EXIT_IS_CALL(dcontext->last_exit->flags);
    security_violation_t indirect_branch_violation =
        is_ind_call ? INDIRECT_CALL_RCT_VIOLATION : INDIRECT_JUMP_RCT_VIOLATION;
    int res = 0;
    bool cache = true;
    DEBUG_DECLARE(const char *ibranch_type = is_ind_call ? "call" : "jmp";)
    ASSERT(is_ind_call || EXIT_IS_JMP(dcontext->last_exit->flags));

    ASSERT((is_ind_call && TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call))) ||
           (!is_ind_call && TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))));

    LOG(THREAD, LOG_RCT, 2, "RCT: ind %s target = " PFX "\n", ibranch_type, target_addr);

    STATS_INC(rct_ind_branch_validations);

    DOSTATS({
        /* since the exports are now added to the global hashtable,
         * we have to check this first to collect these stats
         */
        /* FIXME: we need symbols at loglevel 0 for this to work */
        if (rct_is_exported_function(target_addr)) {
            if (is_ind_call)
                STATS_INC(rct_ind_call_exports);
            else
                STATS_INC(rct_ind_jmp_exports);

            if (DYNAMO_OPTION(IAT_convert)) {
                LOG(THREAD, LOG_RCT, 2,
                    "RCT: address taken export or IAT conversion missed for " PFX,
                    target_addr);
                /* the module entry point is in fact hit here */
                /* FIXME: investigate if an export is really not used
                 * via IAT or a variation of register.
                 */
            }
        }
    });

    /* FIXME: if we use per-module hashtables, we'd first have to
     * looking up the module in question */
    if (!is_address_taken(dcontext, target_addr)) {
        bool is_code_section;
        /* FIXME: use loglevel 2 when the define is on by default */
        LOG(THREAD, LOG_RCT, 1, "RCT: bad ind %s target: " PFX ", source " PFX "\n",
            ibranch_type, target_addr, src_addr);

        DOLOG(2, LOG_RCT, {
            char symbuf[MAXIMUM_SYMBOL_LENGTH];
            print_symbolic_address(target_addr, symbuf, sizeof(symbuf), true);
            LOG(THREAD, LOG_SYMBOLS, 2, "\t%s\n", symbuf);
        });

#    ifdef DR_APP_EXPORTS
        /* case 9195: Allow certain API calls so we don't get security violations
         * when using DR with the start / stop interface.
         * NOTE: this is a security hole so should never be in product build
         */
        if (target_addr == (app_pc)dr_app_start ||
            target_addr == (app_pc)dr_app_take_over ||
            target_addr == (app_pc)dr_app_stop ||
            target_addr == (app_pc)dr_app_stop_and_cleanup ||
            target_addr == (app_pc)dr_app_stop_and_cleanup_with_stats ||
            target_addr == (app_pc)dr_app_cleanup)
            goto good;
#    endif
        if (!is_ind_call) {
            /* for DYNAMO_OPTION(rct_ind_jump) we need to allow an
             * after call location (normally a target of return) to be
             * targeted.
             * Instances in ole32.dll abound.
             *
             * 77a7f057 e8ac2ffdff       call    ole32!IIDFromString+0xf6 (77a52008)
             * ole32!IIDFromString+0x107:
             * 77a52022 8b08             mov     ecx,[eax]
             * 77a52024 8b4004           mov     eax,[eax+0x4]
             * 77a52027 ffe0             jmp     eax
             *
             * We would need to ensure that ret_after_call is turned
             * on, FIXME: should turn into a security_option_t that needs
             * to be at least OPTION_ENABLED
             */

            if (DYNAMO_OPTION(ret_after_call)) {
                if (is_address_after_call(dcontext, target_addr)) {
                    LOG(THREAD, LOG_RCT, 1,
                        "RCT: bad ind jump targeting an after call site: " PFX "\n",
                        target_addr);
                    STATS_INC(rct_ind_jmp_allowed_to_ac);
                    /* the current thread's indirect jmp IBL table will cache this */
                    goto good;
                }
            } else {
                /* case 4982: we can't use RAC data - we'll get a violation
                 * FIXME: add better option enforcement after making ret_after_call a
                 * security_option_t.
                 */
                ASSERT_NOT_IMPLEMENTED(false);
            }
        }

        /* PR 275723: RVA-table-based switch statements.  We check this prior to
         * rct_analyze_module_at_violation to avoid excessive scanning of x64
         * modules from PR 277044/277064.
         */
        if (DYNAMO_OPTION(rct_exempt_intra_jmp)) {
            app_pc code_start, code_end;
            app_pc modbase = get_module_base(target_addr);
            if (modbase != NULL &&
                is_in_code_section(modbase, target_addr, &code_start, &code_end) &&
                src_addr >= code_start && src_addr < code_end) {
                STATS_INC(rct_ind_jmp_x64switch);
                STATS_INC(rct_ind_jmp_exemptions);
                LOG(THREAD, LOG_RCT, 2,
                    "RCT: target " PFX " in same code sec as src " PFX " --ok\n",
                    target_addr, src_addr);
                /* Though we have per-module tables (except on Linux) (PR 214107)
                 * we don't check by source (and xref PR 204770) and don't have
                 * per-module ibl tables, so we can't cache this.
                 */
                cache = false;
                res = 2;
                goto exempted;
            }
        }

        /* Grab the rct_module_lock to ensure no duplicates if two threads
         * attempt to add the same module
         */
        d_r_mutex_lock(&rct_module_lock);
        /* NOTE - under current default options (analyze_at_load) this routine will have
         * no effect and is only being used for its is_in_code_section (&IMAGE) return
         * value.  For x64 it is used to scan on violation (PR 277044/277064). */
        is_code_section = rct_analyze_module_at_violation(dcontext, target_addr);
        d_r_mutex_unlock(&rct_module_lock);

        /* In fact, we have to let all regions that are not modules - so
         * .A and .B attacks will still be marked as such instead of failing here.
         */
        if (!is_code_section) {
            /* FIXME: assert DGC */
            /* we could be targeting a .data section that is within a
             * module we still don't cache it but only a performance
             * hit - it will not be reported anyways
             */
            STATS_INC(rct_ind_branch_not_code_section);

            /* ASLR: check if is in wouldbe region, if so report as failure */
            if (aslr_is_possible_attack(target_addr)) {
                LOG(THREAD, LOG_RCT, 1,
                    "RCT: ASLR: wouldbe a preferred DLL, " PFX " --BAD\n", target_addr);
                STATS_INC(aslr_rct_ind_wouldbe);
                /* fall through and report */
            } else {
                LOG(THREAD, LOG_RCT, 1,
                    "RCT: not a code section, ignoring " PFX " --check\n", target_addr);
                /* not caching violation target if not in code section */
                return 2; /* positive, ignore violation */
            }
        }

        /* we could be running in an unload race, and is_code_section
         * would still be true if target is not yet unmapped, if it
         * just became unreadable we also ignore like above
         */
        if (is_unreadable_or_currently_unloaded_region(target_addr)) {
            STATS_INC(rct_ind_branch_unload_race);
            LOG(THREAD, LOG_RCT, 1, "RCT: unload race, ignoring " PFX " --check\n",
                target_addr);
            /* not caching violation target since it will disappear */
            /* note that we should throw exception while processing code origin checks */
            return 2; /* positive, ignore violation */
        }

        if (is_address_taken(dcontext, target_addr)) {
            LOG(THREAD, LOG_RCT, 1, "RCT: new module added for " PFX " --ok\n",
                target_addr);
            STATS_INC(rct_ok_at_vio);
            goto good;
        }

        /* FIXME: this code will not be needed if the exports are
         * automatically added to the list yet I'd like to keep it
         * around for the STATS that may be relevant to case 1948
         */
        /* FIXME: case 3946 we need symbols at loglevel 0 to collect these stats */
        DOLOG(1, LOG_RCT | LOG_SYMBOLS, {
            /* this is an expensive bsearch, so we may not want to
             * collect it as other stats */
            if (rct_is_exported_function(target_addr)) {
                DOSTATS({
                    if (is_ind_call)
                        STATS_INC(rct_ind_call_exports);
                    else
                        STATS_INC(rct_ind_jmp_exports);
                });
                SYSLOG_INTERNAL_WARNING_ONCE("missed an export " PFX " caught by "
                                             "rct_is_exported_function()",
                                             target_addr);
                DOLOG(1, LOG_RCT, {
                    char name[MAXIMUM_SYMBOL_LENGTH];
                    print_symbolic_address(target_addr, name, sizeof(name), false);
                    LOG(THREAD, LOG_RCT, 1, "RCT: exported function " PFX " %s missed!\n",
                        target_addr, name);
                });
            }
        });

        LOG(THREAD, LOG_RCT, 1,
            "RCT: BAD[%d]  problem target=" PFX " src fragment=" PFX " type=%s\n",
            GLOBAL_STAT(rct_ind_call_violations) + GLOBAL_STAT(rct_ind_jmp_violations),
            target_addr, src_addr, ibranch_type);

        /* FIXME: case 4331, as a minimal change for 2.1, this
         * currently reuses several .C exceptions, while in fact only
         * the -exempt_rct list is needed (but not fibers)
         */
        if (at_known_exception(dcontext, target_addr, src_addr)) {
            LOG(THREAD, LOG_RCT, 1, "RCT: target " PFX " exempted --ok\n");
            DOSTATS({
                if (is_ind_call)
                    STATS_INC(rct_ind_call_exemptions);
                else
                    STATS_INC(rct_ind_jmp_exemptions);
            });

            res = 2;
            /* we'll cache violation target */
            goto exempted;
        }

        DOSTATS({
            if (is_ind_call)
                STATS_INC(rct_ind_call_violations);
            else
                STATS_INC(rct_ind_jmp_violations);
        });
        SYSLOG_INTERNAL_WARNING_ONCE(
            "indirect %s targeting unknown " PFX,
            EXIT_IS_CALL(dcontext->last_exit->flags) ? "call" : "jmp", target_addr);
        /* does not return when OPTION_BLOCK is enforced */
        if (security_violation(dcontext, target_addr, indirect_branch_violation,
                               is_ind_call ? DYNAMO_OPTION(rct_ind_call)
                                           : DYNAMO_OPTION(rct_ind_jump)) ==
            indirect_branch_violation) {
            /* running in detect mode */
            /* we'll cache violation target */
            res = -2;
        } else {
            res = -3;
            /* exempted Threat ID */
            /* we'll cache violation target */
        }

    exempted:
        /* Exempted or bad in detect mode, either way should add the
         * violating address so future references to it in other
         * threads do not fail.
         */
        if (DYNAMO_OPTION(rct_cache_exempt) != RCT_CACHE_EXEMPT_NONE) {
            d_r_mutex_lock(&rct_module_lock);
            rct_add_valid_ind_branch_target(dcontext, target_addr);
            d_r_mutex_unlock(&rct_module_lock);
        }
        return res;
    }
good:
    LOG(THREAD, LOG_RCT, 3, "RCT: good ind %s to " PFX "\n", ibranch_type, target_addr);
    DOSTATS({
        if (is_ind_call)
            STATS_INC(rct_ind_call_good);
        else
            STATS_INC(rct_ind_jmp_good);
    });
    return 1;
}

#    ifdef WINDOWS
/* Add allowed targets in dynamorio.dll.
 * Note: currently only needed for WINDOWS
 * Note: needs dynamo_dll_start to be initialized (currently done by
 *       vmareas_init->find_dynamo_library_vm_areas)
 */
void
rct_known_targets_init(void)
{
    /* Allow targeting dynamorio!generic_nudge_target and
     * dynamorio!safe_apc_or_thread_target (case 7266)
     */
    d_r_mutex_lock(&rct_module_lock);

    ASSERT(is_in_dynamo_dll((app_pc)generic_nudge_target));
    rct_add_valid_ind_branch_target(GLOBAL_DCONTEXT, (app_pc)generic_nudge_target);

    ASSERT(is_in_dynamo_dll((app_pc)safe_apc_or_thread_target));
    rct_add_valid_ind_branch_target(GLOBAL_DCONTEXT, (app_pc)safe_apc_or_thread_target);

    d_r_mutex_unlock(&rct_module_lock);
}
#    endif

void
rct_init(void)
{
    if (!(TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
          TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump)))) {
        ASSERT(!(TESTANY(OPTION_REPORT | OPTION_BLOCK, DYNAMO_OPTION(rct_ind_call)) ||
                 TESTANY(OPTION_REPORT | OPTION_BLOCK, DYNAMO_OPTION(rct_ind_jump))));
        return;
    }
    /* FIXME: hashtable_init currently in fragment.c should be moved here */
    IF_WINDOWS(rct_known_targets_init());
}

void
rct_exit(void)
{
    if (!(TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
          TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))))
        return;

    DELETE_LOCK(rct_module_lock);
}

#endif /* RCT_IND_BRANCH */
