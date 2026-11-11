/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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

#include "v2p_tracker.h"

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <algorithm>
#    include <ctype.h>
#    include <dirent.h>
#    include <limits>
#    include <errno.h>
#    include <fcntl.h>
#    include <inttypes.h>
#    include <poll.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <sys/syscall.h>
#    include <sys/utsname.h>
#    include <sys/wait.h>
#    include <time.h>
#    include <unistd.h>

#    ifndef __WCLONE
#        define __WCLONE 0x80000000
#    endif

namespace dynamorio {
namespace drmemtrace {
namespace {

/* -------------------------------------------------------------------------
 * Pagemap, Tracefs Control & Output Record Helpers
 * ------------------------------------------------------------------------- */

// Offset between Unix epoch (1970) and Windows FILETIME epoch (1601) used by DR
// raw trace timestamps (`instru_t::get_timestamp()`).
constexpr uint64_t kDrUtcToEpochMicros = 11644473600ULL * 1000000ULL;

// `/proc/<pid>/pagemap` entry bits and `/proc/kpageflags` (`KPF_THP`) bit mask.
constexpr uint64_t kPagemapPresentBit = 1ULL << 63;
constexpr uint64_t kPagemapPfnMask = (1ULL << 55) - 1;
constexpr uint64_t kKpfHuge = 1ULL << 17;
constexpr uint64_t kKpfThp = 1ULL << 22;
constexpr uint64_t kKpfZeroPage = 1ULL << 24;

// Saturating unsigned 64-bit addition used to guard virtual range end calculations.
uint64_t
sat_add(uint64_t base, uint64_t delta)
{
    return delta > (std::numeric_limits<uint64_t>::max)() - base
        ? (std::numeric_limits<uint64_t>::max)()
        : base + delta;
}

// Writes a 32-byte `v2p_record_t` entry to `out_fd`, retrying on `EINTR` or short writes
// so fixed-stride binary records are never misaligned.
void
write_record(int out_fd, uint64_t ts, uint64_t va, uint64_t pa, uint32_t pages,
             uint32_t type)
{
    if (out_fd < 0)
        return;
    v2p_record_t rec = { ts, va, pa, pages, type };
    const char *ptr = reinterpret_cast<const char *>(&rec);
    for (size_t rem = sizeof(rec); rem > 0;) {
        ssize_t n = write(out_fd, ptr, rem);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        ptr += n;
        rem -= static_cast<size_t>(n);
    }
}

// Allocates one temporary 4 KB anonymous page via raw `SYS_mmap` (bypassing DR's heap
// and syscall wrappers); returns `nullptr` on raw kernel `-errno` (`-4095..-1`).
void *
mmap_temp_page(int prot)
{
    long ret =
        syscall(SYS_mmap, nullptr, kPageSize4K, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ret <= 0 && ret >= -4095L) ? nullptr : reinterpret_cast<void *>(ret);
}

// Reads `/proc/<pid>/pagemap` for `vaddr` and returns its resident PFN, or 0 if absent.
uint64_t
lookup_pagemap_pfn(pid_t pid, uint64_t vaddr)
{
    int fd =
        open(("/proc/" + std::to_string(pid) + "/pagemap").c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;
    uint64_t entry = 0;
    off_t off = static_cast<off_t>((vaddr / kPageSize4K) * sizeof(uint64_t));
    bool ok = pread(fd, &entry, sizeof(entry), off) == sizeof(entry) &&
        (entry & kPagemapPresentBit) != 0;
    close(fd);
    return ok ? (entry & kPagemapPfnMask) : 0;
}

// Faults a temporary read-only anonymous page to discover the PFN of the kernel's
// shared 4 KB zero page from `/proc/self/pagemap`.
uint64_t
calibrate_zero_pfn()
{
    void *page = mmap_temp_page(PROT_READ);
    if (page == nullptr)
        return 0;
    volatile uint8_t sink = *static_cast<volatile const uint8_t *>(page);
    (void)sink;
    uint64_t zero_pfn = lookup_pagemap_pfn(getpid(), reinterpret_cast<uint64_t>(page));
    syscall(SYS_munmap, page, kPageSize4K);
    return zero_pfn;
}

} // namespace

/* -------------------------------------------------------------------------
 * Tracker Lifecycle & Tracefs Instance Setup
 * ------------------------------------------------------------------------- */

// Resets all per-session tracking state, shadow mappings, and pending hook tables.
void
v2p_tracker_t::reset_session_state(pid_t pid)
{
    owner_pid_ = getpid();
    target_pid_ = pid;
    reader_tid_.store(0, std::memory_order_relaxed);
    clock_offset_us_ = 0;
    vmemmap_base_ = 0;
    zero_pfn_ = 0;
    tracked_mm_ = 0;
    exec_saved_offset_ = -1;
    kernel_tracing_active_ = false;
    suspended_for_exec_ = false;
    pipe_remainder_.clear();
    registered_probes_.clear();
    clear_shadow_tables();
    hooks_.clear();
}

// Detaches inherited descriptors in a `fork()` child without tearing down parent probes,
// optionally adding the child PID to `set_event_notrace_pid` before starting its tracker.
void
v2p_tracker_t::detach_forked_child(bool exclude_child_pid)
{
    if (exclude_child_pid && !instance_dir_.empty()) {
        v2p_write_tracefs_file(instance_dir_ + "/set_event_notrace_pid",
                               std::to_string(getpid()) + "\n", /*append=*/true);
    }
    if (trace_pipe_fd_ >= 0)
        close(trace_pipe_fd_);
    if (out_fd_ >= 0)
        close(out_fd_);
    trace_pipe_fd_ = -1;
    out_fd_ = -1;
    instance_dir_.clear();
    reset_session_state(0);
    thread_running_.store(false, std::memory_order_release);
    running_.store(false, std::memory_order_release);
}

// Cleans up any leftover `drmemtrace_v2p_<pid>` instance or `_<pid>` kprobes if an
// earlier traced process with the same recycled PID exited via `SIGKILL`.
void
v2p_tracker_t::pre_clean_stale_tracefs() const
{
    const std::string sfx = "_" + std::to_string(target_pid_);
    const std::string inst = tracefs_root_ + "/instances/drmemtrace_v2p" + sfx;
    struct stat st = {};
    if (stat(inst.c_str(), &st) == 0) {
        v2p_write_tracefs_file(inst + "/tracing_on", "0");
        v2p_write_tracefs_file(inst + "/events/enable", "0");
        rmdir(inst.c_str());
    }

    // Only touch global `kprobe_events` if a stale `_<pid>` probe actually exists.
    std::string kp_path = tracefs_root_ + "/kprobe_events";
    std::string kp_text;
    if (!v2p_read_file_bytes(kp_path, kp_text) ||
        kp_text.find(sfx + " ") == std::string::npos) {
        return;
    }
    for (const v2p_probe_spec_t &spec : plan_.probes) {
        v2p_write_tracefs_file(kp_path, "-:kprobes/" + spec.base_name + sfx + "\n",
                               /*append=*/true);
    }
}

bool
v2p_tracker_t::start(pid_t pid, const std::string &output_path, uint32_t buffer_size_kb,
                     bool ignore_missing_hooks)
{
    // If `v2p_tracker` was already active before `fork()`, drop inherited parent FDs
    // without touching the parent's tracefs instance; otherwise stop the old session.
    if (running_.load(std::memory_order_acquire)) {
        if (owner_pid_ != 0 && getpid() != owner_pid_)
            detach_forked_child(/*exclude_child_pid=*/true);
        else
            stop();
    }

    reset_session_state(pid);
    buffer_size_kb_ = buffer_size_kb;
    ignore_missing_hooks_ = ignore_missing_hooks;

    // Require 4 KB base pages and at least Linux version 5.4.
    struct utsname uts = {};
    if (dr_page_size() != kPageSize4K || uname(&uts) != 0 ||
        sscanf(uts.release, "%d.%d", &kernel_major_, &kernel_minor_) != 2 ||
        (kernel_major_ < 5 || (kernel_major_ == 5 && kernel_minor_ < 4))) {
        return false;
    }

    // Locate the mounted tracefs root directory (`instances` support required).
    struct stat st = {};
    if (stat("/sys/kernel/tracing/instances", &st) == 0)
        tracefs_root_ = "/sys/kernel/tracing";
    else if (stat("/sys/kernel/debug/tracing/instances", &st) == 0)
        tracefs_root_ = "/sys/kernel/debug/tracing";
    else
        return false;

    // Resolve all kernel kprobe definitions, struct field offsets, and enum bits via
    // BTF, emitting `(CRITICAL)` errors and per-hook uncovered-transition warnings.
    std::string plan_error;
    const bool plan_ok =
        v2p_check_kernel_support("/sys/kernel/btf/vmlinux", tracefs_root_, plan_,
                                 plan_error, ignore_missing_hooks_);
    for (const std::string &err : plan_.critical_errors)
        dr_fprintf(STDERR, "ERROR: drmemtrace v2p_tracker: %s\n", err.c_str());
    for (const std::string &warn : plan_.warnings)
        dr_fprintf(STDERR, "WARNING: drmemtrace v2p_tracker: %s\n", warn.c_str());
    if (!plan_ok) {
        if (plan_.critical_errors.empty()) {
            dr_fprintf(STDERR,
                       "ERROR: drmemtrace v2p_tracker: aborting due to missing tracefs "
                       "hooks; pass -v2p_tracefs_ignore_missing_hooks to proceed with "
                       "partial V2P coverage\n");
        }
        return false;
    }

    // Open `raw/v2p.bin`, discover the shared 4 KB zero-page PFN, and start tracing.
    out_fd_ = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out_fd_ < 0)
        return false;
    zero_pfn_ = calibrate_zero_pfn();
    if (!setup_tracefs_and_reader(/*is_initial_start=*/true)) {
        close(out_fd_);
        out_fd_ = -1;
        return false;
    }
    running_.store(true, std::memory_order_release);
    return true;
}

// Creates the isolated tracefs instance, registers and filters all probes, and starts
// the background `trace_pipe` reader thread. On `start()` (`is_initial_start == true`),
// also calibrates kernel clock/pointer constants and snapshots `/proc/<pid>/pagemap`
// (skipped when restarting tracefs after a failed `execve`, which reuses them).
bool
v2p_tracker_t::setup_tracefs_and_reader(bool is_initial_start)
{
    auto fail = [this]() {
        cleanup_kernel_tracing();
        return false;
    };

    const std::string sfx = "_" + std::to_string(target_pid_);
    pre_clean_stale_tracefs();
    instance_dir_ = tracefs_root_ + "/instances/drmemtrace_v2p" + sfx;
    registered_probes_.clear();
    pipe_remainder_.clear();
    kernel_tracing_active_ = true;
    if (mkdir(instance_dir_.c_str(), 0755) != 0 && errno != EEXIST)
        return fail();

    // Configure the instance ring buffer: use `CLOCK_TAI` on Linux 5.19+ (unaffected by
    // NTP slew) or `CLOCK_MONOTONIC`, and track child threads via `event-fork`.
    // Scope events by matching the traced process's kernel address space (`mm_struct *`)
    // on each probe rather than `set_event_pid`: `set_event_pid` misses background
    // `khugepaged` kernel-thread collapses because `khugepaged` runs under its own PID,
    // whereas matching `mm == tracked_mm_` records `khugepaged` activity on our process
    // while still ignoring all other processes.
    const bool use_tai = kernel_major_ > 5 || (kernel_major_ == 5 && kernel_minor_ >= 19);
    if (!v2p_write_tracefs_file(instance_dir_ + "/trace_clock",
                                use_tai ? "tai" : "mono") ||
        (buffer_size_kb_ > 0 &&
         !v2p_write_tracefs_file(instance_dir_ + "/buffer_size_kb",
                                 std::to_string(buffer_size_kb_))) ||
        !v2p_write_tracefs_file(instance_dir_ + "/options/event-fork", "1")) {
        return fail();
    }

    // Measure the offset from tracefs timestamps to DR's UTC/FILETIME microsecond clock.
    if (is_initial_start) {
        struct timespec clk_ts = {}, real_ts = {};
        if (clock_gettime(use_tai ? CLOCK_TAI : CLOCK_MONOTONIC, &clk_ts) != 0 ||
            clock_gettime(CLOCK_REALTIME, &real_ts) != 0) {
            return fail();
        }
        clock_offset_us_ = (real_ts.tv_sec * 1000000ULL + real_ts.tv_nsec / 1000ULL +
                            kDrUtcToEpochMicros) -
            (clk_ts.tv_sec * 1000000ULL + clk_ts.tv_nsec / 1000ULL);
    }

    // Register all BTF-validated kprobes and kretprobes, reporting every failure at once.
    const std::string kp = tracefs_root_ + "/kprobe_events";
    std::vector<std::pair<v2p_probe_spec_t, const char *>> failed_regs;
    for (const v2p_probe_spec_t &spec : plan_.probes) {
        std::string name = spec.base_name + sfx;
        std::string cmd = (spec.is_return ? "r4096:kprobes/" : "p:kprobes/") + name +
            " " + spec.definition + "\n";
        if (!v2p_write_tracefs_file(kp, cmd, /*append=*/true))
            failed_regs.push_back({ spec, "failed to register kprobe" });
        else
            registered_probes_.push_back(name);
    }
    if (!failed_regs.empty() && !handle_runtime_hook_failures(failed_regs))
        return fail();

    trace_pipe_fd_ =
        open((instance_dir_ + "/trace_pipe").c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (trace_pipe_fd_ < 0)
        return fail();
    if (is_initial_start && !calibrate_live_vmemmap_and_mm()) {
        dr_fprintf(STDERR,
                   "ERROR: drmemtrace v2p_tracker: (CRITICAL) vmemmap calibration "
                   "failed (requires CAP_SYS_ADMIN access to /proc/self/pagemap) - "
                   "cannot translate kernel page pointers to physical addresses\n");
        return fail();
    }

    // Install kernel-side `mm == tracked_mm_` filters, enable `sched_process_exec` and
    // all registered probes, and turn on ring-buffer recording BEFORE scanning
    // `/proc/<pid>/pagemap` so any concurrent fault or unmap during the scan is captured.
    std::string exec_warn;
    const std::string exec_flt = "pid == " + std::to_string(target_pid_);
    if (plan_.has_exec_tracepoint &&
        !set_event("sched/sched_process_exec", /*enable=*/true, &exec_flt)) {
        plan_.has_exec_tracepoint = false;
        exec_warn = "sched/sched_process_exec (failed to enable tracepoint) - "
                    "transitions uncovered: process address-space replacement on execve, "
                    "including teardown of prior mappings and initial snapshot of the "
                    "new executable";
    }
    std::vector<std::pair<v2p_probe_spec_t, const char *>> failed_enables;
    for (const v2p_probe_spec_t &spec : plan_.probes) {
        const std::string expr = probe_filter_expr(spec);
        if (!set_event("kprobes/" + spec.base_name + sfx, /*enable=*/true, &expr))
            failed_enables.push_back({ spec, "failed to filter or enable kprobe" });
    }
    if ((!failed_enables.empty() || !exec_warn.empty()) &&
        !handle_runtime_hook_failures(failed_enables, exec_warn)) {
        return fail();
    }
    if (!v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "1"))
        return fail();

    if (is_initial_start)
        snapshot_pagemap(/*timestamp_us=*/0);

    // Spawn the background client reader thread to drain `trace_pipe_fd_` continuously.
    reader_tid_.store(0, std::memory_order_relaxed);
    stop_requested_.store(false, std::memory_order_relaxed);
    thread_exited_.store(false, std::memory_order_relaxed);
    thread_running_.store(true, std::memory_order_release);
    if (!dr_create_client_thread(&v2p_tracker_t::reader_thread_entry, this)) {
        thread_running_.store(false, std::memory_order_release);
        return fail();
    }
    return true;
}

// Enables or disables `events/<rel_dir>` in the tracefs instance, updating `filter`
// while the event is disabled so an unfiltered window never opens.
bool
v2p_tracker_t::set_event(const std::string &rel_dir, bool enable,
                         const std::string *filter) const
{
    const std::string base = instance_dir_ + "/events/" + rel_dir;
    bool ok = true;
    if (!enable)
        ok = v2p_write_tracefs_file(base + "/enable", "0") && ok;
    if (filter != nullptr)
        ok = v2p_write_tracefs_file(base + "/filter", *filter) && ok;
    if (enable)
        ok = v2p_write_tracefs_file(base + "/enable", "1") && ok;
    return ok;
}

// Builds the tracefs filter expression (`mm == tracked_mm_` plus any static filter) for
// `spec`; in tracefs, `"0"` clears any active filter until `tracked_mm_` is learned.
std::string
v2p_tracker_t::probe_filter_expr(const v2p_probe_spec_t &spec) const
{
    if (spec.filter_by_mm && tracked_mm_ != 0) {
        char mm_hex[32];
        snprintf(mm_hex, sizeof(mm_hex), "0x%" PRIx64, tracked_mm_);
        return std::string("mm == ") + mm_hex +
            (spec.static_filter.empty() ? "" : " && " + spec.static_filter);
    }
    return spec.static_filter.empty() ? "0" : spec.static_filter;
}

// Installs kernel-side event filters (`mm == tracked_mm_` plus any static probe filter)
// so non-target processes do not consume tracefs ring-buffer space.
bool
v2p_tracker_t::apply_probe_filters() const
{
    if (!kernel_tracing_active_)
        return true;
    const std::string sfx = "_" + std::to_string(target_pid_);
    for (const v2p_probe_spec_t &spec : plan_.probes) {
        const std::string expr = probe_filter_expr(spec);
        if (!v2p_write_tracefs_file(instance_dir_ + "/events/kprobes/" + spec.base_name +
                                        sfx + "/filter",
                                    expr)) {
            return false;
        }
    }
    return true;
}

// Reports runtime `kprobe_events` / filter / enable failures with uncovered-transition
// diagnostics, prunes broken hook groups (and any companion probes) so half-registered
// entry/return pairs never run, and aborts unless `ignore_missing_hooks_` is set and no
// critical hook failed.
bool
v2p_tracker_t::handle_runtime_hook_failures(
    const std::vector<std::pair<v2p_probe_spec_t, const char *>> &failed_specs,
    const std::string &extra_warning)
{
    std::set<std::string> failed_hooks;
    bool had_critical = false;
    if (!extra_warning.empty()) {
        dr_fprintf(STDERR, "WARNING: drmemtrace v2p_tracker: %s\n",
                   extra_warning.c_str());
    }
    for (const auto &entry : failed_specs) {
        const v2p_probe_spec_t &spec = entry.first;
        const std::string key = spec.hook_name.empty() ? spec.base_name : spec.hook_name;
        if (!failed_hooks.insert(key).second)
            continue;
        std::string msg = key + " (" + entry.second + ")";
        if (!spec.uncovered_transitions.empty())
            msg += " - transitions uncovered: " + spec.uncovered_transitions;
        if (spec.is_critical) {
            had_critical = true;
            dr_fprintf(STDERR, "ERROR: drmemtrace v2p_tracker: (CRITICAL) %s\n",
                       msg.c_str());
        } else {
            dr_fprintf(STDERR, "WARNING: drmemtrace v2p_tracker: %s\n", msg.c_str());
        }
    }
    if (had_critical || !ignore_missing_hooks_) {
        if (!had_critical) {
            dr_fprintf(STDERR,
                       "ERROR: drmemtrace v2p_tracker: aborting due to missing tracefs "
                       "hooks; pass -v2p_tracefs_ignore_missing_hooks to proceed with "
                       "partial V2P coverage\n");
        }
        return false;
    }

    // Unregister any sibling or companion probes belonging to a failed hook group.
    std::set<std::string> drop_bases;
    if (failed_hooks.count("handle_mm_fault") > 0)
        drop_bases.insert("huge_zero_out");
    if (failed_hooks.count("folio_add_file_rmap_pmd") > 0 ||
        failed_hooks.count("page_add_file_rmap") > 0) {
        drop_bases.insert("collapse_in");
        drop_bases.insert("collapse_out");
    }
    if (failed_hooks.count("hugetlb_fault") > 0) {
        drop_bases.insert("hugetlb_anon");
        drop_bases.insert("hugetlb_unlock");
    }
    const std::string sfx = "_" + std::to_string(target_pid_);
    std::vector<v2p_probe_spec_t> healthy_probes;
    for (const v2p_probe_spec_t &spec : plan_.probes) {
        const std::string key = spec.hook_name.empty() ? spec.base_name : spec.hook_name;
        if (failed_hooks.count(key) == 0 && drop_bases.count(spec.base_name) == 0) {
            healthy_probes.push_back(spec);
            continue;
        }
        const std::string full = spec.base_name + sfx;
        set_event("kprobes/" + full, /*enable=*/false);
        auto rit = std::find(registered_probes_.begin(), registered_probes_.end(), full);
        if (rit != registered_probes_.end()) {
            v2p_write_tracefs_file(tracefs_root_ + "/kprobe_events",
                                   "-:kprobes/" + full + "\n", true);
            registered_probes_.erase(rit);
        }
    }
    plan_.probes = std::move(healthy_probes);
    return true;
}

// Calibrates `vmemmap_base_` and `tracked_mm_` before live tracing starts:
//   - Why `vmemmap_base_` is needed: hooked kernel MM functions never pass physical RAM
//     addresses; they pass a kernel virtual `struct page *` / `struct folio *` pointer
//     into Linux's flat array of 64-byte page descriptors indexed by physical frame
//     number (`page_ptr = vmemmap_base + pfn * 64`). Because KASLR randomizes where that
//     array starts on every boot, we must measure `vmemmap_base_` at runtime to convert
//     any hooked page pointer into a physical address.
//   - How we measure it: fault one temporary 4 KB page on this thread to observe both
//     its kernel `page=` pointer (from `anon_new`) and its physical frame number (from
//     `/proc/self/pagemap`), giving `vmemmap_base_ = page_ptr - pfn * 64` (plus `mm=`
//     for kernel-side `mm_struct` filtering).
bool
v2p_tracker_t::calibrate_live_vmemmap_and_mm()
{
    const std::string anon_ev = "kprobes/anon_new_" + std::to_string(target_pid_);
    const std::string tid_flt =
        "common_pid == " + std::to_string(static_cast<long>(syscall(SYS_gettid)));
    if (!set_event(anon_ev, /*enable=*/true, &tid_flt))
        return false;
    if (!v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "1")) {
        set_event(anon_ev, /*enable=*/false);
        return false;
    }

