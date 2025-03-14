/* **********************************************************
 * Copyright (c) 2011-2024 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef WINDOWS
# define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "dr_api.h"
#include "drmgr.h"
#include "callstack.h"
#include "utils.h"
#include "drsyms.h"
#include "drsymcache.h"
#ifdef WINDOWS
# include "windefs.h"
# include "../wininc/ndk_extypes.h" /* for SYSTEM_INFORMATION_CLASS */
#else
# include <string.h>
#  include <signal.h> /* for SIGSEGV */
#endif
#include <stddef.h> /* for offsetof */
#include <ctype.h> /* for tolower */

/* globals that affect NOTIFY* and *LOG* macros */
int tls_idx_util = -1;
bool op_print_stderr = true;
int op_verbose_level;
bool op_pause_at_assert;
bool op_pause_via_loop;
bool op_ignore_asserts;
uint op_prefix_style;
file_t f_global = INVALID_FILE;
int reported_disk_error;
bool op_use_symcache;
#ifdef STATISTICS
uint symbol_lookups;
uint symbol_searches;
uint symbol_lookup_cache_hits;
uint symbol_search_cache_hits;
uint symbol_address_lookups;
#endif

#ifdef WINDOWS
static dr_os_version_info_t os_version = {sizeof(os_version),};
#endif

#ifdef WINDOWS
/* for ensuring library isolation */
static PEB *priv_peb;
#endif

static thread_id_t primary_thread = INVALID_THREAD_ID;

/***************************************************************************
 * UTILITIES
 */

/* Mac has builtin -- though should verify in all builds. */
/* AArchXX has memset from DR memfuncs. */
#if !defined(MACOS) && !defined(AARCHXX)
/* FIXME: VC8 uses intrinsic memset yet has it call out, so /nodefaultlib
 * gets a link error missing _memset.  This does not help, nor does /Oi:
 *   #pragma intrinsic ( memset)
 * So have to provide our own memset:
 * But with /O2 it actually uses the intrinsic.
 */
# ifndef NDEBUG /* cmake Release build type */
void *
memset(void *dst, int val, size_t size)
{
    register unsigned char *ptr = (unsigned char *) dst;
    while (size-- > 0)
        *ptr++ = val;
    return dst;
}
# endif
#endif

void
wait_for_user(const char *message)
{
#ifdef WINDOWS
    dr_messagebox("%s in pid "PIDFMT"", message, dr_get_process_id());
#else
    if (op_pause_via_loop) {
        /* PR 406725: on Linux, infinite loop rather than waiting for stdin */
        bool forever = true; /* make it easy to break out in gdb */
        dr_fprintf(STDERR, "%s in pid "PIDFMT"\n", message, dr_get_process_id());
        dr_fprintf(STDERR, "<in infinite loop>\n");
        while (forever) {
            dr_thread_yield();
        }
    } else {
        char keypress;
        dr_fprintf(STDERR, "%s in pid "PIDFMT"\n", message, dr_get_process_id());
        dr_fprintf(STDERR, "<press enter to continue>\n");
        dr_read_file(stdin->IF_MACOS_ELSE(_file, IF_ANDROID_ELSE(_file, _fileno)),
                     &keypress, sizeof(keypress));
    }
#endif
}

void
drmemory_abort(void)
{
    if (op_pause_at_assert)
        wait_for_user("Dr. Memory is paused at an assert");
    dr_abort();
}

bool
safe_read(void *base, size_t size, void *out_buf)
{
    /* DR's dr_safe_read() is now faster than try/except.
     * History: this routine pre-dates dr_safe_read() itself, and
     * even once that was available it was too slow on Windows
     * as it involved an NtReadVirtualMemory syscall.
     * For all of our uses, a failure is rare, so we would rather
     * have low overhead on non-failure and have no syscall or
     * setjmp overhead (i#265).
     * Xref the same problem with leak_safe_read_heap (PR 570839).
     */
    return dr_safe_read(base, size, out_buf, NULL);
}

/* if returns false, calls instr_free() on inst first */
bool
safe_decode(void *drcontext, app_pc pc, instr_t *inst, app_pc *next_pc /*OPTIONAL OUT*/)
{
    app_pc nxt;
    DR_TRY_EXCEPT(drcontext, {
        nxt = decode(drcontext, pc, inst);
    }, { /* EXCEPT */
        /* in case decode filled something in before crashing */
        instr_free(drcontext, inst);
        return false;
    });
    if (next_pc != NULL)
        *next_pc = nxt;
    return true;
}

bool
lookup_has_fast_search(const module_data_t *mod)
{
    drsym_debug_kind_t kind;
    drsym_error_t res = drsym_get_module_debug_kind(mod->full_path, &kind);
    return res == DRSYM_SUCCESS && TEST(DRSYM_PDB, kind);
}

/* default cb used when we want first match */
static bool
search_syms_cb(drsym_info_t *info, drsym_error_t status, void *data)
{
    size_t *ans = (size_t *) data;
    LOG(3, "sym lookup cb: %s @ offs "PIFX"\n", info->name, info->start_offs);
    ASSERT(ans != NULL, "invalid param");
    *ans = info->start_offs;
    return false; /* stop iterating: we want first match */
}

#ifdef WINDOWS
/* XXX i#1465: we ensure SymFromName (answer passed in data) matches
 * SymSearch (this callback) and make it visible, until we've figured out
 * the underlying bug(s) behind symbol cache corruption.
 */
static bool
verify_lookup_cb(drsym_info_t *info, drsym_error_t status, void *data)
{
    size_t *ans = (size_t *) data;
    LOG(3, "verify lookup cb: %s "PIFX" vs "PIFX"\n", info->name, *ans, info->start_offs);
    ASSERT(ans != NULL, "invalid param");
    if (*ans != info->start_offs) {
        NOTIFY_ERROR("DBGHELP ERROR: mismatch for %s between SymFromName ("PIFX
                     ") and SymSearch ("PIFX")!"NL,
                     info->name, *ans, info->start_offs);
        dr_abort(); /* make sure we see this on bots */
    }
    return false; /* stop iterating: we want first match */
}
#endif

