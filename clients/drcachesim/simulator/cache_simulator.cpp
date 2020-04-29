/* **********************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <iostream>
#include <iterator>
#include <string>
#include <assert.h>
#include <limits.h>
#include <stdint.h> /* for supporting 64-bit integers*/
#include "../common/memref.h"
#include "../common/options.h"
#include "../common/utils.h"
#include "../reader/config_reader.h"
#include "../reader/file_reader.h"
#include "../reader/ipc_reader.h"
#include "cache_stats.h"
#include "cache.h"
#include "cache_lru.h"
#include "cache_fifo.h"
#include "cache_simulator.h"
#include "droption.h"

#include "snoop_filter.h"

analysis_tool_t *
cache_simulator_create(const cache_simulator_knobs_t &knobs)
{
    return new cache_simulator_t(knobs);
}

analysis_tool_t *
cache_simulator_create(const std::string &config_file)
{
    return new cache_simulator_t(config_file);
}

cache_simulator_t::cache_simulator_t(const cache_simulator_knobs_t &knobs)
    : simulator_t(knobs.num_cores, knobs.skip_refs, knobs.warmup_refs,
                  knobs.warmup_fraction, knobs.sim_refs, knobs.cpu_scheduling,
                  knobs.verbose)
    , knobs_(knobs)
    , l1_icaches_(NULL)
    , l1_dcaches_(NULL)
    , is_warmed_up_(false)
{
    // XXX i#1703: get defaults from hardware being run on.

    // This configuration allows for one shared LLC only.
    cache_t *llc = create_cache(knobs_.replace_policy);
    if (llc == NULL) {
        success_ = false;
        return;
    }

    std::string cache_name = "LL";
    all_caches_[cache_name] = llc;
    llcaches_[cache_name] = llc;

    if (knobs_.data_prefetcher != PREFETCH_POLICY_NEXTLINE &&
        knobs_.data_prefetcher != PREFETCH_POLICY_NONE) {
        // Unknown value.
        success_ = false;
        return;
    }

    bool warmup_enabled_ = ((knobs_.warmup_refs > 0) || (knobs_.warmup_fraction > 0.0));

    if (!llc->init(knobs_.LL_assoc, (int)knobs_.line_size, (int)knobs_.LL_size, NULL,
                   new cache_stats_t(knobs_.LL_miss_file, warmup_enabled_))) {
        error_string_ =
            "Usage error: failed to initialize LL cache.  Ensure sizes and "
            "associativity are powers of 2, that the total size is a multiple "
            "of the line size, and that any miss file path is writable.";
        success_ = false;
        return;
    }

    l1_icaches_ = new cache_t *[knobs_.num_cores];
    l1_dcaches_ = new cache_t *[knobs_.num_cores];
    unsigned int total_snooped_caches = 2 * knobs_.num_cores;
    snooped_caches_ = new cache_t *[total_snooped_caches];
    if (knobs_.model_coherence) {
        snoop_filter_ = new snoop_filter_t;
    }

    for (unsigned int i = 0; i < knobs_.num_cores; i++) {
        l1_icaches_[i] = create_cache(knobs_.replace_policy);
        if (l1_icaches_[i] == NULL) {
            success_ = false;
            return;
        }
        snooped_caches_[2 * i] = l1_icaches_[i];
        l1_dcaches_[i] = create_cache(knobs_.replace_policy);
        if (l1_dcaches_[i] == NULL) {
            success_ = false;
            return;
        }
        snooped_caches_[(2 * i) + 1] = l1_dcaches_[i];

        if (!l1_icaches_[i]->init(
                knobs_.L1I_assoc, (int)knobs_.line_size, (int)knobs_.L1I_size, llc,
                new cache_stats_t("", warmup_enabled_, knobs_.model_coherence),
                nullptr /*prefetcher*/, false /*inclusive*/, knobs_.model_coherence,
                2 * i, snoop_filter_) ||
            !l1_dcaches_[i]->init(
                knobs_.L1D_assoc, (int)knobs_.line_size, (int)knobs_.L1D_size, llc,
                new cache_stats_t("", warmup_enabled_, knobs_.model_coherence),
                knobs_.data_prefetcher == PREFETCH_POLICY_NEXTLINE
                    ? new prefetcher_t((int)knobs_.line_size)
                    : nullptr,
                false /*inclusive*/, knobs_.model_coherence, (2 * i) + 1,
                snoop_filter_)) {
            error_string_ = "Usage error: failed to initialize L1 caches.  Ensure sizes "
                            "and associativity are powers of 2 "
                            "and that the total sizes are multiples of the line size.";
            success_ = false;
            return;
        }

        cache_name = "L1_I_Cache_" + std::to_string(i);
        all_caches_[cache_name] = l1_icaches_[i];
        cache_name = "L1_D_Cache_" + std::to_string(i);
        all_caches_[cache_name] = l1_dcaches_[i];
    }

    if (knobs_.model_coherence &&
        !snoop_filter_->init(snooped_caches_, total_snooped_caches)) {
        ERRMSG("Usage error: failed to initialize snoop filter.\n");
        success_ = false;
        return;
    }
}

