/* **********************************************************
 * Copyright (c) 2015-2017 Google, Inc.  All rights reserved.
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

/* Test the drwrap extension with non-default calling conventions where available. */

#include "dr_api.h"
#include "drwrap.h"
#include "drmgr.h"

#define CHECK(x, ...)                        \
    do {                                     \
        if (!(x)) {                          \
            dr_fprintf(STDERR, __VA_ARGS__); \
            dr_abort();                      \
        }                                    \
    } while (0)

#if defined(WINDOWS) && !defined(X64)
#    define PLATFORM_HAS_THISCALL 1
#endif

#if !defined(ARM) && !defined(X64)
#    define PLATFORM_HAS_FASTCALL 1
#endif

#ifdef WINDOWS
#    ifdef X64
#        define SET_LENGTH_SYMBOL "?setLength@Rectangular@@QEAAXH@Z"
#        define COMPUTE_WEIGHT_SYMBOL "?computeWeight@Rectangular@@QEAAXHHH@Z"
#        define COMPUTE_DISPLACEMENT_SYMBOL \
            "?computeDisplacement@Rectangular@@QEAAXHHHHHHHHH@Z"
#    else
#        define SET_LENGTH_SYMBOL "?setLength@Rectangular@@QAEXH@Z"
#        define COMPUTE_WEIGHT_SYMBOL "?computeWeight@Rectangular@@QAIXHHH@Z"
#        define COMPUTE_DISPLACEMENT_SYMBOL \
            "?computeDisplacement@Rectangular@@QAEXHHHHHHHHH@Z"
#    endif
#else /* UNIX */
#    define SET_LENGTH_SYMBOL "_ZN11Rectangular9setLengthEi"
#    define COMPUTE_WEIGHT_SYMBOL "_ZN11Rectangular13computeWeightEiii"
#    define COMPUTE_DISPLACEMENT_SYMBOL "_ZN11Rectangular19computeDisplacementEiiiiiiiii"
#endif

static client_id_t client_id;
static app_pc set_field_pc;
static app_pc compute_weight_pc;
static app_pc compute_displacement_pc;
static bool first_displacement_call = true;

/* disable the MSVC warning about a constant loop predicate (the while(0) in CHECK) */
#ifdef _MSC_VER
#    pragma warning(disable : 4127)
#endif

#ifdef PLATFORM_HAS_THISCALL
static void
check_thiscall(void *wrapcxt)
{
    /* N.B.: get the arg before the register, b/c drwrap_get_mcontext() permanently
     * alters the state of the drwrap internal mcontext (i.e., no cheating).
     */
    app_pc first_arg = (app_pc)drwrap_get_arg(wrapcxt, 0);
    app_pc this_pointer = (app_pc)drwrap_get_mcontext(wrapcxt)->xcx;

    CHECK(first_arg == this_pointer,
          "wrap target is not a proper 'thiscall' (register xcx contains " PIFX
          ", but arg 0 is " PIFX ")",
          (ptr_uint_t)this_pointer, (ptr_uint_t)first_arg);
}
#endif