typedef struct _search_regex_t {
    const char *regex;
    drsym_enumerate_ex_cb orig_cb;
    void *orig_data;
} search_regex_t;

/* cb used when we need to do our own regex matching
 * XXX: would be better for drsyms to do this for us instead of
 * returning NYI on search, although we'd still have
 * multiple loops: for efficiency might need to pool together
 * what we're interested in from all modules and do one loop.
 * maybe each module registers pattern and gets callback from
 * some central module event code.
 */
static bool
search_syms_regex_cb(drsym_info_t *info, drsym_error_t status, void *data)
{
    search_regex_t *sr = (search_regex_t *) data;
    const char *sym = strchr(sr->regex, '!');
    const char *name = info->name;

    LOG(3, "%s: comparing %s to pattern |%s| (regex=|%s|)\n", __FUNCTION__,
        name, sym == NULL ? sr->regex : sym + 1, sr->regex);
    if (sr->regex[0] == '\0' ||
        (sym != NULL && *(sym+1) == '\0') ||
        text_matches_pattern(name, sym == NULL ? sr->regex : sym + 1, false)) {
        return sr->orig_cb(info, status, sr->orig_data);
    }
    return true; /* keep iterating */
}

static app_pc
lookup_symbol_common(const module_data_t *mod, const char *sym_pattern,
                     bool full, drsym_enumerate_ex_cb callback, void *data)
{
    /* We have to specify the module via "modname!symname".
     * We must use the same modname as in full_path.
     */
#define MAX_SYM_WITH_MOD_LEN 256
    char sym_with_mod[MAX_SYM_WITH_MOD_LEN];
    size_t modoffs;
    IF_DEBUG(int len;)
    drsym_error_t symres;
    char *fname = NULL, *c, *fend;

    if (mod->full_path == NULL || mod->full_path[0] == '\0'
        /* i#1803: handle special cases like "[vdso]" */
        IF_LINUX(|| mod->full_path[0] == '['))
        return NULL;
    if (callback == NULL) {
        if (op_use_symcache) {
            uint count;
            size_t *array, single;
            if (drsymcache_lookup(mod, sym_pattern, &array, &count,
                                  &single) == DRMF_SUCCESS) {
                /* if there are multiple we just return the first one */
                modoffs = array[0];
                drsymcache_free_lookup(array, count);
                STATS_INC(symbol_lookup_cache_hits);
                if (modoffs == 0)
                    return NULL;
                else
                    return mod->start + modoffs;
            }
        }
        STATS_INC(symbol_lookups); /* not total, rather un-cached */
    } else
        STATS_INC(symbol_searches);

    /* Now that we know this is a symcache miss, check if the module has
     * symbols and warn if it doesn't.  DrMemory also uses this to fetch
     * missing symbols at the end of the run.
     */
    module_check_for_symbols(mod->full_path);

    for (c = mod->full_path; *c != '\0'; c++) {
        if (*c == DIRSEP IF_WINDOWS(|| *c == ALT_DIRSEP))
            fname = c + 1;
    }
    fend = c;
    ASSERT(fname != NULL, "unable to get fname for module");
    if (fname == NULL)
        return NULL;
    /* now get rid of extension */
    for (; c > fname && *c != '.'; c--)
        ; /* nothing */
    if (c == fname) /* no extension: e.g., "/usr/lib/dyld" on MacOS */
        c = fend;
    if (c > fname) {
        ASSERT(c - fname < BUFFER_SIZE_ELEMENTS(sym_with_mod), "sizes way off");
        IF_DEBUG(len = )
            dr_snprintf(sym_with_mod, c - fname, "%s", fname);
        ASSERT(len == -1 || c == fend, "error printing modname!symname");
    }
    IF_DEBUG(len = )
        dr_snprintf(sym_with_mod + (c - fname),
                    BUFFER_SIZE_ELEMENTS(sym_with_mod) - (c - fname),
                    "!%s", sym_pattern);
    ASSERT(len > 0, "error printing modname!symname");
    IF_WINDOWS(ASSERT(using_private_peb(), "private peb not preserved"));

    /* We rely on drsym_init() having been called during init */
    if (callback == NULL IF_WINDOWS(&& full)) {
        /* A SymSearch full search is slower than SymFromName */
        symres = drsym_lookup_symbol(mod->full_path, sym_with_mod, &modoffs,
                                     DRSYM_DEMANGLE);
#ifdef WINDOWS
        /* i#1465: our theory to explain bogus symbols is that dbghelp is
         * giving them to us, so we live w/ the cost of a sanity check here
         * until we fix that bug.  Only a few queries come here: only one per
         * typical module (most go to SymSearch).
         */
        if (symres == DRSYM_SUCCESS) {
            drsym_error_t search_res =
                drsym_search_symbols_ex(mod->full_path, sym_with_mod,
                                        DRSYM_FULL_SEARCH | DRSYM_DEFAULT_FLAGS,
                                        verify_lookup_cb, sizeof(drsym_info_t), &modoffs);
            ASSERT(search_res == DRSYM_SUCCESS, "Search failed but FromName worked");
        }
#endif
    } else {
        /* drsym_search_symbols() is faster than either drsym_lookup_symbol() or
         * drsym_enumerate_symbols() (i#313)
         */
        modoffs = 0;
#ifdef WINDOWS
        /* i#1050: use drsym_search_symbols_ex to handle cases
         * where two functions share the same address.
         */
        symres = drsym_search_symbols_ex(mod->full_path, sym_with_mod,
                                         (full ? DRSYM_FULL_SEARCH : 0) |
                                         DRSYM_DEFAULT_FLAGS,
                                         callback == NULL ? search_syms_cb : callback,
                                         sizeof(drsym_info_t),
                                         callback == NULL ? &modoffs : data);
        if (symres == DRSYM_ERROR_NOT_IMPLEMENTED) {
#endif
            /* ELF or PECOFF where regex search is NYI
             * XXX: better for caller to do single walk though: should we
             * return failure and have caller call back with "" and its own
             * pattern-matching callback?
             */
            search_regex_t *sr = (search_regex_t *)
                global_alloc(sizeof(*sr), HEAPSTAT_MISC);
            sr->regex = sym_with_mod;
            sr->orig_cb = callback == NULL ? search_syms_cb : callback;
            sr->orig_data = callback == NULL ? &modoffs : data;
            symres = drsym_enumerate_symbols_ex(mod->full_path,
                                                search_syms_regex_cb,
                                                sizeof(drsym_info_t),
                                                (void *) sr, DRSYM_DEMANGLE);
            global_free(sr, sizeof(*sr), HEAPSTAT_MISC);
#ifdef WINDOWS
        }
#endif
    }
    LOG(2, "sym lookup of %s in %s => %d "PFX"\n", sym_with_mod, mod->full_path,
        symres, modoffs);
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
        if (callback == NULL) {
            if (op_use_symcache)
                drsymcache_add(mod, sym_pattern, modoffs);
            if (modoffs == 0) /* using as sentinel: assuming no sym there */
                return NULL;
            else
                return mod->start + modoffs;
        } else /* non-null to indicate success */
            return mod->start;
    } else {
        if (symres == DRSYM_ERROR_SYMBOL_NOT_FOUND && op_use_symcache)
            drsymcache_add(mod, sym_pattern, 0);
        return NULL;
    }
}