cache_simulator_t::cache_simulator_t(const std::string &config_file)
    : simulator_t()
    , l1_icaches_(NULL)
    , l1_dcaches_(NULL)
    , snooped_caches_(NULL)
    , snoop_filter_(NULL)
    , is_warmed_up_(false)
{
    std::map<std::string, cache_params_t> cache_params;
    config_reader_t config_reader;
    if (!config_reader.configure(config_file, knobs_, cache_params)) {
        error_string_ =
            "Usage error: Failed to read/parse configuration file " + config_file;
        success_ = false;
        return;
    }

    init_knobs(knobs_.num_cores, knobs_.skip_refs, knobs_.warmup_refs,
               knobs_.warmup_fraction, knobs_.sim_refs, knobs_.cpu_scheduling,
               knobs_.verbose);

    if (knobs_.data_prefetcher != PREFETCH_POLICY_NEXTLINE &&
        knobs_.data_prefetcher != PREFETCH_POLICY_NONE) {
        // Unknown prefetcher type.
        success_ = false;
        return;
    }

    bool warmup_enabled_ = ((knobs_.warmup_refs > 0) || (knobs_.warmup_fraction > 0.0));

    l1_icaches_ = new cache_t *[knobs_.num_cores];
    l1_dcaches_ = new cache_t *[knobs_.num_cores];

    // Create all the caches in the hierarchy.
    for (const auto &cache_params_it : cache_params) {
        std::string cache_name = cache_params_it.first;
        const auto &cache_config = cache_params_it.second;

        cache_t *cache = create_cache(cache_config.replace_policy);
        if (cache == NULL) {
            success_ = false;
            return;
        }

        all_caches_[cache_name] = cache;
    }

    int num_LL = 0;
    unsigned int total_snooped_caches = 0;
    std::string lowest_shared_cache = "";
    if (knobs_.model_coherence) {
        snoop_filter_ = new snoop_filter_t;
        std::string LL_name;
        /* This block determines where in the cache hierarchy to place the snoop filter.
         * If there is more than one LLC, the snoop filter is above those.
         */
        for (const auto &cache_it : all_caches_) {
            std::string cache_name = cache_it.first;
            const auto &cache_config = cache_params.find(cache_name)->second;
            if (cache_config.parent == CACHE_PARENT_MEMORY) {
                num_LL++;
                LL_name = cache_config.name;
            }
        }
        if (num_LL == 1) {
            /* There is one LLC, so we find highest cache with
             * multiple children to place the snoop filter.
             * Fully shared caches are marked as non-coherent.
             */
            cache_params_t current_cache = cache_params.find(LL_name)->second;
            non_coherent_caches_[LL_name] = all_caches_[LL_name];
            while (current_cache.children.size() == 1) {
                std::string child_name = current_cache.children[0];
                non_coherent_caches_[child_name] = all_caches_[child_name];
                current_cache = cache_params.find(child_name)->second;
            }
            if (current_cache.children.size() > 0) {
                lowest_shared_cache = current_cache.name;
                total_snooped_caches = (unsigned int)current_cache.children.size();
            }
        } else {
            total_snooped_caches = num_LL;
        }
        snooped_caches_ = new cache_t *[total_snooped_caches];
    }

    // Initialize all the caches in the hierarchy and identify both
    // the L1 caches and LLC(s).
    int snoop_id = 0;
    for (const auto &cache_it : all_caches_) {
        std::string cache_name = cache_it.first;
        cache_t *cache = cache_it.second;

        // Locate the cache's configuration.
        const auto &cache_config_it = cache_params.find(cache_name);
        if (cache_config_it == cache_params.end()) {
            error_string_ =
                "Error locating the configuration of the cache: " + cache_name;
            success_ = false;
            return;
        }
        auto &cache_config = cache_config_it->second;

        // Locate the cache's parent_.
        cache_t *parent_ = NULL;
        if (cache_config.parent != CACHE_PARENT_MEMORY) {
            const auto &parent_it = all_caches_.find(cache_config.parent);
            if (parent_it == all_caches_.end()) {
                error_string_ = "Error locating the configuration of the parent cache: " +
                    cache_config.parent;
                success_ = false;
                return;
            }
            parent_ = parent_it->second;
        }

        // Locate the cache's children.
        std::vector<caching_device_t *> children;
        children.clear();
        for (std::string &child_name : cache_config.children) {
            const auto &child_it = all_caches_.find(child_name);
            if (child_it == all_caches_.end()) {
                error_string_ =
                    "Error locating the configuration of the cache: " + child_name;
                success_ = false;
                return;
            }
            children.push_back(child_it->second);
        }

        // Determine if this cache should be connected to the snoop filter.
        bool LL_snooped = num_LL > 1 && cache_config.parent == CACHE_PARENT_MEMORY;
        bool mid_snooped = total_snooped_caches > 1 &&
            cache_config.parent.compare(lowest_shared_cache) == 0;

        bool is_snooped = knobs_.model_coherence && (LL_snooped || mid_snooped);

        // If cache is below a snoop filter, it should be marked as coherent.
        bool is_coherent_ = knobs_.model_coherence &&
            (non_coherent_caches_.find(cache_name) == non_coherent_caches_.end());

        if (!cache->init(
                (int)cache_config.assoc, (int)knobs_.line_size, (int)cache_config.size,
                parent_,
                new cache_stats_t(cache_config.miss_file, warmup_enabled_, is_coherent_),
                cache_config.prefetcher == PREFETCH_POLICY_NEXTLINE
                    ? new prefetcher_t((int)knobs_.line_size)
                    : nullptr,
                cache_config.inclusive, is_coherent_, is_snooped ? snoop_id : -1,
                is_snooped ? snoop_filter_ : nullptr, children)) {
            error_string_ = "Usage error: failed to initialize the cache " + cache_name;
            success_ = false;
            return;
        }

        // Next snooped cache should have a different ID.
        if (is_snooped) {
            snooped_caches_[snoop_id] = cache;
            snoop_id++;
        }

        bool is_l1_or_llc = false;

        // Assign the pointers to the L1 instruction and data caches.
        if (cache_config.core >= 0 && cache_config.core < (int)knobs_.num_cores) {
            is_l1_or_llc = true;
            if (cache_config.type == CACHE_TYPE_INSTRUCTION ||
                cache_config.type == CACHE_TYPE_UNIFIED) {
                l1_icaches_[cache_config.core] = cache;
            }
            if (cache_config.type == CACHE_TYPE_DATA ||
                cache_config.type == CACHE_TYPE_UNIFIED) {
                l1_dcaches_[cache_config.core] = cache;
            }
        }

        // Assign the pointer(s) to the LLC(s).
        if (cache_config.parent == CACHE_PARENT_MEMORY) {
            is_l1_or_llc = true;
            llcaches_[cache_name] = cache;
        }

        // Keep track of non-L1 and non-LLC caches.
        if (!is_l1_or_llc) {
            other_caches_[cache_name] = cache;
        }
    }
    if (knobs_.model_coherence && !snoop_filter_->init(snooped_caches_, snoop_id)) {
        ERRMSG("Usage error: failed to initialize snoop filter.\n");
        success_ = false;
        return;
    }
}

