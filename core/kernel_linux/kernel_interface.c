/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
 * Copyright (c) 2013 Peter Feiner.  All rights reserved.
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

/* The kernel's print helpers (pr_info(), pr_err(), etc.) expand pr_fmt(): this must be
 * defined at the top before the #include block to have the module name prepended to
 * every message. Since this is a kernel macro, not a DR one, it has to be lower-case.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "kernel_interface.h"

#include <asm/page.h>
#ifdef CONFIG_X86
#    include <asm/tsc.h> /* For tsc_khz. */
#elif defined(CONFIG_ARM64)
#    include <asm/arch_timer.h> /* For arch_timer_get_cntfrq(). */
#    include <asm/cacheflush.h> /* For flush_icache_range(). */
#    include <asm/cpufeature.h> /* For read_sanitised_ftr_reg(). */
#endif
#include <linux/cpumask.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/panic.h>
#include <linux/percpu.h>
#include <linux/pgtable.h>
#include <linux/printk.h>
#include <linux/preempt.h>
#include <linux/random.h>
#include <linux/smp.h>
#include <linux/stdarg.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "configure.h"
#include "dr_project_wide_defines.h"
#include "kernel_assert.h"

static void *heap = NULL;
static size_t heap_size;

static unsigned long (*kallsyms_lookup_name_ptr)(const char *name) = NULL;
static int (*kallsyms_lookup_size_offset_ptr)(unsigned long addr,
                                              unsigned long *symbolsize,
                                              unsigned long *offset) = NULL;
static void *(*vmalloc_node_range_ptr)(unsigned long size, unsigned long align,
                                       unsigned long start, unsigned long end,
                                       gfp_t gfp_mask, pgprot_t prot,
                                       unsigned long vm_flags, int node,
                                       const void *caller) = NULL;

static void *kernel_image_start = NULL;
static void *kernel_image_end = NULL;
#ifdef CONFIG_ARM64
static struct mm_struct *init_mm_ptr = NULL;
#endif

/* Holds DR's per-CPU TLS, including future client TLS slots; d_r_os_init() checks the
 * size. It must be static per-CPU data for gs:[disp32] access from generated code on
 * x86, and it shares the kernel's 8 KiB PERCPU_MODULE_RESERVE, so keep it small.
 * The module loader copies this zero-initialized buffer to every possible CPU
 * and releases the storage on unload.
 */
struct dr_cpu_local_storage {
    u8 data[1024];
};

static DEFINE_PER_CPU_ALIGNED(struct dr_cpu_local_storage, dr_cpu_local_storage);

static size_t
get_symbol_size(unsigned long address)
{
    KERNEL_ASSERT(kallsyms_lookup_size_offset_ptr != NULL);

    unsigned long size, offset;
    /* kallsyms_lookup_size_offset returns 1 on success,
     * and 0 if the address is not found.
     */
    if (kallsyms_lookup_size_offset_ptr(address, &size, &offset)) {
        return size;
    }
    return 0;
}

void *
kernel_find_symbol(const char *name, size_t *size)
{
    KERNEL_ASSERT(kallsyms_lookup_name_ptr != NULL);
    KERNEL_ASSERT(name != NULL);

    unsigned long addr = kallsyms_lookup_name_ptr(name);
    if (addr == 0) {
        return NULL;
    }

    if (size != NULL) {
        *size = get_symbol_size(addr);
    }
    return (void *)addr;
}

static int
resolve_kallsyms_lookup_name(void)
{
    /* kallsyms_lookup_name is unexported in newer kernels (5.7+), so we use
     * this kprobe trick to resolve its address.
     * NOTE: This requires the kernel to be configured with CONFIG_KPROBES=y.
     * If this becomes an issue, an alternative is to parse the address from
     * /proc/kallsyms in user space and pass it into the kernel module as a
     * module parameter.
     */
    struct kprobe kp = {
        .symbol_name = "kallsyms_lookup_name",
    };

    int ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("Failed to register kprobe for kallsyms_lookup_name\n");
        return ret;
    }

    kallsyms_lookup_name_ptr = (void *)kp.addr;
    unregister_kprobe(&kp);
    return 0;
}