app_pc
lookup_symbol(const module_data_t *mod, const char *symname)
{
    return lookup_symbol_common(mod, symname, false, NULL, NULL);
}

app_pc
lookup_internal_symbol(const module_data_t *mod, const char *symname)
{
    return lookup_symbol_common(mod, symname, true, NULL, NULL);
}

/* Iterates over symbols matching modname!sym_pattern until
 * callback returns false.
 * N.B.: if you add a call to this routine, or modify an existing call,
 * bump SYMCACHE_VERSION and add symcache checks.
 */
bool
lookup_all_symbols(const module_data_t *mod, const char *sym_pattern, bool full,
                   drsym_enumerate_ex_cb callback, void *data)
{
    return (lookup_symbol_common(mod, sym_pattern, full, callback, data) != NULL);
}

bool
module_has_debug_info(const module_data_t *mod)
{
    /* Since we don't care whether line #s are avail we don't need
     * to call the slower drsym_get_module_debug_kind()
     */
    return (drsym_module_has_symbols(mod->full_path) == DRSYM_SUCCESS);
}

#ifdef DEBUG
void
print_mcontext(file_t f, dr_mcontext_t *mc)
{
# ifdef X86
    dr_fprintf(f, "\txax="PFX", xbx="PFX", xcx="PFX", xdx="PFX"\n"
               "\txsi="PFX", xdi="PFX", xbp="PFX", xsp="PFX"\n",
               mc->xax, mc->xbx, mc->xcx, mc->xdx,
               mc->xsi, mc->xdi, mc->xbp, mc->xsp);
# elif defined(ARM)
    dr_fprintf(f, "\tr0="PFX", r1="PFX", r2="PFX", r3="PFX"\n"
               "\tr4="PFX", r5="PFX", r6="PFX", r7="PFX"\n",
               "\tr8="PFX", r9="PFX", r10="PFX", r11="PFX"\n",
               "\tr12="PFX", sp="PFX", lr="PFX", pc="PFX"\n",
               mc->r0, mc->r1, mc->r2, mc->r3,
               mc->r4, mc->r5, mc->r6, mc->r7,
               mc->r8, mc->r9, mc->r10, mc->r11,
               mc->r12, mc->sp, mc->lr, mc->pc);
# endif
}
#endif

void
hashtable_delete_with_stats(hashtable_t *table, const char *name)
{
    LOG(1, "final %s table size: %u bits, %u entries\n", name,
        table->table_bits, table->entries);
    /* XXX: add collision data: though would want those stats mid-run
     * for tables that have entries freed during exit before here
     */
    hashtable_delete(table);
}

#ifdef STATISTICS
void
hashtable_cluster_stats(hashtable_t *table, const char *name)
{
    uint max_cluster = 0, tot_cluster = 0, count_cluster = 0;
    uint i;
    for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
        hash_entry_t *he;
        uint cluster = 0;
        if (table->table[i] != NULL)
            count_cluster++;
        for (he = table->table[i]; he != NULL; he = he->next)
            cluster++;
        if (cluster > max_cluster)
            max_cluster = cluster;
        tot_cluster += cluster;
    }
    /* we don't want to use floating point so we print count and tot */
    LOG(0, "%s table: clusters=%u max=%u tot=%u\n",
        name, count_cluster, max_cluster, tot_cluster);
}
#endif

void
print_prefix_to_buffer(char *buf, size_t bufsz, size_t *sofar)
{
    void *drcontext = dr_get_current_drcontext();
    ssize_t len;
    if (op_prefix_style == PREFIX_STYLE_NONE) {
        BUFPRINT_NO_ASSERT(buf, bufsz, *sofar, len, "");
        return;
    } else if (op_prefix_style == PREFIX_STYLE_BLANK) {
        BUFPRINT_NO_ASSERT(buf, bufsz, *sofar, len, "%s", PREFIX_BLANK);
        return;
    } else if (drcontext != NULL) {
        thread_id_t tid = dr_get_thread_id(drcontext);
        if (primary_thread != INVALID_THREAD_ID/*initialized?*/ &&
            tid != primary_thread) {
            /* no-assert since used for errors, etc. in fragile contexts */
            BUFPRINT_NO_ASSERT(buf, bufsz, *sofar, len, "~~%d~~ ", tid);
            return;
        }
    }
    /* no-assert since used for errors, etc. in fragile contexts */
    BUFPRINT_NO_ASSERT(buf, bufsz, *sofar, len, "%s", PREFIX_DEFAULT_MAIN_THREAD);
}

