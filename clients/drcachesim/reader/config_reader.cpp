/* **********************************************************
 * Copyright (c) 2018 Google, LLC  All rights reserved.
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
 * * Neither the name of Google, LLC nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config_reader.h"

#include <iostream>
using namespace std;

config_reader_t::config_reader_t()
{
    /* Empty. */
}

config_reader_t::~config_reader_t()
{
    fin.close();
}

bool
config_reader_t::configure(const string &config_file,
                           cache_simulator_knobs_t &knobs,
                           std::map<string, cache_params_t*> &caches)
{
    // String used to construct meaningful error messages.
    string error_msg;

    // Open the config file.
    fin.open(config_file);
    if (!fin.is_open()) {
        error_msg = "Failed to open the config file '" + config_file + "'\n";
        ERRMSG(error_msg.c_str());
        return false;
    }

    // Walk through the configuration file.
    while (!fin.eof()) {
        string param;
        fin >> ws >> param;

        if (param == "//") {
            // A comment.
            getline(fin, param);
        }
        else if (param == "num_cores") {
            // Number of cache cores.
            fin >> knobs.num_cores;
            if (knobs.num_cores == 0) {
                ERRMSG("Number of cores must be >0\n");
                return false;
            }
        }
        else if (param == "line_size") {
            // Cache line size in bytes.
            fin >> knobs.line_size;
            if (knobs.line_size == 0) {
                ERRMSG("Line size must be >0\n");
                return false;
            }
        }
        else if (param == "skip_refs") {
            // Number of references to skip.
            fin >> knobs.skip_refs;
        }
        else if (param == "warmup_refs") {
            // Number of references to use for caches warmup.
            fin >> knobs.warmup_refs;
        }
        else if (param == "warmup_fraction") {
            // Fraction of cache lines that must be filled to end the warmup.
            fin >> knobs.warmup_fraction;
            if (knobs.warmup_fraction < 0.0 || knobs.warmup_fraction > 1.0) {
                ERRMSG("Warmup fraction should be in [0.0, 1.0]\n");
                return false;
            }
        }
        else if (param == "sim_refs") {
            // Number of references to simulate.
            fin >> knobs.sim_refs;
        }
        else if (param == "cpu_scheduling") {
            // Whether to simulate CPU scheduling or not.
            string bool_val;
            fin >> bool_val;
            if (bool_val == "true" || bool_val == "True" ||
                bool_val == "TRUE") {
                knobs.cpu_scheduling = true;
            }
            else {
                knobs.cpu_scheduling = false;
            }
        }
        else if (param == "verbose") {
            // Verbose level.
            fin >> knobs.verbose;
        }
        else {
            // A cache unit.
            cache_params_t *cache = new cache_params_t;
            cache->name = param;
            if (!configure_cache(cache)) {
                return false;
            }
            caches[cache->name] = cache;
        }

        fin >> ws;
    }

    // Check cache configuration.
    return check_cache_config(knobs.num_cores, caches);
}