    // Trigger a single 4 KB write fault, freeze `tracing_on` immediately, and look up
    // the resulting PFN in `/proc/self/pagemap` before unmapping the temporary page.
    void *cal_page = mmap_temp_page(PROT_READ | PROT_WRITE);
    uint64_t cal_addr = reinterpret_cast<uint64_t>(cal_page);
    uint64_t cal_pfn = 0;
    if (cal_page != nullptr) {
        *static_cast<volatile uint8_t *>(cal_page) = 1;
        v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "0");
        cal_pfn = lookup_pagemap_pfn(getpid(), cal_addr);
        syscall(SYS_munmap, cal_page, kPageSize4K);
    } else {
        v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "0");
    }
    set_event(anon_ev, /*enable=*/false);

    // Drain `trace_pipe` directly so the temporary calibration page is not recorded in
    // `v2p.bin`; `set_event` replaces `anon_ev`'s filter immediately after.
    char chunk[4096];
    std::string buf;
    for (ssize_t n = 0; (n = read(trace_pipe_fd_, chunk, sizeof(chunk))) > 0;)
        buf.append(chunk, static_cast<size_t>(n));

    for (size_t pos = 0; pos < buf.size();) {
        size_t nl = buf.find('\n', pos);
        size_t end = (nl == std::string::npos) ? buf.size() : nl;
        std::string line = buf.substr(pos, end - pos);
        pos = (nl == std::string::npos) ? buf.size() : nl + 1;

        size_t colon = line.find(": ");
        if (colon == std::string::npos || line.compare(colon + 2, 9, "anon_new_") != 0)
            continue;
        size_t ev = colon + 2;
        uint64_t page = 0, mm = 0, va = 0;
        if (vmemmap_base_ == 0 && cal_pfn != 0 &&
            v2p_extract_u64_field(line, "vaddr=", va, 16, ev) &&
            (va & ~(kPageSize4K - 1)) == cal_addr &&
            v2p_extract_u64_field(line, "page=", page, 16, ev) && (page >> 63) != 0 &&
            page > (cal_pfn << kStructPageShift)) {
            uint64_t base = page - (cal_pfn << kStructPageShift);
            if ((base >> 63) != 0 && (base & ((1ULL << kStructPageShift) - 1)) == 0)
                vmemmap_base_ = base;
        }
        if (target_pid_ == getpid() && tracked_mm_ == 0 &&
            v2p_extract_u64_field(line, "mm=", mm, 16, ev) && mm != 0) {
            tracked_mm_ = mm;
        }
    }
    return vmemmap_base_ != 0;
}