void
print_prefix_to_console(void)
{
    char buf[16];
    size_t sofar = 0;
    print_prefix_to_buffer(buf, BUFFER_SIZE_ELEMENTS(buf), &sofar);
    PRINT_CONSOLE("%s", buf);
}

#ifdef WINDOWS
bool
print_to_cmd(char *buf)
{
    /* we avoid buffer size limits in dr_fprintf by directly
     * talking to kernel32 ourselves
     */
    uint written;
    if (!WriteFile(GetStdHandle(STD_ERROR_HANDLE),
                   buf, (DWORD) strlen(buf), (LPDWORD) &written, NULL))
        return false;
    return true;
}
#endif

bool
unsigned_multiply_will_overflow(size_t m, size_t n)
{
    /* There are several methods, but we'd like to avoid a divide (for performance,
     * and for future ARM support where divides need software support).
     * XXX: should we just do assembly "mul; seto al"?
     */
#ifdef X64
    /* First split m and n into their top and bottom 32-bit words: {m,n}{1,0}.
     * Now m*n == ((m1<<32)+m0)*((n1<<32)+n0)
     *         == (m1<<32)*(n1<<32) + (m1<<32)*n0 + m0*(n1<<32) + m0*n0
     *         == (m1*n1)<<64 + (m1*n0 + m0*n1)<<32 + m0*n0
     */
    size_t m0 = m & 0xffffffffU;
    size_t m1 = (m >> 32) & 0xffffffffU;
    size_t n0 = n & 0xffffffffU;
    size_t n1 = (n >> 32) & 0xffffffffU;
    uint64 middle;
    /* Will any one term overflow on its own?
     * First one must be zero before the <<64:
     */
    if (m1 > 0 && n1 > 0)
        return true;
    /* Either m1 or n1 is zero, so add can't overflow, and <<32 => cmp to UINT_MAX: */
    middle = (uint64)m1*n0 + (uint64)m0*n1;
    if (middle > UINT_MAX)
        return true;
    /* Ensure final sum doesn't overflow */
    if (middle + (uint64)m0*n0 < middle)
        return true;
    return false;
#else
    /* Easiest and fastest to just do a 64-bit multiply */
    uint64 ans = (uint64)m * n;
    return (ans > UINT_MAX);
#endif
}

void
crash_process(void)
{
#ifdef WINDOWS
    dr_exit_process(STATUS_ACCESS_VIOLATION);
#else
    dr_exit_process(SIGSEGV << 8);
#endif
}

/***************************************************************************
 * STRING PARSING
 *
 */

bool
text_matches_pattern(const char *text, const char *pattern,
                     bool ignore_case)
{
    /* Match text with pattern and return the result.
     * The pattern may contain '*' and '?' wildcards.
     */
    const char *cur_text = text,
               *text_last_asterisk = NULL,
               *pattern_last_asterisk = NULL;
    char cmp_cur, cmp_pat;
    while (*cur_text != '\0') {
        cmp_cur = *cur_text;
        cmp_pat = *pattern;
        if (ignore_case) {
            /* XXX DRi#943: toupper is better, for int18n, and we need to call
             * islower() first to be safe for all tolower() implementations.
             * Even better would be switching to our own locale-independent case
             * folding.
             */
            cmp_cur = (char) tolower(cmp_cur);
            cmp_pat = (char) tolower(cmp_pat);
        }
        if (*pattern == '*') {
            while (*++pattern == '*') {
                /* Skip consecutive '*'s */
            }
            if (*pattern == '\0') {
                /* the pattern ends with a series of '*' */
                LOG(5, "    text_matches_pattern \"%s\" == \"%s\"\n", text, pattern);
                return true;
            }
            text_last_asterisk = cur_text;
            pattern_last_asterisk = pattern;
        } else if ((cmp_cur == cmp_pat) || (*pattern == '?')) {
            ++cur_text;
            ++pattern;
        } else if (text_last_asterisk != NULL) {
            /* No match. But we have seen at least one '*', so go back
             * and try at the next position.
             */
            pattern = pattern_last_asterisk;
            cur_text = text_last_asterisk++;
        } else {
            LOG(5, "    text_matches_pattern \"%s\" != \"%s\"\n", text, pattern);
            return false;
        }
    }
    while (*pattern == '*')
        ++pattern;
    LOG(4, "    text_matches_pattern \"%s\": end at \"%.5s\"\n", text, pattern);
    return *pattern == '\0';
}

/* patterns is a null-separated, double-null-terminated list of strings */
bool
text_matches_any_pattern(const char *text, const char *patterns, bool ignore_case)
{
    const char *c = patterns;
    while (*c != '\0') {
        if (text_matches_pattern(text, c, ignore_case))
            return true;
        c += strlen(c) + 1;
    }
    return false;
}

/* patterns is a null-separated, double-null-terminated list of strings */
const char *
text_contains_any_string(const char *text, const char *patterns, bool ignore_case,
                         const char **matched)
{
    const char *c = patterns;
    while (*c != '\0') {
        const char *match =
            ignore_case ? strcasestr(text, c) : strstr(text, c);
        if (match != NULL) {
            if (matched != NULL)
                *matched = c;
            return match;
        }
        c += strlen(c) + 1;
    }
    return NULL;
}

/***************************************************************************
 * WINDOWS SYSTEM CALLS
 */

#ifdef WINDOWS
typedef enum _THREADINFOCLASS {
    ThreadBasicInformation,
    ThreadTimes,
    ThreadPriority,
    ThreadBasePriority,
    ThreadAffinityMask,
    ThreadImpersonationToken,
    ThreadDescriptorTableEntry,
    ThreadEnableAlignmentFaultFixup,
    ThreadEventPair_Reusable,
    ThreadQuerySetWin32StartAddress,
    ThreadZeroTlsCell,
    ThreadPerformanceCount,
    ThreadAmILastThread,
    ThreadIdealProcessor,
    ThreadPriorityBoost,
    ThreadSetTlsArrayAddress,
    ThreadIsIoPending,
    ThreadHideFromDebugger,
    MaxThreadInfoClass
} THREADINFOCLASS;