#ifdef PLATFORM_HAS_FASTCALL
static void
check_fastcall(void *wrapcxt)
{
    app_pc first_arg = (app_pc)drwrap_get_arg(wrapcxt, 0);
    app_pc second_arg = (app_pc)drwrap_get_arg(wrapcxt, 1);
    dr_mcontext_t *mc = drwrap_get_mcontext(wrapcxt);

    CHECK(first_arg == (app_pc)mc->xcx, "first arg of fastcall not in xcx");
    CHECK(second_arg == (app_pc)mc->xdx, "second arg of fastcall not in xdx");
}
#endif

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    CHECK(wrapcxt != NULL && user_data != NULL, "invalid arg");
    CHECK(drwrap_get_arg(wrapcxt, 0) != NULL, "\"this\" pointer is NULL");
    if (drwrap_get_func(wrapcxt) == set_field_pc) {
        ptr_uint_t length_arg = (ptr_uint_t)drwrap_get_arg(wrapcxt, 1);

        CHECK(length_arg == 7, "length arg is %d but should be %d", length_arg, 7);
#ifdef PLATFORM_HAS_THISCALL
        check_thiscall(wrapcxt);
#endif
    } else if (drwrap_get_func(wrapcxt) == compute_weight_pc) {
        app_pc this_pointer = (app_pc)drwrap_get_arg(wrapcxt, 0);
        ptr_uint_t width_arg = (ptr_uint_t)drwrap_get_arg(wrapcxt, 1);
        ptr_uint_t height_arg = (ptr_uint_t)drwrap_get_arg(wrapcxt, 2);
        ptr_uint_t density_arg = (ptr_uint_t)drwrap_get_arg(wrapcxt, 3);

        CHECK(this_pointer != NULL, "'this' pointer is NULL");
        CHECK(width_arg == 3, "width arg is %d but should be %d", width_arg, 3);
        CHECK(height_arg == 2, "height arg is %d but should be %d", height_arg, 2);
        CHECK(density_arg == 10, "density arg is %d but should be %d", density_arg, 10);

#ifdef PLATFORM_HAS_FASTCALL
        check_fastcall(wrapcxt);
#endif
    } else if (drwrap_get_func(wrapcxt) == compute_displacement_pc) {
        uint i, value;

        CHECK((app_pc)drwrap_get_arg(wrapcxt, 0) != NULL, "'this' pointer is NULL");
        if (first_displacement_call) {
            for (i = 1; i < 10; i++) {
                value = (uint)(ptr_uint_t)drwrap_get_arg(wrapcxt, i);
                CHECK(value == (ptr_uint_t)i,
                      "value of arg %d is wrong: expected %d but found %d", i, i, value);
            }
            first_displacement_call = false;
        } else {
            for (i = 1; i < 10; i++)
                drwrap_set_arg(wrapcxt, i, (void *)(ptr_uint_t)(10 - i));
        }
    } else {
        CHECK(false, "wrong wrap func");
    }
}

static app_pc
wrap_function(module_data_t *module, const char *symbol, drwrap_callconv_t callconv)
{
    app_pc pc = (app_pc)dr_get_proc_address(module->handle, symbol);
    bool ok = drwrap_wrap_ex(pc, wrap_pre, NULL, NULL, callconv);

    CHECK(ok, "wrap failed");
    CHECK(drwrap_is_wrapped(pc, wrap_pre, NULL), "drwrap_is_wrapped query failed");
    return pc;
}

static void
event_exit(void)
{
    drwrap_exit();
    drmgr_exit();
    dr_fprintf(STDERR, "all done\n");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drwrap_callconv_t thiscall, fastcall;
    module_data_t *module = dr_get_main_module();

    client_id = id; /* avoid compiler warning */

    drmgr_init();
    drwrap_init();
    dr_register_exit_event(event_exit);

#ifdef PLATFORM_HAS_THISCALL
    thiscall = DRWRAP_CALLCONV_THISCALL;
#else
    thiscall = DRWRAP_CALLCONV_DEFAULT;
#endif
    set_field_pc = wrap_function(module, SET_LENGTH_SYMBOL, thiscall);

#ifdef PLATFORM_HAS_FASTCALL
    fastcall = DRWRAP_CALLCONV_FASTCALL;
#else
    fastcall = DRWRAP_CALLCONV_DEFAULT;
#endif
    compute_weight_pc = wrap_function(module, COMPUTE_WEIGHT_SYMBOL, fastcall);

    compute_displacement_pc =
        wrap_function(module, COMPUTE_DISPLACEMENT_SYMBOL, thiscall);

    dr_free_module_data(module);
}

/* re-enable the warning about constant loop predicates */
#ifdef _MSC_VER
#    pragma warning(4 : 4127)
#endif
