/* **********************************************************
 * Copyright (c) 2013-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2007 VMware, Inc.  All rights reserved.
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

#include <windows.h>
#include <stdio.h>
#include <io.h> /* _access */

/* This is a tool whose sole purpose is to provide an in-memory image
 * of an initialized dynamorio.dll, so we can query it with a debugger.
 *
 * If you launch this executable through a debugger, use -debugbreak;
 * otherwise, use -loop and then attach.
 *
 * example usage:
 *   ./cdb -g -c "ln 7004e660; ln 77f830e7; q" c:\\derek\\dr\\tools\\DRload.exe
 * -debugbreak c:\\derek\\dr\\builds\\11087\\exports\\x86_win32_rel\\dynamorio.dll | grep
 * '|'
 * =>
 *   (7004deb8)   dynamorio!shared_code+0x7a8   |  (7004edc4)   dynamorio!features
 *   (77f830c2)   ntdll!RtlIsDosDeviceName_U+0x25   |  (77faebf2)
 * ntdll!RtlpAllocateHeapUsageEntry
 */

typedef int (*int_func_t)();
typedef void (*void_func_t)();

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf) (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0

int
usage(char *exec)
{
    fprintf(stderr,
            "Usage: %s [-help] [-debugbreak] [-loop] [-key] [-no_init]\n"
            "        [-call_to_offset <hex offset>] [-find_safe_offset] [-no_resolve]\n"
            "        [-map <filename>] [-base <hex addr>] [-imagelist <file> |"
            "<DR/other dll path>]\n",
            exec);
    return 1;
}

int
help(char *exec)
{
    usage(exec);
    fprintf(stderr, "   -help : print this message\n");
    fprintf(stderr,
            "   -debugbreak : for launching under a debugger, trigger a "
            "debugbreak once dll is loaded\n");
    fprintf(stderr,
            "   -loop : for attaching a debugger, loop infinitely once dll is"
            " loaded\n");
    fprintf(stderr,
            "   -key : for attaching a debugger, wait for keypress once dll is"
            " loaded\n");
    fprintf(stderr,
            "   -no_init : don't call dynamorio init function after dll is"
            " loaded (use for non-dr dll)\n");
    fprintf(stderr,
            "   -call_to_offset <hex offset> : once dll is loaded call this "
            "offset to the dll base");
    fprintf(stderr,
            "   -find_safe_offset : if -call_to_offset is set, finds the first"
            "return instr in\n      the same mem region as the supplied offset and calls"
            "it instead.");
    fprintf(stderr,
            "   -no_resolve : pass DONT_RESOLVE_DLL_REFERENCES to the ldr when"
            " loading the dll\n      (prevents dependent dlls from being loaded)\n");
    fprintf(stderr, "   -map <filename> <hex address> : map filename at address\n");
    fprintf(stderr, "   -base <address> : maps dynamorio.dll at address\n");
    fprintf(stderr, "   -preferred <hex address> : makes -base usable for other dlls\n");
    fprintf(stderr, "   <DR/other dll path> : path to dll to load\n");
    return 0;
}

int
map_file(const char *filename, void *addr, int image)
{
    HANDLE map;
    PBYTE view;
    /* Must specify FILE_SHARE_READ to open if -persist_lock_file is in use */
    HANDLE file =
        CreateFileA(filename, GENERIC_READ | (image ? FILE_EXECUTE : 0), FILE_SHARE_READ,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == NULL) {
        int err = GetLastError();
        fprintf(stderr, "Error %d opening \"%s\"\n", err, filename);
        return 0;
    }
    /* For image, it fails w/ ACCESS_DENIED at the map stage if we ask for
     * more than PAGE_READONLY, and at the view stage if we ask for
     * anything more than FILE_MAP_READ.
     */
    map = CreateFileMapping(file, NULL, PAGE_READONLY | (image ? SEC_IMAGE : 0), 0, 0,
                            NULL);
    if (map == NULL) {
        int err = GetLastError();
        CloseHandle(file);
        fprintf(stderr, "Error %d mapping \"%s\"\n", err, filename);
        return 0;
    }
    view = MapViewOfFileEx(map, FILE_MAP_READ, 0, 0, 0, addr);
    if (view == NULL) {
        int err = GetLastError();
        CloseHandle(map);
        CloseHandle(file);
        fprintf(stderr, "Error %d mapping view of \"%s\"\n", err, filename);
        return 0;
    }
    return 1;
}

int
main(int argc, char *argv[])
{
    char *DRpath;
    HANDLE dll;
    int_func_t init_func;
    void_func_t take_over_func;
    int res = 0;
    BOOL debugbreak = FALSE;
    BOOL infinite = FALSE;
    BOOL keypress = FALSE;
    BOOL initialize_dr = TRUE;
    BOOL use_dont_resolve = FALSE;
    int arg_offs = 1;
    void *force_base = NULL;
    void *preferred_base = NULL;
    int call_offset = -1;
    BOOL find_safe_offset = FALSE;
    char *imagelist = NULL;

    /* Link user32.dll for easier running under dr */
    do {
        if (argc > 1000)
            MessageBeep(0);
    } while (0);

    if (argc < 2)
        return usage(argv[0]);
    while (arg_offs < argc && argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-help") == 0) {
            return help(argv[0]);
        } else if (strcmp(argv[arg_offs], "-debugbreak") == 0) {
            debugbreak = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-loop") == 0) {
            infinite = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-key") == 0) {
            keypress = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-no_init") == 0) {
            initialize_dr = FALSE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-call_to_offset") == 0) {
            int len;
            arg_offs += 1;
            if (argc - (arg_offs) < 1)
                return usage(argv[0]);
            len = sscanf(argv[arg_offs], "%08x", &call_offset);
            if (len != 1 || call_offset == -1)
                return usage(argv[0]);
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-find_safe_offset") == 0) {
            find_safe_offset = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-no_resolve") == 0) {
            use_dont_resolve = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-map") == 0) {
            void *addr = NULL;
            int len;
            arg_offs += 1;
            if (argc - (arg_offs + 1) < 1)
                return usage(argv[0]);
            len = sscanf(argv[arg_offs + 1], "%p", &addr);
            if (len != 1 || addr == NULL)
                return usage(argv[0]);
            map_file(argv[arg_offs], addr, 0 /* mapped */);
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-base") == 0) {
            int len;
            arg_offs += 1;
            if (argc - (arg_offs) < 1)
                return usage(argv[0]);
            len = sscanf(argv[arg_offs], "%p", &force_base);
            if (len != 1 || force_base == NULL)
                return usage(argv[0]);
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-preferred") == 0) {
            int len;
            arg_offs += 1;
            if (argc - (arg_offs) < 1)
                return usage(argv[0]);
            len = sscanf(argv[arg_offs], "%p", &preferred_base);
            if (len != 1 || preferred_base == NULL)
                return usage(argv[0]);
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-imagelist") == 0) {
            arg_offs += 1;
            if (argc - (arg_offs) < 1)
                return usage(argv[0]);
            imagelist = argv[arg_offs];
            arg_offs += 1;
        } else
            return usage(argv[0]);
        if (argc - arg_offs < (imagelist == NULL ? 1 : 0))
            return usage(argv[0]);
    }

    if (imagelist != NULL) {
        FILE *f;
        int count = 0;
        char line[MAX_PATH];
        if (_access(imagelist, 4 /*read*/) == -1) {
            fprintf(stderr, "Cannot read %s\n", imagelist);
            return 1;
        }
        f = fopen(imagelist, "r");
        if (f == NULL) {
            fprintf(stderr, "Failed to open %s\n", imagelist);
            return 1;
        }
        while (fgets(line, BUFFER_SIZE_ELEMENTS(line), f) != NULL) {
            size_t len = strlen(line) - 1;
            while (len > 0 && (line[len] == '\n' || line[len] == '\r')) {
                line[len] = '\0';
                len--;
            }
            fprintf(stderr, "loading %s\n", line);
            if (map_file(line, NULL, 1 /*image*/))
                count++;
            else
                fprintf(stderr, "  => FAILED\n");
        }
        fprintf(stderr, "loaded %d images successfully\n", count);
        fflush(stderr);
    } else {
        DRpath = argv[arg_offs];

        if (force_base != NULL) {
            /* add load blocks at the expected base addresses */
            void *base;
            if (preferred_base != NULL)
                base = preferred_base;
            else /* assume DR dll */
                base = (void *)0x71000000;
            VirtualAllocEx(GetCurrentProcess(), base, 0x1000, MEM_RESERVE, PAGE_NOACCESS);
            if (preferred_base == NULL) {
                /* also do debug build base */
                base = (void *)0x15000000;
                VirtualAllocEx(GetCurrentProcess(), base, 0x1000, MEM_RESERVE,
                               PAGE_NOACCESS);
            }
            base = force_base;
            /* to ensure we fill all cavities we loop through */
            while (base > (void *)0x10000) {
                base = (void *)((intptr_t)base - 0x10000);
                VirtualAllocEx(GetCurrentProcess(), base, 0x1000, MEM_RESERVE,
                               PAGE_NOACCESS);
            }

#if 0
            map_file(DRpath, force_base, 1 /* image */);
            /* FIXME: note that the DLL will not be relocated! */
            /* we can't really initialize */
#endif
        }

        if (use_dont_resolve) {
            dll = LoadLibraryExA(DRpath, NULL, DONT_RESOLVE_DLL_REFERENCES);
        } else {
            dll = LoadLibraryA(DRpath);
        }

        if (dll == NULL) {
            int err = GetLastError();
            fprintf(stderr, "Error %d loading %s\n", err, DRpath);
            return 1;
        }

        if (initialize_dr) {
            init_func = (int_func_t)GetProcAddress(dll, "dynamorio_app_init");
            take_over_func = (void_func_t)GetProcAddress(dll, "dynamorio_app_take_over");
            if (init_func == NULL || take_over_func == NULL) {
                fprintf(stderr, "Error finding DR init routines\n");
                res = 1;
                goto done;
            }
            res = (*init_func)();
            /* FIXME: ASSERT(res) */
            (*take_over_func)();
            res = 0;
        }

        if (call_offset != -1) {
            unsigned char *call_location = (char *)dll + call_offset;
            if (find_safe_offset) {
                MEMORY_BASIC_INFORMATION mbi;
                if (VirtualQuery(call_location, &mbi, sizeof(mbi)) != sizeof(mbi) ||
                    mbi.State == MEM_FREE || mbi.State == MEM_RESERVE) {
                    fprintf(stderr, "Call offset invalid, leaving as is\n");
                } else {
                    /* find safe place to call, we just look for 0xc3 though could in
                     * theory use other types of rets too */
                    unsigned char *test;
                    for (test = call_location;
                         test < (char *)mbi.BaseAddress + mbi.RegionSize; test++) {
                        if (*test == 0xc3 /* plain ret */) {
                            fprintf(stderr, "Found safe call target at offset 0x%tx\n",
                                    test - (char *)dll);
                            call_location = test;
                            break;
                        }
                    }
                    if (call_location != test) {
                        fprintf(stderr, "Unable to find safe call target\n");
                    }
                }
            }
            fprintf(stderr, "Calling base(%p) + offset(0x%tx) = %p\n", dll,
                    call_location - (char *)dll, call_location);
            (*(int (*)())(call_location))();
        }
    }

done:
    if (keypress) {
        fprintf(stderr, "press any key or attach a debugger...\n");
        fflush(stderr);
        getchar();
    }
    if (debugbreak) {
        __debugbreak();
    }
    if (infinite) {
        while (1)
            Sleep(1);
    }
    return res;
}