// Snapshots already-resident 4 KB, 2 MB THP, and `hugetlbfs` mappings from
// `/proc/<pid>/smaps` (or `maps`), `/proc/<pid>/pagemap`, and `/proc/kpageflags`.
void
v2p_tracker_t::snapshot_pagemap(uint64_t timestamp_us)
{
    const std::string proc_dir = "/proc/" + std::to_string(target_pid_);
    FILE *maps = fopen((proc_dir + "/smaps").c_str(), "r");
    const bool have_smaps = (maps != nullptr);
    if (!have_smaps)
        maps = fopen((proc_dir + "/maps").c_str(), "r");
    int pm_fd = open((proc_dir + "/pagemap").c_str(), O_RDONLY | O_CLOEXEC);
    int kpf_fd = open("/proc/kpageflags", O_RDONLY | O_CLOEXEC);

    // Step 1: Parse user-space VMA ranges, `KernelPageSize:` (`>= 64 kB` on `hugetlbfs`
    // VMAs), and PMD-mapped THP counters (`AnonHugePages:`, `ShmemPmdMapped:`,
    // `FilePmdMapped:`) from `smaps`.
    struct vma_desc_t {
        uint64_t start = 0;
        uint64_t end = 0;
        uint64_t page_bytes = kPageSize4K;
        bool has_pmd_thp = false;
    };
    std::vector<vma_desc_t> vmas;
    char line[512];
    bool in_user_vma = false;
    while (maps != nullptr && pm_fd >= 0 && fgets(line, sizeof(line), maps) != nullptr) {
        uint64_t s = 0, e = 0, kb = 0;
        if (isxdigit(static_cast<unsigned char>(line[0])) &&
            sscanf(line, "%" SCNx64 "-%" SCNx64, &s, &e) == 2) {
            // Skip kernel-space pseudo-VMAs such as `[vsyscall]` (`>= 0xffff8000...`).
            in_user_vma = (s < 0xffff800000000000ULL && e > s);
            if (in_user_vma)
                vmas.push_back({ s, e, kPageSize4K, !have_smaps });
        } else if (!in_user_vma) {
            continue;
        } else if (strncmp(line, "KernelPageSize:", 15) == 0 &&
                   sscanf(line + 15, "%" SCNu64, &kb) == 1 && kb >= 64 &&
                   (kb & (kb - 1)) == 0 &&
                   kb <= (std::numeric_limits<uint32_t>::max)() * 4ULL) {
            vmas.back().page_bytes = kb * 1024ULL;
        } else if ((strncmp(line, "AnonHugePages:", 14) == 0 ||
                    strncmp(line, "ShmemPmdMapped:", 15) == 0 ||
                    strncmp(line, "FilePmdMapped:", 14) == 0) &&
                   sscanf(strchr(line, ':') + 1, "%" SCNu64, &kb) == 1 && kb > 0) {
            vmas.back().has_pmd_thp = true;
        }
    }

    // Step 2: Query `/proc/<pid>/pagemap` across each user-space VMA.
    uint64_t entries[kPagesPer2M];
    for (const vma_desc_t &vma : vmas) {
        // Explicit `hugetlbfs` VMAs (`KernelPageSize >= 64 kB`): step one hugepage at
        // a time when `hugetlbfs` hooks are active, or skip when `CONFIG_HUGETLB_PAGE=n`.
        if (vma.page_bytes > kPageSize4K) {
            if (!plan_.has_hugetlb)
                continue;
            auto hpage_pages = static_cast<uint32_t>(vma.page_bytes / kPageSize4K);
            for (uint64_t va = vma.start; va + vma.page_bytes <= vma.end;
                 va += vma.page_bytes) {
                uint64_t entry = 0;
                off_t off = static_cast<off_t>((va / kPageSize4K) * sizeof(uint64_t));
                if (pread(pm_fd, &entry, sizeof(entry), off) == sizeof(entry) &&
                    (entry & kPagemapPresentBit) != 0 && (entry & kPagemapPfnMask) != 0) {
                    bind_pages(timestamp_us, va, (entry & kPagemapPfnMask) * kPageSize4K,
                               hpage_pages, /*is_huge_2m=*/false);
                }
            }
            continue;
        }

        // Standard 4 KB / THP VMAs: read `pagemap` in up to 2 MB-aligned batches so
        // 2 MB THP PMDs are recognized before falling back to per-4 KB PTE bindings.
        for (uint64_t va = vma.start; va < vma.end;) {
            uint64_t batch_end =
                std::min(vma.end, (va + kPageSize2M) & ~(kPageSize2M - 1));
            ssize_t bytes =
                pread(pm_fd, entries, ((batch_end - va) / kPageSize4K) * sizeof(uint64_t),
                      static_cast<off_t>((va / kPageSize4K) * sizeof(uint64_t)));
            if (bytes <= 0)
                break;
            size_t n = static_cast<size_t>(bytes) / sizeof(uint64_t);

            // Check whether this batch is a 2 MB-aligned run of 512 contiguous PFNs.
            uint64_t base_pfn = entries[0] & kPagemapPfnMask;
            uint64_t kpf = 0;
            bool is_2m_contig = n == kPagesPer2M && (va & (kPageSize2M - 1)) == 0 &&
                (entries[0] & kPagemapPresentBit) != 0 && base_pfn != 0 &&
                (base_pfn & (kPagesPer2M - 1)) == 0;
            for (size_t i = 1; is_2m_contig && i < kPagesPer2M; ++i) {
                is_2m_contig = (entries[i] & kPagemapPresentBit) != 0 &&
                    (entries[i] & kPagemapPfnMask) == base_pfn + i;
            }
            if (is_2m_contig && kpf_fd >= 0 &&
                pread(kpf_fd, &kpf, sizeof(kpf),
                      static_cast<off_t>(base_pfn * sizeof(uint64_t))) == sizeof(kpf)) {
                if ((kpf & kKpfHuge) != 0) {
                    if (plan_.has_hugetlb) {
                        bind_pages(timestamp_us, va, base_pfn * kPageSize4K, kPagesPer2M,
                                   false);
                    }
                    va += kPageSize2M;
                    continue;
                }
                bool is_huge_zero = (kpf & kKpfZeroPage) != 0;
                if ((kpf & kKpfThp) != 0 && (is_huge_zero || vma.has_pmd_thp)) {
                    bind_pages(timestamp_us, va, base_pfn * kPageSize4K, kPagesPer2M,
                               /*is_huge_2m=*/true, is_huge_zero);
                    va += kPageSize2M;
                    continue;
                }
            }

            // Otherwise bind each resident 4 KB page in the batch individually.
            for (size_t i = 0; i < n; ++i) {
                uint64_t pfn = entries[i] & kPagemapPfnMask;
                if ((entries[i] & kPagemapPresentBit) != 0 && pfn != 0)
                    bind_pages(timestamp_us, va + i * kPageSize4K, pfn * kPageSize4K, 1);
            }
            va += n * kPageSize4K;
        }
    }

    if (kpf_fd >= 0)
        close(kpf_fd);
    if (pm_fd >= 0)
        close(pm_fd);
    if (maps != nullptr)
        fclose(maps);
}