typedef LONG KPRIORITY;

typedef struct _THREAD_BASIC_INFORMATION { // Information Class 0
    NTSTATUS ExitStatus;
    PNT_TIB TebBaseAddress;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    KPRIORITY Priority;
    KPRIORITY BasePriority;
} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;

#define InitializeObjectAttributes( p, n, a, r, s ) {   \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }

#define OBJ_CASE_INSENSITIVE    0x00000040L

GET_NTDLL(NtQueryInformationThread, (DR_PARAM_IN HANDLE ThreadHandle,
                                     DR_PARAM_IN THREADINFOCLASS ThreadInformationClass,
                                     DR_PARAM_OUT PVOID ThreadInformation,
                                     DR_PARAM_IN ULONG ThreadInformationLength,
                                     DR_PARAM_OUT PULONG ReturnLength OPTIONAL));

GET_NTDLL(NtOpenThread, (DR_PARAM_OUT PHANDLE ThreadHandle,
                         DR_PARAM_IN ACCESS_MASK DesiredAccess,
                         DR_PARAM_IN POBJECT_ATTRIBUTES ObjectAttributes,
                         DR_PARAM_IN PCLIENT_ID ClientId));

TEB *
get_TEB(void)
{
#ifdef X64
    return (TEB *) __readgsqword(offsetof(TEB, Self));
#else
    return (TEB *) __readfsdword(offsetof(TEB, Self));
#endif
}

TEB *
get_TEB_from_handle(HANDLE h)
{
    uint got;
    THREAD_BASIC_INFORMATION info;
    NTSTATUS res;
    memset(&info, 0, sizeof(THREAD_BASIC_INFORMATION));
    res = NtQueryInformationThread(h, ThreadBasicInformation,
                                   &info, sizeof(THREAD_BASIC_INFORMATION), &got);
    if (!NT_SUCCESS(res) || got != sizeof(THREAD_BASIC_INFORMATION)) {
        ASSERT(false, "internal error");
        return NULL;
    }
    return (TEB *) info.TebBaseAddress;
}

thread_id_t
get_tid_from_handle(HANDLE h)
{
    uint got;
    THREAD_BASIC_INFORMATION info;
    NTSTATUS res;
    memset(&info, 0, sizeof(THREAD_BASIC_INFORMATION));
    res = NtQueryInformationThread(h, ThreadBasicInformation,
                                   &info, sizeof(THREAD_BASIC_INFORMATION), &got);
    if (!NT_SUCCESS(res) || got != sizeof(THREAD_BASIC_INFORMATION)) {
        LOG(1, "%s: failed with 0x%08x %d vs %d\n", __FUNCTION__,
            res, got, sizeof(THREAD_BASIC_INFORMATION));
        return INVALID_THREAD_ID;
    }
    return (thread_id_t) info.ClientId.UniqueThread;
}

TEB *
get_TEB_from_tid(thread_id_t tid)
{
    HANDLE h;
    TEB *teb = NULL;
    NTSTATUS res;
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID cid;
    /* i#1254: this will fail in a sandboxed process */
    ASSERT(false, "use get_TEB_from_handle(dr_get_dr_thread_handle(drcontext)) instead!");
    /* these aren't really HANDLEs */
    cid.UniqueProcess = (HANDLE) dr_get_process_id();
    cid.UniqueThread = (HANDLE) tid;
    InitializeObjectAttributes(&oa, NULL, OBJ_CASE_INSENSITIVE, NULL, NULL);
    res = NtOpenThread(&h, THREAD_QUERY_INFORMATION, &oa, &cid);
    if (NT_SUCCESS(res)) {
        teb = get_TEB_from_handle(h);
        /* avoid DR's hook on NtClose: dr_close_file() calls the raw version */
        dr_close_file(h);
    } else {
        WARN("WARNING: get_TEB_from_tid tid=%d failed "PFX"\n", tid, res);
    }
    return teb;
}

void
set_app_error_code(void *drcontext, uint val)
{
    TEB *teb;
    bool swapped = false;
    if (!dr_using_app_state(drcontext)) {
        swapped = true;
        dr_switch_to_app_state(drcontext);
    }
    teb = get_TEB();
    teb->LastErrorValue = val;
    if (swapped)
        dr_switch_to_dr_state(drcontext);
}

PEB *
get_app_PEB(void)
{
    /* With i#249, in DrMem code the PEB pointed at by the TEB is DR's private
     * copy, so we query DR to get the app's PEB.
     * Note that NtQueryInformationProcess, disturbingly, returns the pointer
     * in the TEB, so we can't use that!
     * Update: I'm not seeing NtQueryInformationProcess do that: for i#5
     * it returns the system PEB, not TEB->ProcessEnvironmentBlock.
     * Does it do different things at different times, or was I measuring on
     * different OS's?  The latter is on xp64.
     */
    return (PEB *) dr_get_app_PEB();
}

#ifdef DEBUG
/* check that peb isolation is consistently applied (xref i#324) */
bool
using_private_peb(void)
{
    TEB *teb = get_TEB();
    return (teb != NULL && teb->ProcessEnvironmentBlock == priv_peb);
}
#endif

HANDLE
get_private_heap_handle(void)
{
    return (HANDLE) priv_peb->ProcessHeap;
}

HANDLE
get_process_heap_handle(void)
{
    return (HANDLE) get_app_PEB()->ProcessHeap;
}

bool
is_current_process(HANDLE h)
{
    bool res = false;
    /* if it fails, assume NOT cur process since usually would use NT_CURRENT_PROCESS */
    return (drsys_handle_is_current_process(h, &res) == DRMF_SUCCESS && res);
}