cache_simulator_t::~cache_simulator_t()
{
    for (auto &caches_it : all_caches_) {
        cache_t *cache = caches_it.second;
        delete cache->get_stats();
        delete cache->get_prefetcher();
        delete cache;
    }

    if (l1_icaches_ != NULL) {
        delete[] l1_icaches_;
    }
    if (l1_dcaches_ != NULL) {
        delete[] l1_dcaches_;
    }
    if (snooped_caches_ != NULL) {
        delete[] snooped_caches_;
    }
    if (snoop_filter_ != NULL) {
        delete snoop_filter_;
    }
}

uint64_t
cache_simulator_t::remaining_sim_refs() const
{
    return knobs_.sim_refs;
}

bool
cache_simulator_t::process_memref(const memref_t &memref)
{
    if (knobs_.skip_refs > 0) {
        knobs_.skip_refs--;
        return true;
    }

    // If no warmup is specified and we have simulated sim_refs then
    // we are done.
    if ((knobs_.warmup_refs == 0 && knobs_.warmup_fraction == 0.0) &&
        knobs_.sim_refs == 0)
        return true;

    // The references after warmup and simulated ones are dropped.
    if (check_warmed_up() && knobs_.sim_refs == 0)
        return true;

    // Both warmup and simulated references are simulated.

    if (!simulator_t::process_memref(memref))
        return false;

    if (memref.marker.type == TRACE_TYPE_MARKER) {
        // We ignore markers before we ask core_for_thread, to avoid asking
        // too early on a timestamp marker.
        if (knobs_.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: "
                      << "marker type " << memref.marker.marker_type << " value "
                      << memref.marker.marker_value << "\n";
        }
        return true;
    }

    // We use a static scheduling of threads to cores, as it is
    // not practical to measure which core each thread actually
    // ran on for each memref.
    int core;
    if (memref.data.tid == last_thread_)
        core = last_core_;
    else {
        core = core_for_thread(memref.data.tid);
        last_thread_ = memref.data.tid;
        last_core_ = core;
    }

    if (type_is_instr(memref.instr.type) ||
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR) {
        if (knobs_.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: "
                      << " @" << (void *)memref.instr.addr << " instr x"
                      << memref.instr.size << "\n";
        }
        l1_icaches_[core]->request(memref);
    } else if (memref.data.type == TRACE_TYPE_READ ||
               memref.data.type == TRACE_TYPE_WRITE ||
               // We may potentially handle prefetches differently.
               // TRACE_TYPE_PREFETCH_INSTR is handled above.
               type_is_prefetch(memref.data.type)) {
        if (knobs_.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: "
                      << " @" << (void *)memref.data.pc << " "
                      << trace_type_names[memref.data.type] << " "
                      << (void *)memref.data.addr << " x" << memref.data.size << "\n";
        }
        l1_dcaches_[core]->request(memref);
    } else if (memref.flush.type == TRACE_TYPE_INSTR_FLUSH) {
        if (knobs_.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: "
                      << " @" << (void *)memref.data.pc << " iflush "
                      << (void *)memref.data.addr << " x" << memref.data.size << "\n";
        }
        l1_icaches_[core]->flush(memref);
    } else if (memref.flush.type == TRACE_TYPE_DATA_FLUSH) {
        if (knobs_.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: "
                      << " @" << (void *)memref.data.pc << " dflush "
                      << (void *)memref.data.addr << " x" << memref.data.size << "\n";
        }
        l1_dcaches_[core]->flush(memref);
    } else if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
        handle_thread_exit(memref.exit.tid);
        last_thread_ = 0;
    } else if (memref.marker.type == TRACE_TYPE_INSTR_NO_FETCH) {
        // Just ignore.
        if (knobs_.verbose >= 3) {
            std::cerr << "::" << memref.data.pid << "." << memref.data.tid << ":: "
                      << " @" << (void *)memref.instr.addr << " non-fetched instr x"
                      << memref.instr.size << "\n";
        }
    } else {
        error_string_ = "Unhandled memref type " + std::to_string(memref.data.type);
        return false;
    }

    // reset cache stats when warming up is completed
    if (!is_warmed_up_ && check_warmed_up()) {
        for (auto &cache_it : all_caches_) {
            cache_t *cache = cache_it.second;
            cache->get_stats()->reset();
        }
        if (knobs_.verbose >= 1) {
            std::cerr << "Cache simulation warmed up\n";
        }
    } else {
        knobs_.sim_refs--;
    }

    return true;
}