/* -------------------------------------------------------------------------
 * Shadow Page Table Operations & Teardown
 * ------------------------------------------------------------------------- */

// Converts a kernel `struct page *` or `struct folio *` pointer into its physical
// byte address by subtracting `vmemmap_base_` (`pfn = (page_ptr - vmemmap_base_) / 64`).
bool
v2p_tracker_t::page_ptr_to_paddr(uint64_t page_ptr, uint64_t &paddr_out) const
{
    if (vmemmap_base_ == 0 || page_ptr < vmemmap_base_)
        return false;
    uint64_t pfn = (page_ptr - vmemmap_base_) >> kStructPageShift;
    if (pfn >= (1ULL << 52))
        return false;
    paddr_out = pfn * kPageSize4K;
    return true;
}

// Matches `mm` against the traced process's `mm_struct *`, learning `tracked_mm_` on
// the first event from `target_pid_` (or one of its `/proc/<pid>/task/<tid>` threads)
// and installing kernel-side `mm == tracked_mm_` filters.
bool
v2p_tracker_t::is_tracked_mm(uint64_t mm, pid_t tid)
{
    if (mm == 0)
        return false;
    if (tracked_mm_ == 0) {
        if (kernel_tracing_active_ && tid != target_pid_) {
            std::string task_path =
                "/proc/" + std::to_string(target_pid_) + "/task/" + std::to_string(tid);
            if (access(task_path.c_str(), F_OK) != 0)
                return false;
        }
        tracked_mm_ = mm;
        apply_probe_filters();
    }
    return mm == tracked_mm_;
}