bool
is_wow64_process(void)
{
    /* Another feature DR now provides for us */
    return dr_is_wow64();
}

const wchar_t *
get_app_commandline(void)
{
    PEB *peb = get_app_PEB();
    if (peb != NULL) {
        RTL_USER_PROCESS_PARAMETERS *param = (RTL_USER_PROCESS_PARAMETERS *)
            peb->ProcessParameters;
        if (param != NULL) {
            return param->CommandLine.Buffer;
        }
    }
    return L"";
}

int
sysnum_from_name(const char *name)
{
    drsys_syscall_t *syscall;
    drsys_sysnum_t sysnum;
    if (drsys_name_to_syscall(name, &syscall) == DRMF_SUCCESS &&
        drsys_syscall_number(syscall, &sysnum) == DRMF_SUCCESS) {
        ASSERT(sysnum.secondary == 0, "should only query primary");
        return sysnum.number;
    }
    return -1;
}

bool
get_sysnum(const char *name, drsys_sysnum_t *var, bool ok_to_fail)
{
    drsys_syscall_t *syscall;
    if (drsys_name_to_syscall(name, &syscall) != DRMF_SUCCESS ||
        drsys_syscall_number(syscall, var) != DRMF_SUCCESS) {
        /* As assert here does not play well with -ignore_kernel so we downgrade
         * to a warning.
         */
        DOLOG(1, {
            if (!ok_to_fail)
                WARN("WARNING: Failed to find required syscall %s\n", name);
        });
        return false;
    }
    return true;
}

static void
init_os_version(void)
{
    if (!dr_get_os_version(&os_version)) {
        ASSERT(false, "unable to get Windows version");
        /* assume latest just to make progress: good chance of working */
        os_version.version = DR_WINDOWS_VERSION_10_1803;
        os_version.service_pack_major = 1;
        os_version.service_pack_minor = 0;
        /* Make it clear we don't know these fields: */
        os_version.build_number = 0;
        os_version.release_id[0] = '\0';
        os_version.edition[0] = '\0';
    }
}

bool
running_on_Win7_or_later(void)
{
    if (os_version.version == 0)
        init_os_version();
    return (os_version.version >= DR_WINDOWS_VERSION_7);
}

bool
running_on_Win7SP1_or_later(void)
{
    if (os_version.version == 0)
        init_os_version();
    return (os_version.version >= DR_WINDOWS_VERSION_7 &&
            os_version.service_pack_major >= 1);
}

bool
running_on_Vista_or_later(void)
{
    if (os_version.version == 0)
        init_os_version();
    return (os_version.version >= DR_WINDOWS_VERSION_VISTA);
}

dr_os_version_t
get_windows_version(void)
{
    if (os_version.version == 0)
        init_os_version();
    return os_version.version;
}

void
get_windows_version_string(char *buf DR_PARAM_OUT, size_t bufsz)
{
    if (os_version.version == 0)
        init_os_version();
    dr_snprintf(buf, bufsz, "WinVer=%u;Rel=%s;Build=%u;Edition=%s",
                os_version.version, os_version.release_id, os_version.build_number,
                os_version.edition);
    buf[bufsz - 1] = '\0';
}

GET_NTDLL(NtQuerySystemInformation, (DR_PARAM_IN  SYSTEM_INFORMATION_CLASS info_class,
                                     DR_PARAM_OUT PVOID  info,
                                     DR_PARAM_IN  ULONG  info_size,
                                     DR_PARAM_OUT PULONG bytes_received));

app_pc
get_highest_user_address(void)
{
    static app_pc highest_user_address;
    if (highest_user_address == NULL) {
        SYSTEM_BASIC_INFORMATION info;
        ULONG got;
        NTSTATUS res = NtQuerySystemInformation(SystemBasicInformation, &info,
                                                sizeof(info), &got);
        if (NT_SUCCESS(res) && got == sizeof(info))
            highest_user_address = (app_pc) info.HighestUserAddress;
        else
            highest_user_address = (app_pc) POINTER_MAX;
    }
    return highest_user_address;
}

bool
module_imports_from_msvc(const module_data_t *mod)
{
    bool res = false;
    dr_module_import_iterator_t *iter =
        dr_module_import_iterator_start(mod->handle);
# ifdef DEBUG
    const char *modname = dr_module_preferred_name(mod);
    if (modname == NULL)
        modname = "";
# endif
    while (dr_module_import_iterator_hasnext(iter)) {
        dr_module_import_t *imp = dr_module_import_iterator_next(iter);
        LOG(3, "module %s imports from %s\n", modname, imp->modname);
        if (text_matches_pattern(imp->modname, "msvc*.dll", FILESYS_CASELESS)) {
            res = true;
            break;
        }
    }
    dr_module_import_iterator_stop(iter);
    return res;
}

#endif /* WINDOWS */

reg_t
syscall_get_param(void *drcontext, uint num)
{
    reg_t res;
    if (drsys_pre_syscall_arg(drcontext, num, &res) != DRMF_SUCCESS) {
        ASSERT(false, "failed to get arg");
        res = 0;
    }
    return res;
}

/***************************************************************************
 * HEAP WITH STATS
 *
 */

#ifdef STATISTICS
/* We could have each client define this to avoid ifdefs in common/,
 * but the shared hashtable code needs a shared define, so going w/
 * ifdefs.
 */
static const char * heapstat_names[] = {
    "shadow",
    "perbb",
# ifdef TOOL_DR_HEAPSTAT
    "snapshot",
    "staleness",
# endif
    "callstack",
    "hashtable",
    "gencode",
    "rbtree",
    "suppress",
    "wrap/replace",
    "misc",
};

static uint heap_usage[HEAPSTAT_NUMTYPES];  /* cur usage  */
static uint heap_max[HEAPSTAT_NUMTYPES];    /* peak usage */
static uint heap_count[HEAPSTAT_NUMTYPES];  /* # allocs   */