// Return true if the number of warmup references have been executed or if
// specified fraction of the llcaches_ has been loaded. Also return true if the
// cache has already been warmed up. When there are multiple last level caches
// this function only returns true when all of them have been warmed up.
bool
cache_simulator_t::check_warmed_up()
{
    // If the cache has already been warmed up return true
    if (is_warmed_up_)
        return true;

    // If the warmup_fraction option is set then check if the last level has
    // loaded enough data to be warmed up.
    if (knobs_.warmup_fraction > 0.0) {
        is_warmed_up_ = true;
        for (auto &cache : llcaches_) {
            if (cache.second->get_loaded_fraction() < knobs_.warmup_fraction) {
                is_warmed_up_ = false;
                break;
            }
        }

        if (is_warmed_up_) {
            return true;
        }
    }

    // If warmup_refs is set then decrement and indicate warmup done when
    // counter hits zero.
    if (knobs_.warmup_refs > 0) {
        knobs_.warmup_refs--;
        if (knobs_.warmup_refs == 0) {
            is_warmed_up_ = true;
            return true;
        }
    }

    // If we reach here then warmup is not done.
    return false;
}

bool
cache_simulator_t::print_results()
{
    std::cerr << "Cache simulation results:\n";
    // Print core and associated L1 cache stats first.
    for (unsigned int i = 0; i < knobs_.num_cores; i++) {
        print_core(i);
        if (thread_ever_counts_[i] > 0) {
            if (l1_icaches_[i] != l1_dcaches_[i]) {
                std::cerr << "  L1I stats:" << std::endl;
                l1_icaches_[i]->get_stats()->print_stats("    ");
                std::cerr << "  L1D stats:" << std::endl;
                l1_dcaches_[i]->get_stats()->print_stats("    ");
            } else {
                std::cerr << "  unified L1 stats:" << std::endl;
                l1_icaches_[i]->get_stats()->print_stats("    ");
            }
        }
    }

    // Print non-L1, non-LLC cache stats.
    for (auto &caches_it : other_caches_) {
        std::cerr << caches_it.first << " stats:" << std::endl;
        caches_it.second->get_stats()->print_stats("    ");
    }

    // Print LLC stats.
    for (auto &caches_it : llcaches_) {
        std::cerr << caches_it.first << " stats:" << std::endl;
        caches_it.second->get_stats()->print_stats("    ");
    }

    if (knobs_.model_coherence) {
        snoop_filter_->print_stats();
    }

    return true;
}

cache_t *
cache_simulator_t::create_cache(const std::string &policy)
{
    if (policy == REPLACE_POLICY_NON_SPECIFIED || // default LRU
        policy == REPLACE_POLICY_LRU)             // set to LRU
        return new cache_lru_t;
    if (policy == REPLACE_POLICY_LFU) // set to LFU
        return new cache_t;
    if (policy == REPLACE_POLICY_FIFO) // set to FIFO
        return new cache_fifo_t;

    // undefined replacement policy
    ERRMSG("Usage error: undefined replacement policy. "
           "Please choose " REPLACE_POLICY_LRU " or " REPLACE_POLICY_LFU ".\n");
    return NULL;
}