// Resets both the virtual shadow page table and the physical-to-virtual reverse index.
void
v2p_tracker_t::clear_shadow_tables()
{
    shadow_table_ = {};
    paddr_spans_[0].clear();
    paddr_spans_[1].clear();
    max_multi_span_bytes_ = 0;
}

// Clears `is_huge_2m` across `[aligned_2m_va, +2 MB)` once a PMD is split into 4 KB
// PTEs so a later `__split_huge_pmd` call on that range does not treat 4 KB pages as a
// 2 MB PMD.
void
v2p_tracker_t::clear_huge_2m_window(uint64_t aligned_2m_va)
{
    for (auto it = shadow_table_.find_overlapping(aligned_2m_va);
         it != shadow_table_.spans.end() && it->first < aligned_2m_va + kPageSize2M;
         ++it) {
        it->second.is_huge_2m = false;
    }
}

// Rebinds `[start_va, end_va)` to individual 4 KB `zero_pfn_` PTEs (or unmaps if
// `zero_pfn_` is unknown) after a 2 MB shared huge zero page is split.
void
v2p_tracker_t::rebind_zero_ptes(uint64_t timestamp_us, uint64_t start_va, uint64_t end_va)
{
    if (zero_pfn_ == 0) {
        unbind_range(timestamp_us, start_va, end_va);
        return;
    }
    uint64_t zero_pa = zero_pfn_ * kPageSize4K;
    for (uint64_t va = start_va; va < end_va; va += kPageSize4K)
        bind_pages(timestamp_us, va, zero_pa, 1, /*is_huge_2m=*/false);
}

// When a 4 KB write fault or partial unmap touches part of a 2 MB shared huge zero
// page, the kernel splits that 2 MB PMD and points the remaining 4 KB PTEs at the 4 KB
// shared zero page (not at 4 KB sub-frames of the 2 MB zero page); apply that split in
// `shadow_table_` before updating `[lo, hi)`.
void
v2p_tracker_t::split_overlapping_huge_zero(uint64_t timestamp_us, uint64_t lo,
                                           uint64_t hi, bool partial_only)
{
    for (;;) {
        uint64_t hzero_base_2m = ~0ULL;
        for (auto it = shadow_table_.find_overlapping(lo);
             it != shadow_table_.spans.end() && it->first < hi; ++it) {
            if (it->second.is_huge_2m && it->second.is_huge_zero) {
                uint64_t base_2m = it->first & ~(kPageSize2M - 1);
                if (!partial_only || lo > base_2m || hi < base_2m + kPageSize2M) {
                    hzero_base_2m = base_2m;
                    break;
                }
            }
        }
        if (hzero_base_2m == ~0ULL)
            break;
        split_huge_pmd(timestamp_us, hzero_base_2m, /*is_anon=*/true);
    }
}

// Updates the in-memory `paddr_base -> vaddr_start` map (`paddr_spans_`) used by
// `rebind_migrated_folio` to locate virtual pages backed by a migrated physical frame.
// Shared zero pages are excluded because the kernel never migrates them.
void
v2p_tracker_t::update_span_reverse(uint64_t vaddr_start, const shadow_span_t &span,
                                   bool insert)
{
    if (span.page_count == 0 || span.is_huge_zero ||
        (span.page_count == 1 && zero_pfn_ != 0 &&
         span.paddr_base == zero_pfn_ * kPageSize4K)) {
        return;
    }
    // Split single-page (`bucket = 0`) and multi-page (`bucket = 1`) spans so multi-page
    // spans do not widen `lower_bound` lookback on 4 KB page migrations.
    size_t bucket = span.page_count > 1 ? 1 : 0;
    paddr_ref_t ref { span.paddr_base, vaddr_start };
    if (insert) {
        paddr_spans_[bucket].insert(ref);
        if (bucket == 1) {
            max_multi_span_bytes_ =
                std::max(max_multi_span_bytes_,
                         static_cast<uint64_t>(span.page_count) * kPageSize4K);
        }
    } else {
        paddr_spans_[bucket].erase(ref);
    }
}

// Carves `[start_vaddr, end_vaddr)` (restricted to physical frames in `[paddr_lo,
// paddr_hi)`) out of `shadow_table_`, keeping `paddr_spans_` synchronized and
// optionally emitting coalesced `V2P_RECORD_UNMAP` records.
void
v2p_tracker_t::carve_range(uint64_t timestamp_us, uint64_t start_vaddr,
                           uint64_t end_vaddr, uint64_t paddr_lo, uint64_t paddr_hi,
                           bool emit_unmap, bool preserve_huge)
{
    uint64_t run_start = 0;
    uint32_t run_pages = 0;
    shadow_table_.carve(
        start_vaddr, end_vaddr, paddr_lo, paddr_hi, preserve_huge,
        [this](uint64_t va, const shadow_span_t &s, bool ins) {
            update_span_reverse(va, s, ins);
        },
        // Coalesce contiguous carved virtual ranges into a single `UNMAP` record.
        [&](uint64_t m_lo, uint64_t m_hi) {
            if (!emit_unmap)
                return;
            uint32_t m_pages = static_cast<uint32_t>((m_hi - m_lo) / kPageSize4K);
            if (run_pages > 0 &&
                m_pages <= (std::numeric_limits<uint32_t>::max)() - run_pages &&
                m_lo == run_start + static_cast<uint64_t>(run_pages) * kPageSize4K) {
                run_pages += m_pages;
                return;
            }
            if (run_pages > 0) {
                write_record(out_fd_, timestamp_us, run_start, 0, run_pages,
                             V2P_RECORD_UNMAP);
            }
            run_start = m_lo;
            run_pages = m_pages;
        });
    if (emit_unmap && run_pages > 0)
        write_record(out_fd_, timestamp_us, run_start, 0, run_pages, V2P_RECORD_UNMAP);
}