static void
heap_usage_inc(heapstat_t type, size_t size)
{
    uint usage;
    uint delta;
    ASSERT_TRUNCATE(delta, uint, size);
    delta = (uint)size;
    ATOMIC_ADD32(heap_usage[type], delta);
    /* racy: if a problem in practice we can switch to per-thread stats */
    usage = heap_usage[type];
    if (usage > heap_max[type])
        heap_max[type] = usage;
    ATOMIC_INC32(heap_count[type]);
}

static void
heap_usage_dec(heapstat_t type, size_t size)
{
    int delta;
    ASSERT_TRUNCATE(delta, int, size);
    delta = (int)size;
    ATOMIC_ADD32(heap_usage[type], -delta);
    ATOMIC_DEC32(heap_count[type]);
}

void
heap_dump_stats(file_t f)
{
    int i;
    dr_fprintf(f, "\nHeap usage:\n");
    for (i = 0; i < HEAPSTAT_NUMTYPES; i++) {
        dr_fprintf(f, "\t%12s: count=%8u, cur=%6u %s, max=%6u KB\n",
                   heapstat_names[i], heap_count[i],
                   (heap_usage[i] > 8192) ? heap_usage[i]/1024 : heap_usage[i],
                   (heap_usage[i] > 8192) ? "KB" : " B",
                   heap_max[i]/1024);
    }
}
#endif /* STATISTICS */

#undef dr_global_alloc
#undef dr_global_free
#undef dr_thread_alloc
#undef dr_thread_free
#undef dr_nonheap_alloc
#undef dr_nonheap_free

void *
global_alloc(size_t size, heapstat_t type)
{
#ifdef STATISTICS
    heap_usage_inc(type, size);
#endif
    /* Note that the recursive lock inside DR is a perf hit for
     * malloc-intensive apps: we're already holding the malloc_lock,
     * so could use own heap alloc, or add option to DR to not use
     * lock yet still use thread-shared heap.
     */
    return dr_global_alloc(size);
}

void
global_free(void *p, size_t size, heapstat_t type)
{
#ifdef STATISTICS
    heap_usage_dec(type, size);
#endif
    dr_global_free(p, size);
}

void *
thread_alloc(void *drcontext, size_t size, heapstat_t type)
{
#ifdef STATISTICS
    heap_usage_inc(type, size);
#endif
    return dr_thread_alloc(drcontext, size);
}

void
thread_free(void *drcontext, void *p, size_t size, heapstat_t type)
{
#ifdef STATISTICS
    heap_usage_dec(type, size);
#endif
    dr_thread_free(drcontext, p, size);
}

void *
nonheap_alloc(size_t size, uint prot, heapstat_t type)
{
#ifdef STATISTICS
    heap_usage_inc(type, size);
#endif
    return dr_nonheap_alloc(size, prot);
}

void
nonheap_free(void *p, size_t size, heapstat_t type)
{
#ifdef STATISTICS
    heap_usage_dec(type, size);
#endif
    dr_nonheap_free(p, size);
}

#define dr_global_alloc DO_NOT_USE_use_global_alloc
#define dr_global_free  DO_NOT_USE_use_global_free
#define dr_thread_alloc DO_NOT_USE_use_thread_alloc
#define dr_thread_free  DO_NOT_USE_use_thread_free
#define dr_nonheap_alloc DO_NOT_USE_use_nonheap_alloc
#define dr_nonheap_free  DO_NOT_USE_use_nonheap_free

/***************************************************************************
 * REGISTER CONVERSION UTILITIES
 */

#ifdef X86
static reg_id_t
reg_32_to_8h(reg_id_t reg)
{
    ASSERT(reg >= REG_EAX && reg <= REG_EBX,
           "reg_32_to_8h: passed non-32-bit a-d reg");
    return (reg - REG_EAX) + REG_AH;
}
#endif

reg_id_t
reg_ptrsz_to_32(reg_id_t reg)
{
#ifdef X64
    ASSERT(reg >= DR_REG_START_64 && reg <= DR_REG_STOP_64,
           "wrong register for conversion");
    return reg_64_to_32(reg);
#else
    ASSERT(reg >= DR_REG_START_32 && reg < DR_REG_STOP_32,
           "wrong register for conversion");
    return reg;
#endif
}

reg_id_t
reg_ptrsz_to_16(reg_id_t reg)
{
#ifdef X64
    ASSERT(reg >= DR_REG_START_64 && reg <= DR_REG_STOP_64,
           "wrong register for conversion");
    reg = reg_64_to_32(reg);
#else
    ASSERT(reg >= DR_REG_START_32 && reg < DR_REG_STOP_32,
           "wrong register for conversion");
#endif
    return reg_32_to_16(reg);
}

reg_id_t
reg_ptrsz_to_8(reg_id_t reg)
{
#ifdef X64
    ASSERT(reg >= DR_REG_START_64 && reg <= DR_REG_STOP_64,
           "wrong register for conversion");
    reg = reg_64_to_32(reg);
#else
    ASSERT(reg >= DR_REG_START_32 && reg < DR_REG_STOP_32,
           "wrong register for conversion");
#endif
    return reg_32_to_8(reg);
}

#ifdef X86
reg_id_t
reg_ptrsz_to_8h(reg_id_t reg)
{
    ASSERT(reg >= DR_REG_XAX && reg <= DR_REG_XBX,
           "wrong register for conversion");
# ifdef X64
    reg = reg_64_to_32(reg);
# endif
    return reg_32_to_8h(reg);
}
#endif

reg_id_t
reg_to_size(reg_id_t reg, opnd_size_t size)
{
    reg_id_t ptrsz = reg_to_pointer_sized(reg);
    if (size == OPSZ_1)
        return reg_ptrsz_to_8(ptrsz);
    else if (size == OPSZ_2)
        return reg_ptrsz_to_16(ptrsz);
    else if (size == OPSZ_4)
        return IF_X64_ELSE(reg_64_to_32(ptrsz), ptrsz);
#ifdef X64
    else if (size == OPSZ_8)
        return ptrsz;
#endif
    else {
        ASSERT(false, "invalid target reg size");
        return DR_REG_NULL;
    }
}

