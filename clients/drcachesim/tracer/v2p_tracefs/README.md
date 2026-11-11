# Tracefs Virtual-to-Physical Page-Table Tracker (`v2p_tracefs`)

`v2p_tracefs` captures a process's virtual-to-physical (V2P) address mappings during `drmemtrace` `-offline` tracing by hooking Linux `tracefs` events to track all kernel updates to the page table. The tracker applies these transitions to an in-memory shadow page table (seeded from an initial `/proc/<pid>/pagemap` snapshot) and records timestamped mapping changes so post-processing can reconstruct the active mappings throughout the trace. It requires 64-bit Linux (`x86_64` or `aarch64`) with 4 KB base pages, a BTF-enabled Linux 5.4+ kernel, and root privileges.

## Usage and Integration with `drmemtrace`

`-enable_v2p_tracefs` applies only to `drmemtrace` `-offline` mode:

```bash
# 1. Raw trace collection (-offline mode):
bin64/drrun -t drmemtrace -offline -enable_v2p_tracefs -- /path/to/app <args>

# 2. Post-processing:
clients/bin64/drraw2trace -indir drmemtrace.app.pid.xxxx.dir/
```

Data flow from trace collection through post-processing:
1. **Raw Trace Collection (`tracer.cpp`):** Starts the page-table tracker alongside raw instruction tracing to write timestamped mapping updates into `raw/v2p.bin` using the same clock as the raw trace, draining any buffered events on exit.
2. **Post-Processing (`raw2trace_directory.cpp`, `raw2trace.cpp`):** Reads `raw/v2p.bin` from the trace directory, aligns the mapping updates with the timestamps in each thread's raw trace, and emits standard `drmemtrace` physical-address markers into `trace/` whenever a thread accesses a newly touched or remapped page (or marks the physical address unavailable if unmapped).

## Modules

- **`v2p_tracker.{h,cpp}`:** Manages the per-process `tracefs` instance and background event-reader thread, snapshots `/proc/<pid>/pagemap`, maintains the shadow page table, and writes `raw/v2p.bin`.
- **`v2p_check_kernel_support.{h,cpp}`:** Inspects kernel BTF and `tracefs` event metadata at startup to verify compatibility, select compatible kernel hooks for the running kernel, and resolve struct field offsets and enum values at runtime.
- **`v2p_tracefs_hooks.{h,cpp}`:** Parses kernel tracepoint and kprobe events (pairing function entry and return events when needed) and translates them into shadow page-table updates.

## Tracked Page-Table Transitions

After seeding the shadow page table from `/proc/<pid>/pagemap` at startup (and after `fork`), the tracker monitors runtime updates to 4 KB pages, transparent hugepages (THP, up to 2 MB), and `hugetlbfs` hugepages (up to 1 GB) across seven categories:

1. **Address-Space Replacement:** Replacing the process image on `execve`, which clears all mappings from the previous address space and takes a fresh `/proc/<pid>/pagemap` snapshot of the new executable.
2. **Page-Table Installation:** Mapping virtual pages or transparent hugepages to physical memory—covering anonymous page faults, Copy-on-Write, swap-in, THP allocation, anonymous hugepage collapse (`khugepaged` and `MADV_COLLAPSE`), KSM page merges, `uprobe` page replacement, anonymous `userfaultfd` insertion, shared zero-page faults, and file-backed page-cache mappings (4 KB pages and 2 MB file/shared-memory hugepages).
3. **Physical Page Migration:** Moving a mapped page or hugepage to a new physical address while keeping its virtual address unchanged (compaction, NUMA balancing, `move_pages`, and memory tiering).
4. **Virtual Address Relocation:** Moving existing mappings from an old virtual address range to a new virtual address range while keeping their physical addresses (`mremap`).
5. **Mapping Invalidation:** Unmapping standard and THP pages—covering both explicit unmapping (`munmap`, `MADV_DONTNEED`, `MADV_FREE`, `MAP_FIXED` overwrites, and file truncation or hole-punching) and kernel reclaim or eviction (swap-out, page-cache eviction, `MADV_PAGEOUT`, and hardware memory-error unmapping).
6. **Hugepage Split:** Splitting a 2 MB transparent hugepage—converting an anonymous or shared-zero 2 MB hugepage in place into 512 individual 4 KB page mappings, or clearing a file/shared-memory 2 MB hugepage mapping so individual 4 KB pages fault back in on demand.
7. **Explicit Hugepage Pools (`hugetlbfs`):** Allocating, sharing, Copy-on-Write unsharing, and unmapping `hugetlbfs` pages (`MAP_HUGETLB`, `SHM_HUGETLB`, and `hugetlbfs` files across 64 KB, 2 MB, 32 MB, and 1 GB page sizes).

Permission-only page-table updates (`mprotect`, accessed/dirty bit updates, and NUMA hint faults) do not change the underlying physical address and emit no `v2p.bin` records.