// Merges adjacent `(vaddr, paddr)` slices that remain contiguous in both virtual and
// physical memory so migrating a multi-page range emits a single `V2P_RECORD_MAP`
// record rather than one record per 4 KB page.
void
v2p_tracker_t::bind_coalesced_slices(uint64_t timestamp_us,
                                     const std::vector<rebound_slice_t> &slices)
{
    for (size_t i = 0; i < slices.size();) {
        const rebound_slice_t &s = slices[i];
        uint32_t count = s.page_count;
        size_t j = i + 1;
        // Merge runs that are contiguous in both virtual and physical space.
        while (j < slices.size() &&
               slices[j].page_count <= (std::numeric_limits<uint32_t>::max)() - count &&
               slices[j].vaddr == s.vaddr + static_cast<uint64_t>(count) * kPageSize4K &&
               slices[j].paddr == s.paddr + static_cast<uint64_t>(count) * kPageSize4K &&
               slices[j].is_huge_2m == s.is_huge_2m &&
               slices[j].is_huge_zero == s.is_huge_zero) {
            count += slices[j++].page_count;
        }
        bool keep_huge =
            s.is_huge_2m && count == kPagesPer2M && (s.vaddr & (kPageSize2M - 1)) == 0;
        if (s.is_huge_zero && !keep_huge) {
            if (zero_pfn_ != 0) {
                rebind_zero_ptes(timestamp_us, s.vaddr,
                                 v2p_shadow_page_table_t::end_of(s.vaddr, count));
            }
        } else {
            bind_pages(timestamp_us, s.vaddr, s.paddr, count, keep_huge,
                       keep_huge && s.is_huge_zero);
        }
        i = j;
    }
}

// Installs `[aligned_vaddr, +page_count * 4 KB) -> aligned_paddr` in `shadow_table_`
// and the physical reverse index, emitting a `V2P_RECORD_MAP` if the mapping changed.
void
v2p_tracker_t::bind_pages(uint64_t timestamp_us, uint64_t aligned_vaddr,
                          uint64_t aligned_paddr, uint32_t page_count, bool is_huge_2m,
                          bool is_huge_zero)
{
    if (page_count == 0)
        return;
    uint64_t end_va = aligned_vaddr + static_cast<uint64_t>(page_count) * kPageSize4K;

    // Suppress duplicate `V2P_RECORD_MAP` records if `shadow_table_` already holds the
    // identical mapping across `[aligned_vaddr, end_va)`.
    uint64_t covered = aligned_vaddr;
    for (auto it = shadow_table_.find_overlapping(aligned_vaddr);
         it != shadow_table_.spans.end() && it->first <= covered && covered < end_va;
         ++it) {
        if (it->second.paddr_base + (covered - it->first) !=
                aligned_paddr + (covered - aligned_vaddr) ||
            it->second.is_huge_2m != is_huge_2m ||
            it->second.is_huge_zero != is_huge_zero) {
            break;
        }
        covered = std::min(
            end_va, v2p_shadow_page_table_t::end_of(it->first, it->second.page_count));
    }
    if (covered == end_va)
        return;

    // Overwriting part of a 2 MB PMD with 4 KB PTEs first splits any shared huge-zero
    // PMD into 4 KB `zero_pfn_` PTEs and clears `is_huge_2m` across that 2 MB window.
    if (!is_huge_2m) {
        split_overlapping_huge_zero(timestamp_us, aligned_vaddr, end_va,
                                    /*partial_only=*/false);
        for (auto it = shadow_table_.find_overlapping(aligned_vaddr);
             it != shadow_table_.spans.end() && it->first < end_va; ++it) {
            if (it->second.is_huge_2m)
                clear_huge_2m_window(it->first & ~(kPageSize2M - 1));
        }
    }

    carve_range(timestamp_us, aligned_vaddr, end_va, 0,
                (std::numeric_limits<uint64_t>::max)(), /*emit_unmap=*/false,
                /*preserve_huge=*/false);
    shadow_span_t span { aligned_paddr, page_count, is_huge_2m, is_huge_zero };
    shadow_table_.spans.emplace(aligned_vaddr, span);
    update_span_reverse(aligned_vaddr, span, /*insert=*/true);
    write_record(out_fd_, timestamp_us, aligned_vaddr, aligned_paddr, page_count,
                 V2P_RECORD_MAP);
}

// Rebinds every virtual span overlapping `[src_paddr, src_paddr + folio_pages * 4 KB)`
// to `dst_paddr` at the same relative folio offset.
void
v2p_tracker_t::rebind_migrated_folio(uint64_t timestamp_us, uint64_t src_paddr,
                                     uint64_t dst_paddr, uint32_t folio_pages)
{
    if (folio_pages == 0 || src_paddr == dst_paddr)
        return;
    uint64_t src_end = src_paddr + static_cast<uint64_t>(folio_pages) * kPageSize4K;
    const uint64_t max_bytes[2] = { 0, max_multi_span_bytes_ };
    std::vector<rebound_slice_t> slices;

    // Query both physical reverse-index buckets (single-page `b=0` and multi-page `b=1`
    // bounded by `max_multi_span_bytes_`) so system-wide migration probes run in O(log
    // N).
    for (size_t b = 0; b < 2; ++b) {
        if (paddr_spans_[b].empty())
            continue;
        uint64_t min_pa = src_paddr > max_bytes[b] ? src_paddr - max_bytes[b] : 0;
        for (auto it = paddr_spans_[b].lower_bound({ min_pa, 0 });
             it != paddr_spans_[b].end() && it->first < src_end; ++it) {
            auto sit = shadow_table_.spans.find(it->second);
            if (sit == shadow_table_.spans.end() || sit->second.paddr_base != it->first)
                continue;
            uint64_t s_pa_end =
                it->first + static_cast<uint64_t>(sit->second.page_count) * kPageSize4K;
            if (s_pa_end <= src_paddr)
                continue;
            uint64_t ov_pa_lo = std::max(it->first, src_paddr);
            uint64_t ov_pa_hi = std::min(s_pa_end, src_end);
            slices.push_back({ it->second + (ov_pa_lo - it->first),
                               dst_paddr + (ov_pa_lo - src_paddr),
                               static_cast<uint32_t>((ov_pa_hi - ov_pa_lo) / kPageSize4K),
                               sit->second.is_huge_2m, false });
        }
    }
    if (slices.empty())
        return;

    // Sort translated slices by virtual address so adjacent PTEs coalesce into one `MAP`.
    std::sort(slices.begin(), slices.end(),
              [](const rebound_slice_t &a, const rebound_slice_t &b) {
                  return a.vaddr < b.vaddr;
              });
    bind_coalesced_slices(timestamp_us, slices);
}

// Moves mapped spans from `[old_addr, old_addr + moved_bytes)` to `new_addr` (`mremap`).
void
v2p_tracker_t::relocate_range(uint64_t timestamp_us, uint64_t old_addr, uint64_t new_addr,
                              uint64_t moved_bytes)
{
    if (moved_bytes < kPageSize4K || old_addr == new_addr)
        return;
    uint64_t old_end = sat_add(old_addr, moved_bytes);

    // Snapshot clipped slices from `[old_addr, old_end)` before unmapping old and
    // destination ranges (which may overlap or replace existing destination mappings).
    std::vector<rebound_slice_t> slices;
    for (auto it = shadow_table_.find_overlapping(old_addr);
         it != shadow_table_.spans.end() && it->first < old_end; ++it) {
        uint64_t s_va = it->first;
        uint64_t clip_va = std::max(s_va, old_addr);
        uint64_t clip_end = std::min(
            v2p_shadow_page_table_t::end_of(s_va, it->second.page_count), old_end);
        slices.push_back({ new_addr + (clip_va - old_addr),
                           it->second.paddr_base + (clip_va - s_va),
                           static_cast<uint32_t>((clip_end - clip_va) / kPageSize4K),
                           it->second.is_huge_2m, it->second.is_huge_zero });
    }
    if (slices.empty())
        return;

    unbind_range(timestamp_us, old_addr, old_end);
    unbind_range(timestamp_us, new_addr, sat_add(new_addr, moved_bytes));
    bind_coalesced_slices(timestamp_us, slices);
}