static int
resolve_kernel_symbols(void)
{
    kallsyms_lookup_size_offset_ptr =
        kernel_find_symbol("kallsyms_lookup_size_offset", NULL);
    if (kallsyms_lookup_size_offset_ptr == NULL) {
        pr_err("Failed to resolve kallsyms_lookup_size_offset\n");
        return -ENOENT;
    }

    /* Across 6.6+ LTS kernels, __vmalloc_node_range is the only reliable API to allocate
     * writable memory at a chosen VA range. We use this to allocate our heap within 2GB
     * reach of the kernel image (see compute_heap_range()). Alternatives:
     * - module_alloc(): Removed in 6.10. Returns RW+NX.
     * - execmem_alloc(): Introduced in 6.10. Returns RW+NX in 6.10-6.12, but
     *   read-only-execute (ROX) in 6.13+.
     * - execmem_alloc_rw(): Introduced in 6.14. Returns RW+NX.
     *
     * TODO i#8124: We would need to allocate two separate memory heaps at startup for
     * fine-grained W^X protection:
     *   1) An executable code heap (marked +x).
     *   2) A non-executable data heap (left NX).
     */
    vmalloc_node_range_ptr = kernel_find_symbol("__vmalloc_node_range", NULL);
    if (vmalloc_node_range_ptr == NULL) {
        /* In 6.10+, CONFIG_MEM_ALLOC_PROFILING renames the symbol to
         * __vmalloc_node_range_noprof, so we need to probe for that instead.
         */
        vmalloc_node_range_ptr = kernel_find_symbol("__vmalloc_node_range_noprof", NULL);
    }
    if (vmalloc_node_range_ptr == NULL) {
        pr_err("Failed to resolve __vmalloc_node_range or __vmalloc_node_range_noprof\n");
        return -ENOENT;
    }

    kernel_image_start = kernel_find_symbol("_text", NULL);
    if (kernel_image_start == NULL) {
        pr_err("Failed to resolve _text\n");
        return -ENOENT;
    }

    kernel_image_end = kernel_find_symbol("_end", NULL);
    if (kernel_image_end == NULL) {
        pr_err("Failed to resolve _end\n");
        return -ENOENT;
    }

#ifdef CONFIG_ARM64
    init_mm_ptr = kernel_find_symbol("init_mm", NULL);
    if (init_mm_ptr == NULL) {
        pr_err("Failed to resolve init_mm\n");
        return -ENOENT;
    }
#endif

    return 0;
}

/* Returns whether |type| is a supported core module memory type.
 * If |prot| is non-NULL, |*prot| is set to the corresponding KERNEL_MEMPROT_* protection
 * on success.
 */
static bool
module_mem_type_supported(enum mod_mem_type type, unsigned int *prot)
{
    unsigned int protection = KERNEL_MEMPROT_READ;
    switch (type) {
    case MOD_TEXT: protection |= KERNEL_MEMPROT_EXEC; break;
    case MOD_DATA: protection |= KERNEL_MEMPROT_WRITE; break;
    case MOD_RODATA:
    case MOD_RO_AFTER_INIT: break;
    default: return false;
    }
    if (prot != NULL) {
        *prot = protection;
    }
    return true;
}

/* Rejects a kernel whose module memory layout has a core type we do not know, so that
 * kernel_walk_module_regions() cannot fail later.
 */
static int
check_module_mem_types(void)
{
    for_class_mod_mem_type(type, core)
    {
        if (THIS_MODULE->mem[type].size != 0 && !module_mem_type_supported(type, NULL)) {
            pr_err("Unsupported module memory type %d\n", type);
            return -EOPNOTSUPP;
        }
    }
    return 0;
}

#ifdef CONFIG_X86_64
/* On x86-64, kernel code lives in the top 2 GiB: the kernel image at
 * __START_KERNEL_map, followed by the module area up to MODULES_END, which also
 * backs other executable allocations (BPF JIT images, ftrace/kprobe trampolines).
 */
#    define KERNEL_CODE_WALK_START __START_KERNEL_map
#    define KERNEL_CODE_WALK_END MODULES_END