## Current Limitations

The following memory mappings and operations are not tracked:

- **Device memory, persistent memory (DAX), and kernel ring buffers:** Memory-mapped hardware registers (MMIO), direct-access persistent memory (DAX), and kernel-managed ring or zero-copy buffers (`io_uring`, eBPF `ringbuf`/`arena`, `perf_event` and packet-socket rings, and `TCP_ZEROCOPY_RECEIVE`).
- **GPU and accelerator memory migration:** Unified-memory pages migrated by drivers between host RAM and private GPU or accelerator memory (`HMM`).
- **Certain `userfaultfd` operations:** Zero-page insertion (`UFFDIO_ZEROPAGE`), page moves (`UFFDIO_MOVE`), shared-memory fault continuation (`UFFDIO_CONTINUE`), and copying into `MAP_SHARED` regions (`UFFDIO_COPY`).
- **KSM zero-page mode and `uprobe` detach:** Kernel Samepage Merging with optional zero-page deduplication enabled (`use_zero_pages=1`), and detaching an active dynamic user-space probe (`uprobe`).
- **Multi-process `hugetlbfs` page-table sharing:** Kernel sharing of page-table directories across processes mapping the same 1 GB-aligned `MAP_SHARED` 2 MB `hugetlbfs` region.
- **Hardware memory-error unmapping on `hugetlbfs`:** Kernel unmapping of `hugetlbfs` pages after an uncorrected hardware ECC memory error.

## Kernel Maintenance

Because Linux `mm/` internal functions and struct layouts can change across kernel versions, `v2p_check_kernel_support` validates BTF types, function parameters, struct offsets, and tracepoint fields at startup (and can be run directly against an extracted `vmlinux` `.BTF` section). Missing or incompatible hooks report the specific page-table transitions left uncovered and abort startup by default; passing `-v2p_tracefs_ignore_missing_hooks` allows tracing to proceed with warnings as long as foundational BTF layout, `vmemmap` calibration, and anonymous page-installation hooks succeed. Because BTF checks alone cannot detect semantic changes inside a function or newly introduced `mm/` mapping paths, the prompt below can be used as a starting point to check `v2p_tracefs` against a newer kernel:

```markdown
Read the source files and `README.md` in `clients/drcachesim/tracer/v2p_tracefs/` to identify the tracepoints, kprobes, BTF struct/field assumptions, and `Current Limitations` currently used to track user-space 4 KB, `mTHP`, 2 MB THP, and `hugetlbfs` virtual-to-physical mappings. Check those assumptions against Linux kernel `<KERNEL_VERSION_OR_TREE>`: verify from primary kernel sources whether any hooked function was renamed, inlined, or changed its parameters, return-value semantics, struct layout, or locking/ordering behavior, and trace user-space PTE/PMD/PUD mutations across `mm/` to check whether any existing or newly added kernel path changes physical mappings without firing an active hook or falling under `Current Limitations`. Cite the specific kernel functions or commits involved and recommend concrete updates to `v2p_tracefs/` and `v2p_reader_unit_test.cpp`.
```

## Current Kernel Hooks (up to v7.4-rc1)

`v2p_check_kernel_support` resolves the active tracefs tracepoints and kprobes for each category from `/sys/kernel/btf/vmlinux`:

- **1. Address-Space Replacement:** `sched/sched_process_exec`
- **2. Page-Table Installation:**
  - Anonymous: `folio_add_new_anon_rmap`, `folio_add_anon_rmap_ptes`, `folio_add_anon_rmap_pmd` (or legacy `page_add_new_anon_rmap`, `page_add_anon_rmap`)
  - Shared zero-page read faults: `handle_mm_fault` paired with `mm_get_huge_zero_folio` (or `mm_get_huge_zero_page`)
  - File-backed memory in the page cache (4 KB and batches): `set_pte_range` (or `do_set_pte`, `alloc_set_pte`)
  - File/shmem 2 MB PMDs: `do_set_pmd` (or `collapse_pte_mapped_thp` paired with `folio_add_file_rmap_pmd` / `page_add_file_rmap`)
- **3. Physical Page Migration:** `remove_migration_ptes`
- **4. Virtual Address Relocation:** `move_page_tables`
- **5. Mapping Invalidation:**
  - Explicit unmapping: `unmap_page_range` (or `unmap_vmas` + `zap_vma_range_batched`)
  - Reclaim and eviction: `try_to_unmap_one`
- **6. Hugepage Split:** `__split_huge_pmd`
- **7. Explicit Hugepage Pools (`hugetlbfs`, when `CONFIG_HUGETLB_PAGE=y`):** `hugetlb_fault`, `hugetlb_add_new_anon_rmap` (or `hugepage_add_new_anon_rmap`), `folio_unlock` (or `unlock_page`), and `__unmap_hugepage_range`