// Unmaps `[start_vaddr, end_vaddr)`, pre-splitting any partially unmapped 2 MB shared
// huge zero page while preserving `is_huge_2m` on partial file-THP zaps until
// `split_pmd`.
void
v2p_tracker_t::unbind_range(uint64_t timestamp_us, uint64_t start_vaddr,
                            uint64_t end_vaddr)
{
    if (end_vaddr <= start_vaddr)
        return;
    split_overlapping_huge_zero(timestamp_us, start_vaddr, end_vaddr,
                                /*partial_only=*/true);
    carve_range(timestamp_us, start_vaddr, end_vaddr, 0,
                (std::numeric_limits<uint64_t>::max)(), /*emit_unmap=*/true,
                /*preserve_huge=*/true);
}

// Unmaps only sub-pages in `[start_vaddr, +max_pages)` still backed by `folio_paddr`,
// preserving any sub-pages that previously diverged via private Copy-on-Write.
void
v2p_tracker_t::unbind_folio_in_range(uint64_t timestamp_us, uint64_t start_vaddr,
                                     uint32_t max_pages, uint64_t folio_paddr,
                                     uint32_t folio_pages)
{
    if (max_pages == 0 || folio_pages == 0)
        return;
    carve_range(timestamp_us, start_vaddr,
                v2p_shadow_page_table_t::end_of(start_vaddr, max_pages), folio_paddr,
                folio_paddr + static_cast<uint64_t>(folio_pages) * kPageSize4K,
                /*emit_unmap=*/true, /*preserve_huge=*/false);
}

// Handles `__split_huge_pmd` on `[aligned_2m_va, +2 MB)`:
// - Shared 2 MB huge zero PMD: rebinds surviving PTEs to `zero_pfn_`.
// - Anonymous 2 MB THP: converts in place to 4 KB granularity (`V2P_RECORD_SPLIT`).
// - File/shmem 2 MB THP (`!is_anon`): zaps the PMD (`V2P_RECORD_UNMAP`).
void
v2p_tracker_t::split_huge_pmd(uint64_t timestamp_us, uint64_t aligned_2m_va, bool is_anon)
{
    uint64_t end_2m = aligned_2m_va + kPageSize2M;
    std::vector<std::pair<uint64_t, uint64_t>> huge_ranges;
    uint64_t base_paddr = 0;
    bool any_huge_zero = false;
    for (auto it = shadow_table_.find_overlapping(aligned_2m_va);
         it != shadow_table_.spans.end() && it->first < end_2m; ++it) {
        if (!it->second.is_huge_2m)
            continue;
        uint64_t lo = std::max(it->first, aligned_2m_va);
        uint64_t hi = std::min(
            v2p_shadow_page_table_t::end_of(it->first, it->second.page_count), end_2m);
        if (huge_ranges.empty())
            base_paddr = it->second.paddr_base + (aligned_2m_va - it->first);
        any_huge_zero = any_huge_zero || it->second.is_huge_zero;
        huge_ranges.push_back({ lo, hi });
    }
    if (huge_ranges.empty())
        return;

    // 1. Shared 2 MB huge zero PMD: Linux populates 512 4 KB `my_zero_pfn` PTEs.
    if (any_huge_zero) {
        clear_huge_2m_window(aligned_2m_va);
        if (zero_pfn_ != 0) {
            for (const auto &rng : huge_ranges)
                rebind_zero_ptes(timestamp_us, rng.first, rng.second);
        } else {
            unbind_range(timestamp_us, aligned_2m_va, end_2m);
        }
        return;
    }

    // 2. Anonymous 2 MB THP: physical frames stay in place at 4 KB PTE granularity.
    if (is_anon) {
        clear_huge_2m_window(aligned_2m_va);
        if (huge_ranges.size() == 1 && huge_ranges[0].first == aligned_2m_va &&
            huge_ranges[0].second == end_2m) {
            write_record(out_fd_, timestamp_us, aligned_2m_va, base_paddr, kPagesPer2M,
                         V2P_RECORD_SPLIT);
        }
        return;
    }

    // 3. File/shmem 2 MB THP: `__split_huge_pmd_locked` clears the PMD without
    // installing PTEs (subsequent accesses fault 4 KB pages back in on demand).
    unbind_range(timestamp_us, aligned_2m_va, end_2m);
}

// Clears the old address space on `execve`, resets `tracked_mm_`, and removes the
// previous `mm` filter so the new executable's `mm_struct *` can be learned.
void
v2p_tracker_t::reset_for_exec(uint64_t timestamp_us)
{
    hooks_.clear();
    if (!shadow_table_.spans.empty()) {
        uint64_t first_va = shadow_table_.spans.begin()->first;
        const auto &last = *shadow_table_.spans.rbegin();
        unbind_range(timestamp_us, first_va,
                     v2p_shadow_page_table_t::end_of(last.first, last.second.page_count));
    }
    clear_shadow_tables();
    tracked_mm_ = 0;
    apply_probe_filters();
}

// Sums ring-buffer `overrun:` counts across `per_cpu/cpu*/stats` in the instance.
uint64_t
v2p_tracker_t::read_total_overflows() const
{
    std::string per_cpu = instance_dir_ + "/per_cpu";
    DIR *dir = opendir(per_cpu.c_str());
    if (dir == nullptr)
        return 0;
    uint64_t total = 0;
    for (struct dirent *e = nullptr; (e = readdir(dir)) != nullptr;) {
        if (strncmp(e->d_name, "cpu", 3) != 0)
            continue;
        FILE *fp = fopen((per_cpu + "/" + e->d_name + "/stats").c_str(), "r");
        char buf[256];
        // Match line-initial `overrun:` rather than `commit overrun:`.
        while (fp != nullptr && fgets(buf, sizeof(buf), fp) != nullptr) {
            if (strncmp(buf, "overrun:", 8) == 0)
                total += strtoull(buf + 8, nullptr, 10);
        }
        if (fp != nullptr)
            fclose(fp);
    }
    closedir(dir);
    return total;
}

// Closes `trace_pipe`, disables events in the isolated instance,
// removes the instance directory, and unregisters this session's kprobes.
void
v2p_tracker_t::cleanup_kernel_tracing()
{
    if (trace_pipe_fd_ >= 0) {
        close(trace_pipe_fd_);
        trace_pipe_fd_ = -1;
    }
    if (!kernel_tracing_active_)
        return;

    v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "0");
    v2p_write_tracefs_file(instance_dir_ + "/events/enable", "0");
    rmdir(instance_dir_.c_str());
    for (const std::string &p : registered_probes_) {
        v2p_write_tracefs_file(tracefs_root_ + "/kprobe_events", "-:kprobes/" + p + "\n",
                               /*append=*/true);
    }
    registered_probes_.clear();
    kernel_tracing_active_ = false;
}

// Stops tracefs collection, joins the reader thread to drain all buffered events, checks
// per-CPU ring-buffer drop counts, tears down the kernel instance, and appends any
// `V2P_RECORD_OVERFLOW` summary plus `V2P_RECORD_END`.
void
v2p_tracker_t::stop_and_drain_tracing(bool save_exec_offset, bool close_output)
{
    // Freeze new ring-buffer writes first so `drain_pipe()` reaches EOF cleanly.
    if (kernel_tracing_active_)
        v2p_write_tracefs_file(instance_dir_ + "/tracing_on", "0");

    // Signal the non-suspendable client reader thread to exit and reap its clone task.
    stop_requested_.store(true, std::memory_order_release);
    if (thread_running_.exchange(false, std::memory_order_acq_rel)) {
        while (!thread_exited_.load(std::memory_order_acquire))
            dr_sleep(1);
        pid_t rtid = reader_tid_.exchange(0, std::memory_order_acq_rel);
        while (rtid > 0 && waitpid(rtid, nullptr, __WCLONE) < 0 && errno == EINTR) {
        }
    } else if (kernel_tracing_active_) {
        drain_pipe();
    }
    reader_tid_.store(0, std::memory_order_relaxed);

    // Check per-CPU ring-buffer drop counts before removing the tracefs instance.
    uint64_t overflows = kernel_tracing_active_ ? read_total_overflows() : 0;
    if (!save_exec_offset && overflows > 0) {
        dr_fprintf(STDERR,
                   "WARNING: drmemtrace v2p_tracker detected %" PRIu64
                   " dropped tracefs events due to buffer overflow (increase "
                   "-v2p_buffer_size_kb)\n",
                   overflows);
    }

    cleanup_kernel_tracing();

    // Emit any overflow summary and the trailing `V2P_RECORD_END` sentinel, saving the
    // pre-trailer file offset when suspending for `execve` in case `execve` fails.
    if (out_fd_ >= 0) {
        if (save_exec_offset)
            exec_saved_offset_ = lseek(out_fd_, 0, SEEK_CUR);
        if (overflows > 0) {
            uint32_t clamped = static_cast<uint32_t>(
                std::min<uint64_t>(overflows, (std::numeric_limits<uint32_t>::max)()));
            write_record(out_fd_, 0, 0, 0, clamped, V2P_RECORD_OVERFLOW);
        }
        write_record(out_fd_, 0, 0, 0, 0, V2P_RECORD_END);
        if (close_output) {
            close(out_fd_);
            out_fd_ = -1;
        }
    }
}