static unsigned long
query_kernel_page(unsigned long addr, unsigned int *prot)
{
    unsigned int level;
    pte_t *ptep = lookup_address(addr, &level);
    unsigned long size;

    *prot = 0;
    if (ptep == NULL) {
        /* Some entry at the PMD level or above is empty, so at least the PMD-sized block
         * containing |addr| is unmapped. We could use |level| to determine the exact skip
         * needed, but the meaning of |level| on a NULL return changed in Linux 6.11:
         * https://github.com/torvalds/linux/commit/9d1dcdfa9091
         * Skipping PMD_SIZE at a time is conservative but works across versions.
         */
        size = PMD_SIZE;
    } else {
        pte_t pte = ptep_get(ptep);
        size = page_level_size(level);

        if ((pte_flags(pte) & _PAGE_PRESENT) != 0) {
            *prot = KERNEL_MEMPROT_READ;
            if (pte_write(pte)) {
                *prot |= KERNEL_MEMPROT_WRITE;
            }
            if (pte_exec(pte)) {
                *prot |= KERNEL_MEMPROT_EXEC;
            }
        }
    }
    return ALIGN_BACKWARD(addr, size) + size;
}
#elif defined(CONFIG_ARM64)
/* On AArch64, kernel code lives in the module region and the vmalloc area above it,
 * which holds the kernel image and other executable allocations (BPF JIT images,
 * kprobe slots).
 */
#    define KERNEL_CODE_WALK_START MODULES_VADDR
#    define KERNEL_CODE_WALK_END VMALLOC_END

/* Also works for PMD/PUD block entries, which use the same bit positions. */
static unsigned int
pte_to_kernel_memprot(pte_t pte)
{
    if (!pte_valid(pte)) {
        return 0;
    }

    unsigned int prot = KERNEL_MEMPROT_READ;
    if (pte_write(pte)) {
        prot |= KERNEL_MEMPROT_WRITE;
    }
    /* Kernel execution is controlled by PXN; UXN only applies to EL0. */
    if ((pte_val(pte) & PTE_PXN) == 0) {
        prot |= KERNEL_MEMPROT_EXEC;
    }
    return prot;
}

/* AArch64 has no lookup_address(), so walk init_mm's page tables directly. */
static unsigned long
query_kernel_page(unsigned long addr, unsigned int *prot)
{
    *prot = 0;
    pgd_t *pgdp = pgd_offset(init_mm_ptr, addr);
    pgd_t pgd = pgdp_get(pgdp);
    if (pgd_none(pgd) || pgd_bad(pgd)) {
        return ALIGN_BACKWARD(addr, PGDIR_SIZE) + PGDIR_SIZE;
    }

    /* Since Linux 6.9, an AArch64 kernel built for 52-bit VA (LPA2) that runs on a CPU
     * without LPA2 folds p4d (4K pages) or pud (16K pages) at runtime. A folded level's
     * entry is located from the address of the entry above it, not from its contents,
     * so pass the real table pointer rather than a local copy. pmd and pte are never
     * folded at runtime, so reading them through copies below is fine.
     */
    p4d_t *p4dp = p4d_offset_lockless(pgdp, pgd, addr);
    p4d_t p4d = p4dp_get(p4dp);
    if (p4d_none(p4d) || p4d_bad(p4d)) {
        return ALIGN_BACKWARD(addr, P4D_SIZE) + P4D_SIZE;
    }

    pud_t pud = pudp_get(pud_offset_lockless(p4dp, p4d, addr));
    if (pud_none(pud)) {
        return ALIGN_BACKWARD(addr, PUD_SIZE) + PUD_SIZE;
    }
    /* pud_bad() is also true for valid block mappings, so check pud_leaf() first. */
    if (pud_leaf(pud)) {
        *prot = pte_to_kernel_memprot(__pte(pud_val(pud)));
        return ALIGN_BACKWARD(addr, PUD_SIZE) + PUD_SIZE;
    }
    if (pud_bad(pud)) {
        return ALIGN_BACKWARD(addr, PUD_SIZE) + PUD_SIZE;
    }

    pmd_t pmd = pmdp_get(pmd_offset(&pud, addr));
    if (pmd_none(pmd)) {
        return ALIGN_BACKWARD(addr, PMD_SIZE) + PMD_SIZE;
    }
    if (pmd_leaf(pmd)) {
        *prot = pte_to_kernel_memprot(__pte(pmd_val(pmd)));
        return ALIGN_BACKWARD(addr, PMD_SIZE) + PMD_SIZE;
    }
    if (pmd_bad(pmd)) {
        return ALIGN_BACKWARD(addr, PMD_SIZE) + PMD_SIZE;
    }

    *prot = pte_to_kernel_memprot(ptep_get(pte_offset_kernel(&pmd, addr)));
    return ALIGN_BACKWARD(addr, PAGE_SIZE) + PAGE_SIZE;
}
#else
#    error "Unsupported architecture"
#endif