bool
config_reader_t::configure_cache(cache_params_t *cache)
{
    // String used to construct meaningful error messages.
    string error_msg;

    char c;
    fin >> ws >> c;
    if (c != '{') {
        ERRMSG("Expected '{' before cache params\n");
        return false;
    }

    while (!fin.eof()) {
        string param;
        fin >> ws >> param;

        if (param == "}") {
            return true;
        }
        else if (param == "//") {
            // A comment.
            getline(fin, param);
        }
        else if (param == "type") {
            // Cache type: instruction, data, or unified (default).
            fin >> cache->type;
            if (cache->type != "instruction" && cache->type != "data" &&
                cache->type != "unified") {
                error_msg = "Unknown cache type: " + cache->type + "\n";
                ERRMSG(error_msg.c_str());
                return false;
            }
        }
        else if (param == "core") {
            // CPU core this cache is associated with.
            fin >> cache->core;
        }
        else if (param == "size") {
            // Cache size in bytes.
            fin >> cache->size;
            if (cache->size <= 0 || !IS_POWER_OF_2(cache->size)) {
                error_msg = "Cache size (" + to_string(cache->size) +
                            ") must be >0 and a power of 2\n";
                ERRMSG(error_msg.c_str());
                return false;
            }
        }
        else if (param == "assoc") {
            // Cache associativity. Must be a power of 2.
            fin >> cache->assoc;
            if (cache->assoc <= 0 || !IS_POWER_OF_2(cache->assoc)) {
                error_msg = "Cache associativity (" + to_string(cache->assoc) +
                            ") must be >0 and a power of 2\n";
                ERRMSG(error_msg.c_str());
                return false;
            }
        }
        else if (param == "inclusive") {
            // Is the cache inclusive of its children.
            string bool_val;
            fin >> bool_val;
            if (bool_val == "true" || bool_val == "True" || bool_val == "TRUE") {
                cache->inclusive = true;
            }
        }
        else if (param == "parent") {
            // Name of the cache's parent. LLC's parent is main memory (mem).
            fin >> cache->parent;
        }
        else if (param == "rplc_policy") {
            // Cache replacement policy: LRU (default), LFU or FIFO.
            fin >> cache->rplc_policy;
            if (cache->rplc_policy != "LRU" && cache->rplc_policy != "LFU" &&
                cache->rplc_policy != "FIFO") {
                error_msg = "Unknown replacement policy: "
                            + cache->rplc_policy + "\n";
                ERRMSG(error_msg.c_str());
                return false;
            }
        }
        else if (param == "prefetcher") {
            // Type of prefetcher: nextline or none.
            fin >> cache->prefetcher;
            if (cache->prefetcher != "none" && cache->prefetcher != "nextline") {
                error_msg = "Unknown prefetcher type: "
                            + cache->prefetcher + "\n";
                ERRMSG(error_msg.c_str());
                return false;
            }
        }
        else if (param == "miss_file") {
            // Name of the file to use to dump cache misses info.
            fin >> cache->miss_file;
        }
        else {
            error_msg = "Unknown cache configuration setting '" + param + "'\n";
            ERRMSG(error_msg.c_str());
            return false;
        }
        fin >> ws;
    }

    ERRMSG("Expected '}' at the end of cache params\n");
    return false;
}

bool
config_reader_t::check_cache_config(unsigned int num_cores,
                                    std::map<string, cache_params_t*> &caches_map)
{
    // String used to construct meaningful error messages.
    string error_msg;

    int *core_inst_caches = new int[num_cores];
    int *core_data_caches = new int[num_cores];
    for (int i = 0; i < num_cores; i++) {
        core_inst_caches[i] = 0;
        core_data_caches[i] = 0;
    }

    for (auto &cache_map : caches_map) {
        string cache_name = cache_map.first;
        auto &cache = cache_map.second;

        // Associate a cache to a core.
        if (cache->core >= 0) {
            if (cache->core >= num_cores) {
                error_msg = "Cache " + cache_name + " is associated with core "
                            + to_string(cache->core) +
                            " which does not exist\n";
                ERRMSG(error_msg.c_str());
                return false;
            }
            if (cache->type == "instruction" || cache->type == "unified") {
               core_inst_caches[cache->core]++;
            }
            if (cache->type == "data" || cache->type == "unified") {
               core_data_caches[cache->core]++;
            }
        }

        // Associate a cache with its parent and children caches.
        if (cache->parent != "mem") {
            auto parent_it = caches_map.find(cache->parent);
            if (parent_it != caches_map.end()) {
                auto &parent = parent_it->second;
                // Check that the cache types are compatible.
                if (parent->type != "unified" &&
                    cache->type != parent->type) {
                    error_msg = "Cache " + cache_name +
                                " and its parent have incompatible types\n";
                    ERRMSG(error_msg.c_str());
                    return false;
                }

                // Add the cache to its parents children.
                parent->children.push_back(cache_name);
            }
            else {
                error_msg = "Cache " + cache_name + " has a listed parent "
                            + cache->parent + " that does not exist\n";
                ERRMSG(error_msg.c_str());
                return false;
            }

            // Check for cycles between the cache and its parent.
            string parent = cache->parent;
            while (parent != "mem") {
                if (parent == cache_name) {
                    error_msg = "Cache " + cache_name + " and its parent " +
                                cache->parent + " have a cyclic reference\n";
                    ERRMSG(error_msg.c_str());
                    return false;
                }
                parent = caches_map.find(parent)->second->parent;
            }
        }
    }

    // Check that each core has exactly one instruction and one data caches or
    // exactly one unified cache.
    for (int i = 0; i < num_cores; i++) {
        if (core_inst_caches[i] != 1) {
            error_msg = "Core " + to_string(i) + " has " +
                        to_string(core_inst_caches[i]) +
                        " instruction caches. Must have exactly 1\n";
            ERRMSG(error_msg.c_str());
            return false;
        }
        if (core_data_caches[i] != 1) {
            error_msg = "Core " + to_string(i) + " has " +
                        to_string(core_data_caches[i]) +
                        " data caches. Must have exactly 1\n";
            ERRMSG(error_msg.c_str());
            return false;
        }
    }

    return true;
}
