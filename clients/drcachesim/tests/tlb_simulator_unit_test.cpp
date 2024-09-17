/* **********************************************************
 * Copyright (c) 2024 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "tlb_simulator_unit_test.h"
#include "../simulator/tlb_simulator.h"
#include "../common/memref.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

class tlb_simulator_mock_t : public tlb_simulator_t {
public:
    tlb_simulator_mock_t(const tlb_simulator_knobs_t &knobs)
        : tlb_simulator_t(knobs) {};

    bool
    process_memref(const memref_t &memref)
    {
        // Process memref like tlb_simulator does.
        bool result = tlb_simulator_t::process_memref(memref);
        // Save address used by tlb_simulator, if any.
        if (!type_has_address(memref.data.type)) {
            return result;
        }
        const memref_t *simref = &memref;
        memref_t phys_memref;
        if (knobs_.use_physical) {
            phys_memref = memref2phys(memref);
            simref = &phys_memref;
        }
        addresses.insert(simref->data.addr);
        // Return the result of tlb_simulator.
        return result;
    };

    void
    set_knob_use_physical(bool set)
    {
        // XXX: We should really consider unifying the common knobs between simulator_t
        // and its derived classes like tlb_simulator_t.
        // Set tlb_simulator_t knob.
        knobs_.use_physical = set;
        // Set simulator_t knob.
        knob_use_physical_ = set;
    };

    std::unordered_set<addr_t> addresses;
};

static memref_t
generate_mem_ref(const addr_t addr, const addr_t pc)
{
    memref_t memref;
    memref.data.type = TRACE_TYPE_READ;
    memref.data.pid = 11111;
    memref.data.tid = 22222;
    memref.data.addr = addr;
    memref.data.size = 8;
    memref.data.pc = pc;
    return memref;
}

// Checks that the addresses the tlb simulator uses (in memrefs) are the same we expect
// in addresses.
static std::string
check_addresses(const std::vector<memref_t> &memrefs,
                const std::unordered_set<addr_t> &addresses,
                const std::string &v2p_file_path = "")
{
    tlb_simulator_knobs_t knobs;
    tlb_simulator_mock_t tlb_simulator_mock(knobs);

    if (!v2p_file_path.empty()) {
        tlb_simulator_mock.set_knob_use_physical(true);
        std::ifstream fin;
        fin.open(v2p_file_path);
        if (!fin.is_open())
            return "Failed to open the v2p file '" + v2p_file_path + "'\n";
        std::string error_str = tlb_simulator_mock.create_v2p_from_file(fin);
        fin.close();
        if (!error_str.empty())
            return "ERROR: v2p_reader failed with: " + error_str + "\n";
    }

    for (const memref_t &memref : memrefs)
        tlb_simulator_mock.process_memref(memref);

    if (addresses.size() != tlb_simulator_mock.addresses.size()) {
        return "ERROR: size mismatch. Expected " + std::to_string(addresses.size()) +
            " got " + std::to_string(tlb_simulator_mock.addresses.size()) + ".\n";
    }

    for (const addr_t addr : addresses) {
        if (tlb_simulator_mock.addresses.count(addr) == 0)
            return "ERROR: address " + std::to_string(addr) + " not found.\n";
    }

    return "";
}

static void
tlb_simulator_check_addresses(const std::string &testdir)
{
    // virtual_addresses and physical_addresses are taken from an X64 trace, hence we
    // enable this test only on X64 hosts.
#if defined(X86_64) || defined(AARCH64)
    const std::string v2p_file_path =
        testdir + "/drmemtrace.threadsig.aarch64.raw/v2p.textproto";
    const std::unordered_set<addr_t> virtual_addresses = { 0x0000fffffb73da60,
                                                           0x00000000004a7a78,
                                                           0x00000000004a5f20 };
    const std::unordered_set<addr_t> physical_addresses = { 0x000000000133da60,
                                                            0x00000000002a7a78,
                                                            0x00000000002a5f20 };
    std::vector<memref_t> memrefs;
    // We don't care about exact PC values.
    // Note: this will cause "Missing physical address marker for $PC" messages, which
    // we ignore.
    addr_t pc = 0;
    for (const addr_t &addr : virtual_addresses) {
        memrefs.push_back(generate_mem_ref(addr, pc));
        pc += 8;
    }

    // Check that the addresses the tlb_simulator operates with are the same virtual
    // addresses we created the memrefs with.
    std::string error_str = check_addresses(memrefs, virtual_addresses);
    if (!error_str.empty()) {
        std::cerr << "ERROR: tlb_simulator_unit_test with virtual_addresses failed with: "
                  << error_str << "\n";
        exit(1);
    }

    // Check that the addresses the tlb_simulator operates with are the same physical
    // addresses we expect.
    error_str = check_addresses(memrefs, physical_addresses, v2p_file_path);
    if (!error_str.empty()) {
        std::cerr
            << "ERROR: tlb_simulator_unit_test with physical_addresses failed with: "
            << error_str << "\n";
        exit(1);
    }
#endif
}

void
unit_test_tlb_simulator(const std::string &testdir)
{
    tlb_simulator_check_addresses(testdir);
}

} // namespace drmemtrace
} // namespace dynamorio