static int
compute_heap_range(unsigned long *start, unsigned long *end)
{
    const unsigned long text = (unsigned long)kernel_image_start;
    const unsigned long last = (unsigned long)kernel_image_end - 1;
    const unsigned long reach_lo = last > INT_MAX ? last - INT_MAX : 0;
    const unsigned long reach_hi =
        text < ULONG_MAX - INT_MAX ? text + INT_MAX + 1 : ULONG_MAX;

    /* Stay inside the walked range: kernel_walk_code_regions() skips the heap by
     * splitting that range around it. On x86-64, addresses below MODULES_VADDR are
     * reserved for the kernel image mapping.
     */
    *start = max(reach_lo, (unsigned long)MODULES_VADDR);
    *end = min(reach_hi, (unsigned long)KERNEL_CODE_WALK_END);
    if (*start >= *end) {
        pr_err("No VA range within reach of the kernel image [%lx, %lx)\n", text,
               last + 1);
        return -ENOMEM;
    }
    return 0;
}

#ifdef DEBUG
/* Checks query_kernel_page() against pages with known protections, so that a broken
 * page table walk fails module load instead of silently reporting wrong regions.
 */
static int
check_query_kernel_page(void)
{
    /* Probe _stext rather than _text: on AArch64, [_text, _stext) holds the Image
     * header, which map_kernel() leaves unmapped.
     */
    const char *kernel_text = kernel_find_symbol("_stext", NULL);
    if (kernel_text == NULL) {
        pr_err("Failed to resolve _stext\n");
        return -ENOENT;
    }

    const unsigned int rx = KERNEL_MEMPROT_READ | KERNEL_MEMPROT_EXEC;
    const unsigned int rw = KERNEL_MEMPROT_READ | KERNEL_MEMPROT_WRITE;
    const bool strict = IS_ENABLED(CONFIG_STRICT_MODULE_RWX);
    const struct {
        const char *name;
        const char *addr;
        unsigned int required;
        unsigned int forbidden;
    } probes[] = {
        { "kernel text", kernel_text, rx, 0 },
        { "module text", THIS_MODULE->mem[MOD_TEXT].base, rx,
          strict ? KERNEL_MEMPROT_WRITE : 0 },
        { "module data", THIS_MODULE->mem[MOD_DATA].base, rw,
          strict ? KERNEL_MEMPROT_EXEC : 0 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(probes); i++) {
        unsigned int prot;
        query_kernel_page((unsigned long)probes[i].addr, &prot);
        if ((prot & probes[i].required) != probes[i].required ||
            (prot & probes[i].forbidden) != 0) {
            pr_err("Page table walk self-check failed for %s: prot=%#x\n", probes[i].name,
                   prot);
            return -EINVAL;
        }
    }

    return 0;
}
#endif

int
kernel_module_init(size_t dr_heap_size)
{
    int ret = resolve_kallsyms_lookup_name();
    if (ret != 0) {
        return ret;
    }

    ret = resolve_kernel_symbols();
    if (ret != 0) {
        return ret;
    }

    ret = check_module_mem_types();
    if (ret != 0) {
        return ret;
    }

#ifdef DEBUG
    ret = check_query_kernel_page();
    if (ret != 0) {
        return ret;
    }
#endif

#ifdef KSTATS
    /* kstats use the timestamp counter frequency as their tick rate and divide by it
     * when reporting.
     */
    if (kernel_get_timestamp_counter_khz() == 0) {
        pr_err("Timestamp counter frequency is unavailable (required by kstats)\n");
        return -ENODEV;
    }
#endif

    heap_size = dr_heap_size;
    /* PAGE_KERNEL_EXEC makes the heap RWX because it backs both DR's vmcode and vmheap.
     * TODO i#8124: Allocate two separate regions instead for fine-grained W^X protection.
     */
    unsigned long start, end;
    ret = compute_heap_range(&start, &end);
    if (ret != 0) {
        return ret;
    }
    heap = vmalloc_node_range_ptr(heap_size, PAGE_SIZE, start, end, GFP_KERNEL,
                                  PAGE_KERNEL_EXEC, VM_FLUSH_RESET_PERMS, NUMA_NO_NODE,
                                  __builtin_return_address(0));
    if (heap == NULL) {
        pr_err("Failed to allocate %zu bytes in [%lx, %lx) with __vmalloc_node_range\n",
               heap_size, start, end);
        return -ENOMEM;
    }

    return 0;
}

void
kernel_module_exit(void)
{
    if (heap != NULL) {
        vfree(heap);
        heap = NULL;
    }
}

size_t
kernel_get_heap_size(void)
{
    return heap_size;
}

void *
kernel_allocate_heap(size_t size)
{
    if (heap != NULL && heap_size >= size) {
        return heap;
    }
    return NULL;
}

int
kernel_get_cpu_id(void)
{
    KERNEL_ASSERT(!preemptible());
    return smp_processor_id();
}

int
kernel_get_online_processor_count(void)
{
    return num_online_cpus();
}

unsigned int
kernel_get_timestamp_counter_khz(void)
{
#ifdef CONFIG_X86
    return tsc_khz;
#elif defined(CONFIG_ARM64)
    return arch_timer_get_cntfrq() / 1000;
#else
#    error "Unsupported architecture"
#endif
}

#ifdef CONFIG_ARM64
void
kernel_get_cache_line_sizes(size_t *dcache_line_size, size_t *icache_line_size)
{
    u64 ctr = read_sanitised_ftr_reg(SYS_CTR_EL0);
    /* Cache line size is represented as Log2(words) in CTR_EL0. */
    if (dcache_line_size != NULL) {
        *dcache_line_size = 4 << SYS_FIELD_GET(CTR_EL0, DminLine, ctr);
    }
    if (icache_line_size != NULL) {
        *icache_line_size = 4 << SYS_FIELD_GET(CTR_EL0, IminLine, ctr);
    }
}

void
kernel_flush_icache_range(void *start, void *end)
{
    /* flush_icache_range() IPIs all online CPUs, which requires IRQs to be enabled. */
    KERNEL_ASSERT(!irqs_disabled());
    flush_icache_range((unsigned long)start, (unsigned long)end);
}
#endif

void
kernel_get_cpu_local_state_layout(size_t *size, size_t *alignment)
{
    *size = sizeof(dr_cpu_local_storage);
    /* DEFINE_PER_CPU_ALIGNED aligns the variable to the cache line, which is stronger
     * than the type's 1-byte alignment in this case.
     */
    *alignment = __alignof__(dr_cpu_local_storage);
}

void *
kernel_get_cpu_local_state(void)
{
    KERNEL_ASSERT(!preemptible());
    return this_cpu_ptr(&dr_cpu_local_storage)->data;
}

#ifdef CONFIG_X86
int
kernel_get_cpu_local_state_segment_offset(void)
{
    /* With -mcmodel=kernel, module code refers to this address through an R_X86_64_32S
     * relocation. The module loader rejects the module unless the address fits in a
     * sign-extended 32-bit value, so it always fits in an int.
     */
    long offs = (__force long)&dr_cpu_local_storage.data;
    KERNEL_ASSERT(offs == (int)offs);
    return (int)offs;
}
#endif

size_t
kernel_get_page_size(void)
{
    /* PAGE_SIZE is the target kernel's base page size, defined by <asm/page.h>. */
    return PAGE_SIZE;
}

void *
kernel_get_image_start(void)
{
    return kernel_image_start;
}

void *
kernel_get_image_end(void)
{
    return kernel_image_end;
}

char *
kernel_get_module_name(void)
{
    return THIS_MODULE->name;
}

void *
kernel_get_module_text_start(void)
{
    return THIS_MODULE->mem[MOD_TEXT].base;
}

void *
kernel_get_module_text_end(void)
{
    return THIS_MODULE->mem[MOD_TEXT].base + THIS_MODULE->mem[MOD_TEXT].size;
}

void
kernel_walk_module_regions(kernel_memory_region_callback_t callback, void *arg)
{
    /* Core regions don't change after the module is loaded, so this doesn't need
     * locking as long as the module stays loaded.
     */
    for_class_mod_mem_type(type, core)
    {
        const struct module_memory *mem = &THIS_MODULE->mem[type];
        /* Skip memory types this module doesn't use. For example, MOD_RO_AFTER_INIT
         * is empty unless the module has __ro_after_init variables or jump labels.
         */
        if (mem->size == 0) {
            continue;
        }

        unsigned int prot;
        if (!module_mem_type_supported(type, &prot)) {
            /* Should be unreachable as unknown module memory type is rejected
             * by check_module_mem_types() at module load.
             */
            KERNEL_ASSERT(false);
            continue;
        }

        const kernel_memory_region_t region = {
            .base = mem->base,
            .size = mem->size,
            .prot = prot,
        };

        callback(&region, arg);
    }
}

typedef struct {
    kernel_memory_region_t pending;
    kernel_memory_region_callback_t callback;
    void *arg;
} kernel_region_coalescer_t;

static void
region_coalescer_flush(kernel_region_coalescer_t *coalescer)
{
    kernel_memory_region_t *pending = &coalescer->pending;
    if (pending->prot != 0 && pending->size > 0) {
        coalescer->callback(pending, coalescer->arg);
    }
    *pending = (kernel_memory_region_t) { 0 };
}

static void
region_coalescer_add(kernel_region_coalescer_t *coalescer, unsigned long start,
                     unsigned long end, unsigned int prot)
{
    kernel_memory_region_t *pending = &coalescer->pending;
    if (prot == pending->prot && (unsigned long)pending->base + pending->size == start) {
        pending->size += end - start;
        return;
    }
    region_coalescer_flush(coalescer);
    *pending = (kernel_memory_region_t) {
        .base = (unsigned char *)start,
        .size = end - start,
        .prot = prot,
    };
}

static void
walk_code_range(unsigned long start, unsigned long end,
                kernel_memory_region_callback_t callback, void *arg)
{
    kernel_region_coalescer_t coalescer = { .callback = callback, .arg = arg };
    unsigned long addr = start;

    while (addr < end) {
        unsigned int prot;
        unsigned long next = query_kernel_page(addr, &prot);
        /* Clamp to |end|, including on wraparound. */
        if (next == 0 || next > end) {
            next = end;
        }
        region_coalescer_add(&coalescer, addr, next, prot);
        addr = next;
    }
    region_coalescer_flush(&coalescer);
}

void
kernel_walk_code_regions(kernel_memory_region_callback_t callback, void *arg)
{
    /* XXX i#8021: Executable memory mapped after this walk is not tracked. Neither is
     * memory unmapped after it (e.g., module unload, freed BPF JIT images), which
     * leaves stale regions behind. The walk also takes no page table locks, so
     * concurrent mapping or protection changes may yield missing or inaccurate regions.
     */
    const unsigned long start = KERNEL_CODE_WALK_START;
    const unsigned long end = KERNEL_CODE_WALK_END;

    /* DR's heap lies within this range and is mapped executable, but it's not kernel
     * code. It should be skipped here.
     */
    const unsigned long heap_start = (unsigned long)heap;
    const unsigned long heap_end = heap_start + heap_size;
    KERNEL_ASSERT(heap != NULL && start <= heap_start && heap_end <= end);

    walk_code_range(start, heap_start, callback, arg);
    walk_code_range(heap_end, end, callback, arg);
}

/* Returns whether the |size| bytes at |addr| can be read without faulting.  Probes one
 * byte per page, as read protections are page-granular.  The kernel's no-fault accessors
 * disable page faults and rely on the exception tables to recover, so this requires no
 * locks and never sleeps.  It is safe to call in any context, including in a crash
 * report.
 *
 * XXX i#8021: A probe of a device MMIO address performs a real read, which can have side
 * effects.  Callers only inspect ordinary memory today, so this is not a concern.
 */
bool
kernel_is_readable_without_fault(const void *addr, size_t size)
{
    if (size == 0) {
        return true;
    }

    /* Pointer overflow is undefined behavior and may be optimized away,
     * so we convert the pointer to integer first.
     */
    unsigned long cur = (unsigned long)addr;
    /* Clamp a range that would overflow. */
    const unsigned long last = cur + size < cur ? ULONG_MAX : cur + size - 1;
    const unsigned long last_page = ALIGN_BACKWARD(last, PAGE_SIZE);

    /* Probe the first requested byte, then the first byte of each subsequent page. */
    char dummy;
    unsigned long page = ALIGN_BACKWARD(cur, PAGE_SIZE);
    while (true) {
        /* copy_from_user_nofault rejects kernel addresses while copy_from_kernel_nofault
         * rejects user addresses, so we split on the boundary the kernel itself uses.
         * TASK_SIZE_MAX is page-aligned, so a page never straddles the split.
         */
        long res = cur < TASK_SIZE_MAX
            ? copy_from_user_nofault(&dummy, (const void __user *)cur, 1)
            : copy_from_kernel_nofault(&dummy, (const void *)cur, 1);
        if (res != 0) {
            return false;
        }
        if (page == last_page) {
            return true;
        }
        /* Advance to the first byte of next page. */
        page += PAGE_SIZE;
        cur = page;
    }
}

unsigned int
kernel_query_time_seconds(void)
{
    return (unsigned int)ktime_get_real_seconds();
}

unsigned long long
kernel_query_time_nanos(void)
{
    return ktime_get_real_fast_ns();
}

unsigned int
kernel_get_random_u32(void)
{
    return get_random_u32();
}

void
kernel_printk(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

void
kernel_panic(const char *message)
{
    panic("%s", message);
}

#define KERNEL_ENV_MAX 20

typedef struct {
    char name[KERNEL_ENV_NAME_MAX];
    char value[KERNEL_ENV_VALUE_MAX];
} kernel_env_t;

/* The env table is not protected by any lock of its own: it is only written during
 * session initialization, which the caller serializes, and read afterwards.
 */
static kernel_env_t env_vars[KERNEL_ENV_MAX];
static int env_count = 0;

int
kernel_setenv(const char *name, const char *value)
{
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL || value == NULL) {
        return -EINVAL;
    }
    if (strlen(name) >= KERNEL_ENV_NAME_MAX || strlen(value) >= KERNEL_ENV_VALUE_MAX) {
        return -E2BIG;
    }

    /* If name already exists in env_vars, overwrite the existing value. */
    for (int i = 0; i < env_count; i++) {
        if (strcmp(name, env_vars[i].name) == 0) {
            strscpy(env_vars[i].value, value, KERNEL_ENV_VALUE_MAX);
            return 0;
        }
    }

    /* If name doesn't exist in env_vars, try appending a new entry. */
    if (env_count >= KERNEL_ENV_MAX) {
        return -ENOSPC;
    }
    strscpy(env_vars[env_count].name, name, KERNEL_ENV_NAME_MAX);
    strscpy(env_vars[env_count].value, value, KERNEL_ENV_VALUE_MAX);
    env_count++;
    return 0;
}

const char *
kernel_getenv(const char *name)
{
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
        return NULL;
    }
    for (int i = 0; i < env_count; i++) {
        if (strcmp(name, env_vars[i].name) == 0) {
            return (const char *)env_vars[i].value;
        }
    }
    return NULL;
}