// Finalizes tracing on normal process exit (or detaches quietly in an untraced fork).
void
v2p_tracker_t::stop()
{
    if (!running_.exchange(false, std::memory_order_acq_rel))
        return;
    if (owner_pid_ != 0 && getpid() != owner_pid_) {
        detach_forked_child(/*exclude_child_pid=*/false);
        return;
    }
    if (suspended_for_exec_) {
        suspended_for_exec_ = false;
        exec_saved_offset_ = -1;
        if (out_fd_ >= 0)
            close(out_fd_);
        out_fd_ = -1;
        return;
    }
    stop_and_drain_tracing(/*save_exec_offset=*/false, /*close_output=*/true);
}

// Drains tracing and writes a tentative `V2P_RECORD_END` before `SYS_execve`/`execveat`
// because a successful `execve` replaces the process image without running `event_exit`.
void
v2p_tracker_t::suspend_for_exec()
{
    if (!running_.load(std::memory_order_acquire) || !kernel_tracing_active_ ||
        suspended_for_exec_ || (owner_pid_ != 0 && getpid() != owner_pid_)) {
        return;
    }
    stop_and_drain_tracing(/*save_exec_offset=*/true, /*close_output=*/false);
    suspended_for_exec_ = true;
}

// Truncates the tentative `V2P_RECORD_END` written by `suspend_for_exec()` and restarts
// the tracefs instance when `SYS_execve`/`execveat` fails and returns to user space.
void
v2p_tracker_t::resume_after_failed_exec()
{
    if (!running_.load(std::memory_order_acquire) || !suspended_for_exec_ ||
        (owner_pid_ != 0 && getpid() != owner_pid_)) {
        return;
    }
    suspended_for_exec_ = false;
    if (out_fd_ >= 0 && exec_saved_offset_ >= 0 &&
        ftruncate(out_fd_, exec_saved_offset_) == 0) {
        lseek(out_fd_, exec_saved_offset_, SEEK_SET);
    }
    exec_saved_offset_ = -1;
    setup_tracefs_and_reader(/*is_initial_start=*/false);
}

/* -------------------------------------------------------------------------
 * Background Trace-Pipe Reader & Event Parsing
 * ------------------------------------------------------------------------- */

// Reads available non-blocking bytes from `trace_pipe_fd_`, reassembling partial lines
// across reads in `pipe_remainder_` and dispatching each complete line.
void
v2p_tracker_t::drain_pipe()
{
    if (trace_pipe_fd_ < 0)
        return;
    char chunk[16384];
    for (ssize_t n = 0; (n = read(trace_pipe_fd_, chunk, sizeof(chunk))) > 0;) {
        pipe_remainder_.append(chunk, static_cast<size_t>(n));
        for (size_t nl = 0; (nl = pipe_remainder_.find('\n')) != std::string::npos;) {
            process_trace_line(pipe_remainder_.substr(0, nl));
            pipe_remainder_.erase(0, nl + 1);
        }
    }
}

// Parses the ftrace prefix (`<comm>-<tid> [<cpu>] [<irq_flags>] <secs>.<usecs>: `) of a
// `trace_pipe` line and dispatches the event payload to `hooks_`.
void
v2p_tracker_t::process_trace_line(const std::string &line)
{
    pid_t tid = 0;
    uint64_t timestamp_us = 0;
    size_t event_pos = std::string::npos;
    bool in_irq = false;
    const char *raw = line.c_str();

    // Scan each `" ["` candidate because thread `comm` names may contain `[...]`
    // (e.g., `" tokio-rt[0]-1000 [000] d.s1. 0.000081: ..."`).
    for (size_t cpu_open = 0; (cpu_open = line.find(" [", cpu_open)) != std::string::npos;
         ++cpu_open) {
        size_t tid_end = cpu_open;
        while (tid_end > 0 && line[tid_end - 1] == ' ')
            --tid_end;
        size_t dash = tid_end;
        while (dash > 0 && isdigit(static_cast<unsigned char>(line[dash - 1])))
            --dash;
        if (dash == 0 || dash == tid_end || line[dash - 1] != '-')
            continue;

        char cpu_s[16], flags_s[16] = {}, sec_s[24], usec_s[16];
        char sp1 = 0, sp2 = 0;
        int n = 0;
        bool has_flags =
            sscanf(raw + cpu_open + 2, "%15[0-9]]%c %15[^] :] %23[0-9].%15[0-9]:%c%n",
                   cpu_s, &sp1, flags_s, sec_s, usec_s, &sp2, &n) == 6 &&
            sp1 == ' ' && sp2 == ' ';
        if (!has_flags &&
            (sscanf(raw + cpu_open + 2, "%15[0-9]]%c %23[0-9].%15[0-9]:%c%n", cpu_s, &sp1,
                    sec_s, usec_s, &sp2, &n) != 5 ||
             sp1 != ' ' || sp2 != ' ')) {
            continue;
        }
        // Byte 2 of the ftrace flags field (`d.s1.`) marks hardirq/softirq/NMI context.
        if (has_flags && strlen(flags_s) >= 3) {
            char irq = flags_s[2];
            in_irq = (irq == 'h' || irq == 'H' || irq == 's' || irq == 'S' ||
                      irq == 'z' || irq == 'Z');
        }
        tid = static_cast<pid_t>(strtol(raw + dash, nullptr, 10));
        timestamp_us = strtoull(sec_s, nullptr, 10) * 1000000ULL +
            strtoull(usec_s, nullptr, 10) + clock_offset_us_;
        event_pos = cpu_open + 2 + static_cast<size_t>(n);
        break;
    }

    // Ignore malformed lines and any page faults generated by the reader thread itself.
    pid_t rtid = reader_tid_.load(std::memory_order_relaxed);
    if (event_pos == std::string::npos || tid <= 0 || (rtid != 0 && tid == rtid) ||
        vmemmap_base_ == 0) {
        return;
    }

    hooks_.handle_event(line, event_pos, tid, timestamp_us, plan_, *this, in_irq);
}

// Entry point for the background client reader thread: marks itself non-suspendable by
// DR synchall, publishes its TID so its own faults are ignored, and polls `trace_pipe`.
void
v2p_tracker_t::reader_thread_entry(void *arg)
{
    auto *self = static_cast<v2p_tracker_t *>(arg);
    dr_client_thread_set_suspendable(false);
    self->reader_tid_.store(static_cast<pid_t>(syscall(SYS_gettid)),
                            std::memory_order_release);

    while (!self->stop_requested_.load(std::memory_order_acquire)) {
        struct pollfd pfd = { self->trace_pipe_fd_, POLLIN, 0 };
        if (poll(&pfd, 1, 10) > 0)
            self->drain_pipe();
    }

    // Perform one final drain after `tracing_on` was set to 0 before signaling exit.
    self->drain_pipe();
    self->thread_exited_.store(true, std::memory_order_release);
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
