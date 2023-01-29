/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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

/* support needed to create a standalone static decoding library that
 * includes only the decode+encode routines from DR (i#617)
 */

/***************************************************************************/
#ifdef STANDALONE_DECODER /* around whole file */

#    include "../globals.h"
#    include "instr.h"
#    include "opnd.h"

#    ifdef UNIX
#        include <unistd.h>
#    endif

/* initialize to all zeros.  disassemble_set_syntax will write to it. */
options_t dynamo_options;

#    undef STDERR

/* support use of STD* macros so user doesn't have to use "stdout->_fileno" */
#    ifdef UNIX
file_t our_stdout = STDOUT_FILENO;
file_t our_stderr = STDERR_FILENO;
file_t our_stdin = STDIN_FILENO;

#        define STDERR (our_stderr)
#    endif

#    ifdef WINDOWS
DR_API file_t
dr_get_stdout_file(void)
{
    return GetStdHandle(STD_OUTPUT_HANDLE);
}

DR_API file_t
dr_get_stderr_file(void)
{
    return GetStdHandle(STD_ERROR_HANDLE);
}

DR_API file_t
dr_get_stdin_file(void)
{
    return GetStdHandle(STD_INPUT_HANDLE);
}

#        define STDERR (dr_get_stderr_file())
#    endif

static uint vendor = VENDOR_INTEL; /* default */

uint
proc_get_vendor(void)
{
    return vendor;
}

DR_API
int
proc_set_vendor(uint new_vendor)
{
    if (new_vendor == VENDOR_INTEL || new_vendor == VENDOR_AMD) {
        uint old_vendor = vendor;
        vendor = new_vendor;
        return old_vendor;
    } else
        return -1;
}

void *
heap_alloc(dcontext_t *dcontext, size_t size HEAPACCT(which_heap_t which))
{
    return malloc(size);
}

void *
heap_reachable_alloc(dcontext_t *dcontext, size_t size HEAPACCT(which_heap_t which))
{
    return malloc(size);
}

void
heap_free(dcontext_t *dcontext, void *p, size_t size HEAPACCT(which_heap_t which))
{
    free(p);
}

void
heap_reachable_free(dcontext_t *dcontext, void *p,
                    size_t size HEAPACCT(which_heap_t which))
{
    free(p);
}

dcontext_t *
get_thread_private_dcontext(void)
{
    return GLOBAL_DCONTEXT;
}

void
external_error(const char *file, int line, const char *msg)
{
    print_file(STDERR, "Usage error: %s (%s, line %d)\n", msg, file, line);
    abort();
}

size_t
proc_save_fpstate(byte *buf)
{
    return 0;
}

void
proc_restore_fpstate(byte *buf)
{
}

/* XXX: duplicated from arch.c */
priv_mcontext_t *
dr_mcontext_as_priv_mcontext(dr_mcontext_t *mc)
{
    return (priv_mcontext_t *)(&mc->MCXT_FLD_FIRST_REG);
}

/* XXX: the code below is duplicated w/ only minor changes from utils.c.
 * But it's messier to go put ifdefs througout utils.c, and there are enough
 * changes to these that utils_shared.c might also be messy.
 */

void
double_print(double val, uint precision, uint *top, uint *bottom, const char **sign)
{
    uint i, precision_multiple;
    if (val < 0.0) {
        val = -val;
        *sign = "-";
    } else {
        *sign = "";
    }
    for (i = 0, precision_multiple = 1; i < precision; i++)
        precision_multiple *= 10;
    *top = (uint)val;
    *bottom = (uint)((val - *top) * precision_multiple);
}

/* For repeated appending to a buffer.  The "sofar" var should be set
 * to 0 by the caller before the first call to print_to_buffer.
 * Returns false if there was not room for the string plus a null,
 * but still prints the maximum that will fit plus a null.
 */
bool
print_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, const char *fmt, ...)
{
    /* in io.c */
    extern int d_r_vsnprintf(char *s, size_t max, const char *fmt, va_list ap);
    ssize_t len;
    va_list ap;
    bool ok;
    va_start(ap, fmt);
    len = d_r_vsnprintf(buf + *sofar, bufsz - *sofar, fmt, ap);
    va_end(ap);
    ok = (len > 0 && len < (ssize_t)(bufsz - *sofar));
    /* XXX: Unfortunately we're duplicating core/utils.c's vprint_to_buffer.
     * Could we move that into io.c to share it with decodelib?
     * For now we duplicate the complex check here: see the comment in
     * vprint_to_buffer on the explanation.
     */
    *sofar += (len == -1 || len == (ssize_t)(bufsz - *sofar) ? (bufsz - *sofar - 1)
                                                             : (len < 0 ? 0 : len));
    /* be paranoid: though usually many calls in a row and could delay until end */
    buf[bufsz - 1] = '\0';
    return ok;
}

void
print_file(file_t f, const char *fmt, ...)
{
    va_list ap;
    /* XXX: vfprintf operates on FILE*, but our file_t is a fd, so we go
     * to a buffer.  Only used internally for disassembly so should not
     * get even close to 4096.
     */
#    define MAX_PRINT_FILE_LEN 4096
    char buf[MAX_PRINT_FILE_LEN];
    ssize_t len;

    va_start(ap, fmt);
    len = vsnprintf(buf, BUFFER_SIZE_ELEMENTS(buf), fmt, ap);
    va_end(ap);

#    ifdef UNIX
    /* Linux vsnprintf returns what would have been written, unlike Windows
     * or d_r_vsnprintf
     */
    if (len >= BUFFER_SIZE_ELEMENTS(buf))
        len = BUFFER_SIZE_ELEMENTS(buf); /* don't need NULL term */
#    else
    if (len < 0)
        len = BUFFER_SIZE_ELEMENTS(buf); /* don't need NULL term */
#    endif
    os_write(f, buf, len);
}

ssize_t
os_write(file_t f, const void *buf, size_t count)
{
#    ifdef UNIX
    return write(f, buf, count);
#    else
    /* file_t is HANDLE opened with CreateFile */
    DWORD written = 0;
    ssize_t out = -1;
    BOOL ok;
    if (f == INVALID_FILE)
        return out;
    /* our internal calls won't overflow uint */
    ok = WriteFile(f, buf, (uint)count, &written, NULL);
    if (ok)
        out = (ssize_t)written;
    return out;
#    endif
}

#endif /* STANDALONE_DECODER */
/***************************************************************************/