/***************************************************************************
 * APP INSTRS
 */

/* Return prev app (non-meta) instr or NULL if no prev app instr.
 * It gives warning on seeing any non-label meta instructions
 * if called during app2app or analysis phases.
 */
instr_t *
instr_get_prev_app_instr(instr_t *instr)
{
    ASSERT(instr != NULL, "instr must not be NULL");
    instr = instr_get_prev(instr);
    /* quick check to avoid loop overhead */
    if (instr == NULL || instr_is_app(instr))
        return instr;
    for (; instr != NULL; instr = instr_get_prev(instr)) {
        if (instr_is_meta(instr)) {
            if (!instr_is_label(instr) &&
                (drmgr_current_bb_phase(dr_get_current_drcontext()) ==
                 DRMGR_PHASE_APP2APP ||
                 drmgr_current_bb_phase(dr_get_current_drcontext()) ==
                 DRMGR_PHASE_ANALYSIS))
                WARN("WARNING: see non-label non-app instruction.\n");
            continue;
        }
        return instr;
    }
    return NULL;
}

/* Return next non-meta (app) instr or NULL if no next app instr.
 * It gives warning on seeing any non-label meta instructions
 * if called during app2app or analysis phases.
 */
instr_t *
instr_get_next_app_instr(instr_t *instr)
{
    ASSERT(instr != NULL, "instr must not be NULL");
    instr = instr_get_next(instr);
    /* quick check to avoid loop overhead */
    if (instr == NULL || instr_is_app(instr))
        return instr;
    for (; instr != NULL; instr = instr_get_next(instr)) {
        if (instr_is_meta(instr)) {
            if (!instr_is_label(instr) &&
                (drmgr_current_bb_phase(dr_get_current_drcontext()) ==
                 DRMGR_PHASE_APP2APP ||
                 drmgr_current_bb_phase(dr_get_current_drcontext()) ==
                 DRMGR_PHASE_ANALYSIS))
                WARN("WARNING: see non-label meta instruction.\n");
            continue;
        }
        return instr;
    }
    return NULL;
}

instr_t *
instrlist_first_app_instr(instrlist_t *ilist)
{
    instr_t *instr;
    ASSERT(ilist != NULL, "instrlist must not be NULL");
    instr = instrlist_first(ilist);
    ASSERT(instr != NULL, "instrlist is empty");
    if (instr == NULL || instr_is_app(instr))
        return instr;
    return instr_get_next_app_instr(instr);
}

instr_t *
instrlist_last_app_instr(instrlist_t *ilist)
{
    instr_t *instr;
    ASSERT(ilist != NULL, "instrlist must not be NULL");
    instr = instrlist_last(ilist);
    ASSERT(instr != NULL, "instrlist is empty");
    if (instr == NULL || instr_is_app(instr))
        return instr;
    return instr_get_prev_app_instr(instr);
}

/***************************************************************************
 * HASHTABLE
 */

/* hashtable was moved and generalized */

static void *
hashwrap_alloc(size_t size)
{
    return global_alloc(size, HEAPSTAT_HASHTABLE);
}

static void
hashwrap_free(void *ptr, size_t size)
{
    global_free(ptr, size, HEAPSTAT_HASHTABLE);
}

static void
hashwrap_assert_fail(const char *msg)
{
    /* The reported file+line won't be the hashtable.c source but we
     * don't want the complexity of snprintf, and msg should identify
     * the source
     */
    ASSERT(false, msg);
}

/***************************************************************************
 * INIT/EXIT
 */

/* Must be called before drmgr or drwrap is initialized, so we allocate all
 * hashtables in the same way for our heap stats.
 */
void
utils_early_init(void)
{
    hashtable_global_config(hashwrap_alloc, hashwrap_free, hashwrap_assert_fail);
}

void
utils_init(void)
{
    tls_idx_util = drmgr_register_tls_field();
    ASSERT(tls_idx_util > -1, "failed to obtain TLS slot");

#ifdef WINDOWS
    if (os_version.version == 0)
        init_os_version();
#endif

    if (drsym_init(IF_WINDOWS_ELSE(NULL, 0)) != DRSYM_SUCCESS) {
        LOG(1, "WARNING: unable to initialize symbol translation\n");
    }

#ifdef WINDOWS
    /* store private peb and check later that it's the same (xref i#324) */
    ASSERT(get_TEB() != NULL, "can't get TEB");
    priv_peb = get_TEB()->ProcessEnvironmentBlock;
#endif

    primary_thread = dr_get_thread_id(dr_get_current_drcontext());
}

void
utils_exit(void)
{
    if (drsym_exit() != DRSYM_SUCCESS) {
        LOG(1, "WARNING: error cleaning up symbol library\n");
    }
    drmgr_unregister_tls_field(tls_idx_util);
}

void
utils_thread_init(void *drcontext)
{
    tls_util_t *pt = (tls_util_t *) thread_alloc(drcontext, sizeof(*pt), HEAPSTAT_MISC);
    memset(pt, 0, sizeof(*pt));
    drmgr_set_tls_field(drcontext, tls_idx_util, (void *) pt);
}

void
utils_thread_exit(void *drcontext)
{
    tls_util_t *pt = (tls_util_t *) drmgr_get_tls_field(drcontext, tls_idx_util);
    /* with PR 536058 we do have dcontext in exit event so indicate explicitly
     * that we've cleaned up the per-thread data
     */
    drmgr_set_tls_field(drcontext, tls_idx_util, NULL);
    thread_free(drcontext, pt, sizeof(*pt), HEAPSTAT_MISC);
}

void
utils_thread_set_file(void *drcontext, file_t f)
{
    tls_util_t *pt = (tls_util_t *) drmgr_get_tls_field(drcontext, tls_idx_util);
    pt->f = f;
}
