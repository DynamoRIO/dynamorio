/* **********************************************************
 * Copyright (c) 2011-2024 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */

/* Some of this is based on the winsysnums.c client in DR */

#ifdef WINDOWS
# define UNICODE
# define _UNICODE
#else
# error Only Windows is supported
#endif

#include "dr_api.h" /* for the types */
#include "dr_frontend.h"
#include "droption.h"
#include "drsyms.h"
#include "../common/utils.h" /* only for BUFFER*, DIRSEP */
#include "../drsyscall/drsyscall.h"

#include <assert.h>
#include <algorithm>
#include <unordered_map>
#include <vector>

/* We'll get an error in standalone usage by DrM's frontend if we call
 * dr_get_current_drcontext() in the utils.h NOTIFY so we make our own.
 * The frontend's -v, -vv, and -vvv set op_verbose_level.
 */
#undef NOTIFY
#define NOTIFY(level, ...) do {          \
    if (op_verbose_level >= (level)) {   \
        dr_fprintf(STDERR, __VA_ARGS__); \
    }                                    \
} while (0)

/***************************************************************************
 * Usercalls
 */

static std::string
sysname_from_wrapper(std::string wrapper)
{
  static std::unordered_map<std::string, std::string> wrapper2sysname({
    {"AllowForegroundActivation"     , "NtUserCallNoParam.ALLOWFOREGNDACTIVATION"      },
    {"CreateMenu"                    , "NtUserCallNoParam.CREATEMENU"                  },
    {"CreatePopupMenu"               , "NtUserCallNoParam.CREATEMENUPOPUP"             },
    {"CreateSystemThreads"           , "NtUserCallNoParam.CREATESYSTEMTHREADS"         },
    {"DeferredDesktopRotation"       , "NtUserCallNoParam.DEFERREDDESKTOPROTATION"     },
    {"DestroyCaret"                  , "NtUserCallNoParam.DESTROY_CARET"               },
    {"DisableProcessWindowsGhosting" , "NtUserCallNoParam.DISABLEPROCWNDGHSTING"       },
    {"EnableMiPShellThread"          , "NtUserCallNoParam.ENABLEMIPSHELLTHREAD"        },
    {"EnablePerMonitorMenuScaling"   , "NtUserCallNoParam.ENABLEPERMONITORMENUSCALING" },
    {"GetIMEShowStatus"              , "NtUserCallNoParam.GETIMESHOWSTATUS"            },
    {"GetInputDesktop"               , "NtUserCallNoParam.GETINPUTDESKTOP"             },
    {"GetMessagePos"                 , "NtUserCallNoParam.GETMESSAGEPOS"               },
    {"GetUnpredictedMessagePos"      , "NtUserCallNoParam.GETUNPREDICTEDMESSAGEPOS"    },
    {"RegisterMessagePumpHook"       , "NtUserCallNoParam.INIT_MESSAGE_PUMP"           },
    {"IsMiPShellThreadEnabled"       , "NtUserCallNoParam.ISMIPSHELLTHREADENABLED"     },
    {"IsQueueAttached"               , "NtUserCallNoParam.ISQUEUEATTACHED"             },
    {"LoadCursorsAndIcons"           , "NtUserCallNoParam.LOADCURSANDICOS"             },
    {"ReleaseCapture"                , "NtUserCallNoParam.RELEASECAPTURE"              },
    {"UnregisterMessagePumpHook"     , "NtUserCallNoParam.UNINIT_MESSAGE_PUMP"         },
    {"UpdatePerUserImmEnabling"      , "NtUserCallNoParam.UPDATEPERUSERIMMENABLING"    },

    {"AllowSetForegroundWindow"      , "NtUserCallOneParam.ALLOWSETFOREGND"            },
    {"BeginDeferWindowPos"           , "NtUserCallOneParam.BEGINDEFERWNDPOS"           },
    {"CreateAniIcon"                 , "NtUserCallOneParam.CREATEEMPTYCUROBJECT"       },
    {"DdeUninitialize"               , "NtUserCallOneParam.CSDDEUNINITIALIZE"          },
    {"DirectedYield"                 , "NtUserCallOneParam.DIRECTEDYIELD"              },
    {"DwmLockScreenUpdates"          , "NtUserCallOneParam.DWMLOCKSCREENUPDATES"       },
    {"EnableSessionForMMCSS"         , "NtUserCallOneParam.ENABLESESSIONFORMMCSS"      },
    {"EnumClipboardFormats"          , "NtUserCallOneParam.ENUMCLIPBOARDFORMATS"       },
    {"ForceEnableNumpadTranslation"  , "NtUserCallOneParam.FORCEENABLENUMPADTRANSLATION"},
    {"ForceFocusBasedMouseWheelRouting", "NtUserCallOneParam.FORCEFOCUSBASEDMOUSEWHEELROUTING"},
    {"MsgWaitForMultipleObjectsEx"   , "NtUserCallOneParam.GETINPUTEVENT"              },
    {"GetKeyboardLayout"             , "NtUserCallOneParam.GETKEYBOARDLAYOUT"          },
    {"GetKeyboardType"               , "NtUserCallOneParam.GETKEYBOARDTYPE"            },
    {"GetProcessDefaultLayout"       , "NtUserCallOneParam.GETPROCDEFLAYOUT"           },
    {"GetQueueStatus"                , "NtUserCallOneParam.GETQUEUESTATUS"             },
    {"GetSendMessageReceiver"        , "NtUserCallOneParam.GETSENDMSGRECVR"            },
    {"GetWinStationInfo"             , "NtUserCallOneParam.GETWINSTAINFO"              },
    {"IsThreadMessageQueueAttached"  , "NtUserCallOneParam.ISTHREADMESSAGEQUEUEATTACHED"},
    {"LoadLocalFonts"                , "NtUserCallOneParam.LOADFONTS"                  },
    {"LockSetForegroundWindow"       , "NtUserCallOneParam.LOCKFOREGNDWINDOW"          },
    {"MessageBeep"                   , "NtUserCallOneParam.MESSAGEBEEP"                },
    {"PostQuitMessage"               , "NtUserCallOneParam.POSTQUITMESSAGE"            },
    {"PostUIActions"                 , "NtUserCallOneParam.POSTUIACTIONS"              },
    {"UserRealizePalette"            , "NtUserCallOneParam.REALIZEPALETTE"             },
    {"RegisterSystemThread"          , "NtUserCallOneParam.REGISTERSYSTEMTHREAD"       },
    {"ReleaseDC"                     , "NtUserCallOneParam.RELEASEDC"                  },
    {"ReplyMessage"                  , "NtUserCallOneParam.REPLYMESSAGE"               },
    {"SetCaretBlinkTime"             , "NtUserCallOneParam.SETCARETBLINKTIME"          },
    {"SetDoubleClickTime"            , "NtUserCallOneParam.SETDBLCLICKTIME"            },
    {"SetInputServiceState"          , "NtUserCallOneParam.SETINPUTSERVICESTATE"       },
    {"SetMessageExtraInfo"           , "NtUserCallOneParam.SETMESSAGEEXTRAINFO"        },
    {"SetProcessDefaultLayout"       , "NtUserCallOneParam.SETPROCDEFLAYOUT"           },
    {"SetShellChangeNotifyWindow"    , "NtUserCallOneParam.SETSHELLCHANGENOTIFYWINDOW" },
    {"SetTSFEventState"              , "NtUserCallOneParam.SETTSFEVENTSTATE"           },
    {"LoadAndSendWatermarkStrings"   , "NtUserCallOneParam.SETWATERMARKSTRINGS"        },
    {"ShowCursor"                    , "NtUserCallOneParam.SHOWCURSOR"                 },
    {"ShowStartGlass"                , "NtUserCallOneParam.SHOWSTARTGLASS"             },
    {"SwapMouseButton"               , "NtUserCallOneParam.SWAPMOUSEBUTTON"            },
    {"WindowFromDC"                  , "NtUserCallOneParam.WINDOWFROMDc"               },
    {"WOWModuleUnload"               , "NtUserCallOneParam.WOWMODULEUNLOAD"            },

    {"DeregisterShellHookWindow"     , "NtUserCallHwnd.DEREGISTERSHELLHOOKWINDOW"      },
    {"GetModernAppWindow"            , "NtUserCallHwnd.GETMODERNAPPWINDOW"             },
    {"GetWindowContextHelpId"        , "NtUserCallHwnd.GETWNDCONTEXTHLPID"             },
    {"RegisterShellHookWindow"       , "NtUserCallHwnd.REGISTERSHELLHOOKWINDOW"        },

    {"SetProgmanWindow"              , "NtUserCallHwndOpt.SETPROGMANWINDOW"            },
    {"SetTaskmanWindow"              , "NtUserCallHwndOpt.SETTASKMANWINDOW"            },

    {"ClearWindowState"              , "NtUserCallHwndParam.CLEARWINDOWSTATE"          },
    {"EnableModernAppWindowKeyboardIntercept", "NtUserCallHwndParam.ENABLEMODERNAPPWINDOWKBDINTERCEPT"},
    {"RegisterKeyboardCorrectionCallout", "NtUserCallHwndParam.REGISTERKBDCORRECTION"  },
    {"RegisterWindowArrangementCallout", "NtUserCallHwndParam.REGISTERWINDOWARRANGEMENTCALLOUT"},
    {"SetWindowState"                , "NtUserCallHwndParam.SETWINDOWSTATE"            },
    {"SetWindowContextHelpId"        , "NtUserCallHwndParam.SETWNDCONTEXTHLPID"        },

    {"ArrangeIconicWindows"          , "NtUserCallHwndLock.ARRANGEICONICWINDOWS"       },
    {"DrawMenuBar"                   , "NtUserCallHwndLock.DRAWMENUBAR"                },
    {"xxxGetSysMenuHandle"           , "NtUserCallHwndLock.GETSYSMENUHANDLE"           },
    {"xxxGetSysMenuPtr"              , "NtUserCallHwndLock.GETSYSMENUHANDLEX"          },
    {"GetWindowTrackInfoAsync"       , "NtUserCallHwndLock.GETWINDOWTRACKINFOASYNC"    },
    {"RealMDIRedrawFrame"            , "NtUserCallHwndLock.REDRAWFRAME"                },
    {"SetActiveImmersiveWindow"      , "NtUserCallHwndLock.SETACTIVEIMMERSIVEWINDOW"   },
    {"SetForegroundWindow"           , "NtUserCallHwndLock.SETFOREGROUNDWINDOW"        },
    {"MDIAddSysMenu"                 , "NtUserCallHwndLock.SETSYSMENU"                 },
    {"UpdateWindow"                  , "NtUserCallHwndLock.UPDATEWINDOW"               },

    {"EnableWindow"                  , "NtUserCallHwndParamLock.ENABLEWINDOW"          },
    {"SetModernAppWindow"            , "NtUserCallHwndParamLock.SETMODERNAPPWINDOW"    },
    {"ShowOwnedPopups"               , "NtUserCallHwndParamLock.SHOWOWNEDPOPUPS"       },
    {"SwitchToThisWindow"            , "NtUserCallHwndParamLock.SWITCHTOTHISWINDOW"    },
    {"ValidateRgn"                   , "NtUserCallHwndParamLock.VALIDATERGN"           },
    {"NotifyOverlayWindow"           , "NtUserCallHwndParam.NOTIFYOVERLAYWINDOW"       },

    {"ChangeWindowMessageFilter"     , "NtUserCallTwoParam.CHANGEWNDMSGFILTER"         },
    {"EnableShellWindowManagementBehavior", "NtUserCallTwoParam.ENABLESHELLWINDOWMGT"  },
    {"GetCursorPos"                  , "NtUserCallTwoParam.GETCURSORPOS"               },
    {"InitOemXlateTables"            , "NtUserCallTwoParam.INITANSIOEM"                },
    {"RegisterGhostWindow"           , "NtUserCallTwoParam.REGISTERGHSTWND"            },
    {"RegisterLogonProcess"          , "NtUserCallTwoParam.REGISTERLOGONPROCESS"       },
    {"RegisterFrostWindow"           , "NtUserCallTwoParam.REGISTERSBLFROSTWND"        },
    {"RegisterUserHungAppHandlers"   , "NtUserCallTwoParam.REGISTERUSERHUNGAPPHANDLERS"},
    {"SetCaretPos"                   , "NtUserCallTwoParam.SETCARETPOS"                },
    {"SetCITInfo"                    , "NtUserCallTwoParam.SETCITINFO"                 },
    {"SetCursorPos"                  , "NtUserCallTwoParam.SETCURSORPOS"               },
    {"SetThreadQueueMergeSetting"    , "NtUserCallTwoParam.SETTHREADQUEUEMERGESETTING" },
    {"UnhookWindowsHook"             , "NtUserCallTwoParam.UNHOOKWINDOWSHOOK"          },
    {"WOWCleanup"                    , "NtUserCallTwoParam.WOWCLEANUP"                 },
      });
  return wrapper2sysname[wrapper];
}

static const char *const usercall_names[] = {
    "NtUserCallNoParam",       "NtUserCallOneParam",  "NtUserCallHwnd",
    "NtUserCallHwndOpt",       "NtUserCallHwndParam", "NtUserCallHwndLock",
    "NtUserCallHwndParamLock", "NtUserCallTwoParam",
};
#define NUM_USERCALL (sizeof(usercall_names) / sizeof(usercall_names[0]))

/* For searching for usercalls we'll go quite a ways */
#define MAX_BYTES_BEFORE_USERCALL 0x300

static void
look_for_usercall_targets(const char *dll_path, byte *map_base, size_t map_size,
                          byte *usercall_addr[NUM_USERCALL],
                          size_t *usercall_targets_found)
{
    int i;
    size_t num_found = 0;
    std::string sym_prefix(dll_path);
    /* Get the basename. */
    sym_prefix = sym_prefix.substr(sym_prefix.find_last_of("\\/") + 1);
    /* Remove the extension. */
    sym_prefix = sym_prefix.substr(0, sym_prefix.find_last_of('.'));
    /* Now add ! so we can have a module!sym fully-qualified name, to avoid
     * querying a previously-queried module.
     */
    sym_prefix += "!";
    for (i = 0; i < NUM_USERCALL; i++) {
        size_t offs;
        /* To handle win10-1607 we have to look for imports from win32u.dll.
         * But, for 32-bit, NoParam instead calls to a local routine that invokes yet
         * another routine that finally does the import.
         */
        static const std::string alt_noparam("Local_NtUserCallNoParam");
        static const std::string alt_twoparam("Local_NtUserCallTwoParam");
        static const std::string imp_prefixA = "_imp__";
        /* x64 uses a single 2nd underscore for some routines. */
        static const std::string imp_prefixB = "_imp_";
        /* We have to look for the __imp first, b/c win10-1607 does have
         * a NoParam wrapper.
         */
        std::string imp_name(sym_prefix + imp_prefixA + usercall_names[i]);
#ifndef X64
        if (i == 0)
            imp_name = sym_prefix + alt_noparam;
        else if (i == NUM_USERCALL - 1)
            imp_name = sym_prefix + alt_twoparam;
#endif
        NOTIFY(3, "Looking for %s" NL, imp_name.c_str());
        drsym_error_t symres = drsym_lookup_symbol(dll_path, imp_name.c_str(), &offs, 0);
        if (symres != DRSYM_SUCCESS) {
            NOTIFY(3, "Looking for %s" NL, imp_name.c_str());
            imp_name = sym_prefix + imp_prefixB + usercall_names[i];
            symres = drsym_lookup_symbol(dll_path, imp_name.c_str(), &offs, 0);
        }
        if (symres == DRSYM_SUCCESS) {
            usercall_addr[i] = map_base + offs;
            ++num_found;
            NOTIFY(2, "%s = %d +0x%x == " PFX "" NL, imp_name.c_str(), symres, offs,
                   usercall_addr[i]);
        } else {
            NOTIFY(3, "Looking for %s" NL, imp_name.c_str());
            imp_name = sym_prefix + usercall_names[i];
            symres = drsym_lookup_symbol(dll_path, imp_name.c_str(), &offs, 0);
            if (symres == DRSYM_SUCCESS) {
                usercall_addr[i] = map_base + offs;
                ++num_found;
                NOTIFY(2, "%s = %d +0x%x == " PFX "" NL, usercall_names[i], symres,
                       offs, usercall_addr[i]);
            } else {
                NOTIFY(2, "Error locating usercall %s" NL, usercall_names[i]);
            }
        }
    }
    *usercall_targets_found = num_found;
}

static std::pair<std::string, int>
look_for_usercall(void *dcontext, byte *entry, const char *sym, byte *mod_end,
                  byte *usercall_addr[NUM_USERCALL])
{
    /* For 32-bit we expect:
     *  USER32!AllowSetForegroundWindow:
     *  76120500 8bff            mov     edi,edi
     *  76120502 55              push    ebp
     *  76120503 8bec            mov     ebp,esp
     *  76120505 6a2e            push    2Eh
     *  76120507 ff7508          push    dword ptr [ebp+8]
     *  7612050a ff15706a1376    call    dword ptr [USER32!_imp__NtUserCallOneParam (76136a70)]
     *  76120510 5d              pop     ebp
     *  76120511 c20400          ret     4
     *
     * For 64-bit:
     *  USER32!AllowSetForegroundWindow:
     *  00007ffb`15e3bdd0 8bc9            mov     ecx,ecx
     *  00007ffb`15e3bdd2 ba2e000000      mov     edx,2Eh
     *  00007ffb`15e3bdd7 48ff2572e20500  jmp     qword ptr [USER32!_imp_NtUserCallOneParam (00007ffb`15e9a050)]
     */
    bool found_set_imm = false;
    int imm = 0;
    app_pc pc, pre_pc;
    instr_t *instr;
    std::string sysname;
    int sysnum = -1;
    if (entry == NULL)
        return std::make_pair("", -1);
    instr = instr_create(dcontext);
    pc = entry;
    while (true) {
        instr_reset(dcontext, instr);
        pre_pc = pc;
        pc = decode(dcontext, pc, instr);
        if (op_verbose_level >= 3) {
            instr_set_translation(instr, pre_pc);
            dr_print_instr(dcontext, STDOUT, instr, "");
        }
        if (pc == NULL || !instr_valid(instr) || pc >= mod_end)
            break;
#ifdef X64
        if (!found_set_imm && instr_get_opcode(instr) == OP_mov_imm &&
            opnd_is_reg(instr_get_dst(instr, 0))) {
            /* The code is in the last param. */
            reg_id_t reg = opnd_get_reg(instr_get_dst(instr, 0));
            if (reg == DR_REG_ECX || /* NoParam */
                reg == DR_REG_EDX || /* OneParam */
                reg == DR_REG_R8D) { /* TwoParam */
                found_set_imm = true;
                imm = (int)opnd_get_immed_int(instr_get_src(instr, 0));
            }
        }
#else
        /* If there are multiple push-immeds we want the outer one
         * as the code is the last param.
         */
        if (!found_set_imm && instr_get_opcode(instr) == OP_push_imm) {
            found_set_imm = true;
            imm = (int)opnd_get_immed_int(instr_get_src(instr, 0));
        }
#endif
        else if (instr_is_call_direct(instr) && found_set_imm) {
            /* We don't rule out usercall imports due to Local_NtUserCallNoParam */
            app_pc tgt = opnd_get_pc(instr_get_target(instr));
            bool found = false;
            int i;
            for (i = 0; i < NUM_USERCALL; i++) {
                if (tgt == usercall_addr[i]) {
                    sysname = sysname_from_wrapper(sym);
                    if (!sysname.empty())
                        sysnum = imm;
                    NOTIFY(2, "Call #0x%02x to %s at %s+0x%x == %s,%d" NL, imm,
                           usercall_names[i], sym, pre_pc - entry, sysname.c_str(),
                           sysnum);
                    found = true;
                    break;
                }
            }
            if (found)
                break;
            found_set_imm = false;
        } else if ((instr_is_call_indirect(instr) ||
                    instr_get_opcode(instr) == OP_jmp_ind) &&
                   found_set_imm &&
                   (opnd_is_abs_addr(instr_get_target(instr))
                    IF_X64(|| opnd_is_rel_addr(instr_get_target(instr))))) {
            app_pc tgt = (app_pc) opnd_get_addr(instr_get_target(instr));
            bool found = false;
            int i;
            for (i = 0; i < NUM_USERCALL; i++) {
                if (tgt == usercall_addr[i]) {
                    sysname = sysname_from_wrapper(sym);
                    if (!sysname.empty())
                        sysnum = imm;
                    NOTIFY(2, "Call #0x%02x to %s at %s+0x%x == %s,%d" NL, imm,
                           usercall_names[i], sym, pre_pc - entry, sysname.c_str(),
                           sysnum);
                    found = true;
                    break;
                }
            }
            if (found)
                break;
            found_set_imm = false;
        } else if (instr_is_return(instr))
            break;
        else if (instr_is_call(instr))
            found_set_imm = false;
        if (pc - entry > MAX_BYTES_BEFORE_USERCALL)
            break;
    }
    instr_destroy(dcontext, instr);
    return std::make_pair(sysname, sysnum);
}

/***************************************************************************
 * Fetch symbols
 */

static const char * const syscall_dlls[] = {
    "ntdll.dll",
    "kernelbase.dll",
    "kernel32.dll",
    "gdi32.dll",
    "imm32.dll",
    "user32.dll",
    "win32u.dll",
};
#define NUM_SYSCALL_DLLS (sizeof(syscall_dlls)/sizeof(syscall_dlls[0]))

DR_EXPORT
drmf_status_t
drsys_find_sysnum_libs(OUT char **sysnum_lib_paths,
                       DR_PARAM_INOUT size_t *num_sysnum_libs)
{
    if (num_sysnum_libs == nullptr)
        return DRMF_ERROR_INVALID_PARAMETER;

    /* First, get %SystemRoot%. */
    TCHAR system_rootw[MAXIMUM_PATH];
    char system_root[MAXIMUM_PATH];
    DWORD len = GetWindowsDirectory(system_rootw, BUFFER_SIZE_ELEMENTS(system_rootw));
    if (len == 0) {
        _tcsncpy(system_rootw, _T("C:\\Windows"), BUFFER_SIZE_ELEMENTS(system_rootw));
        NULL_TERMINATE_BUFFER(system_rootw);
    }
    if (drfront_tchar_to_char(system_rootw, system_root,
                              BUFFER_SIZE_ELEMENTS(system_root)) != DRFRONT_SUCCESS) {
        NOTIFY(0, "Failed to determine system root" NL);
        return DRMF_ERROR_NOT_FOUND;
    }

    /* Next, get the count of dlls that exist on this machine
     * (win32u.dll and kernelbase.dll do not exist on older Windows).
     */
    int count = 0;
    int dll_readable[NUM_SYSCALL_DLLS];
    char buf[MAXIMUM_PATH];
    for (int i = 0; i < NUM_SYSCALL_DLLS; ++i) {
        bool readable;
        _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s%csystem32%c%s",
                  system_root, DIRSEP, DIRSEP, syscall_dlls[i]);
        NULL_TERMINATE_BUFFER(buf);
        if (drfront_access(buf, DRFRONT_READ, &readable) == DRFRONT_SUCCESS &&
            readable) {
            NOTIFY(1, "%s: %s is readable" NL, __FUNCTION__, buf);
            dll_readable[i] = true;
            ++count;
        } else {
            NOTIFY(1, "%s: %s is NOT readable" NL, __FUNCTION__, buf);
            dll_readable[i] = false;
        }
    }

    if (*num_sysnum_libs < count) {
        *num_sysnum_libs = count;
        return DRMF_ERROR_INVALID_SIZE;
    }
    if (sysnum_lib_paths == nullptr)
        return DRMF_ERROR_INVALID_PARAMETER;
    *num_sysnum_libs = count;
    int index = 0;
    for (int i = 0; i < NUM_SYSCALL_DLLS; ++i) {
        if (!dll_readable[i])
            continue;
        _snprintf(sysnum_lib_paths[index], MAXIMUM_PATH, "%s%csystem32%c%s",
                  system_root, DIRSEP, DIRSEP, syscall_dlls[i]);
        sysnum_lib_paths[index][MAXIMUM_PATH-1] = '\0';
        ++index;
    }
    return DRMF_SUCCESS;
}

static drmf_status_t
fetch_symbols(const char **sysnum_lib_paths, size_t num_sysnum_libs,
              const char *cache_dir)
{
    if (drfront_sym_init(NULL, "dbghelp.dll") != DRFRONT_SUCCESS) {
        NOTIFY(0, "Failed to initialize the symbol module" NL);
        return DRMF_ERROR;
    }
    /* Set _NT_SYMBOL_PATH for the app. */
    char symsrv_dir[MAXIMUM_PATH];
    if (drfront_set_client_symbol_search_path(cache_dir, false, symsrv_dir,
                                              BUFFER_SIZE_ELEMENTS(symsrv_dir)) !=
        DRFRONT_SUCCESS ||
        drfront_set_symbol_search_path(symsrv_dir) != DRFRONT_SUCCESS)
        NOTIFY(0, "WARNING: Can't set symbol search path. Symbol lookup may fail." NL);

    for (int i = 0; i < num_sysnum_libs; ++i) {
        char pdb_path[MAXIMUM_PATH];
        drfront_status_t sc = DRFRONT_SUCCESS;
        /* Sometimes there are transient errors on the symbol server side so we
         * re-try.
         */
#define NUM_TRIES 2
        for (int j = 0; j < NUM_TRIES; ++j) {
            NOTIFY(1, "Fetching symbols for \"%s\", attempt #%d" NL,
                   sysnum_lib_paths[i], j);
            sc = drfront_fetch_module_symbols(sysnum_lib_paths[i], pdb_path,
                                              BUFFER_SIZE_ELEMENTS(pdb_path));
            if (sc == DRFRONT_SUCCESS) {
                NOTIFY(1, "\tSuccessfully fetched or found symbols at \"%s\"" NL,
                       pdb_path);
                break;
            }
            /* Else try again */
            /* An invalid local path does not produce very useful error messages,
             * so we blindly try without it.  Things like forward slashes cause
             * problems.
             */
            if (drfront_get_env_var("_NT_SYMBOL_PATH", symsrv_dir,
                                    BUFFER_SIZE_ELEMENTS(symsrv_dir))
                == DRFRONT_SUCCESS) {
                NOTIFY(0, "Ignoring local _NT_SYMBOL_PATH in next attempt." NL);
                if (drfront_set_client_symbol_search_path
                    (cache_dir, true, symsrv_dir, BUFFER_SIZE_ELEMENTS(symsrv_dir)) !=
                    DRFRONT_SUCCESS ||
                    drfront_set_symbol_search_path(symsrv_dir) != DRFRONT_SUCCESS) {
                    NOTIFY(0, "WARNING: Can't set symbol search path. "
                           "Symbol lookup may fail." NL);
                }
            }
        }
        if (sc != DRFRONT_SUCCESS) {
            NOTIFY(0, "Failed to fetch symbols for %s: error %d" NL,
                   sysnum_lib_paths[i], sc);
            return DRMF_ERROR_NOT_FOUND;
        }
    }
    if (drfront_sym_exit() != DRFRONT_SUCCESS)
        NOTIFY(0, "drfront_sym_exit failed %d" NL, GetLastError());
    return DRMF_SUCCESS;
}

/***************************************************************************
 * Parse dlls
 */

/* We expect the win8 x86 sysenter adjacent "inlined" callee to be as simple as
 *     75caeabc 8bd4        mov     edx,esp
 *     75caeabe 0f34        sysenter
 *     75caeac0 c3          ret
 */
#define MAX_INSTRS_SYSENTER_CALLEE  4
/* the max distance from call to the sysenter callee target */
#define MAX_SYSENTER_CALLEE_OFFSET  0x50
#define MAX_INSTRS_BEFORE_SYSCALL 16
#define MAX_INSTRS_IN_FUNCTION 256

/* Returns whether found a syscall.
 * - found_eax: whether the caller has seen "mov imm => %eax"
 * - found_edx: whether the caller has seen "mov $0x7ffe0300 => %edx",
 *              xref the comment below about "mov $0x7ffe0300 => %edx".
 */
static bool
process_syscall_instr(void *dcontext, instr_t *instr, bool found_eax, bool found_edx)
{
    /* ASSUMPTION: a mov imm of 0x7ffe0300 into edx followed by an
     * indirect call via edx is a system call on XP and later
     * On XP SP1 it's call *edx, while on XP SP2 it's call *(edx)
     * For wow it's a call through fs.
     * XXX: DR's core exports various is_*_syscall routines (such as
     * instr_is_wow64_syscall()) which we could use here instead of
     * duplicating if they were more flexible about when they could
     * be called (instr_is_wow64_syscall() for ex. asserts if not
     * in a wow process).
     */
    bool is_wow64 = dr_is_wow64();
    if (/* int 2e or x64 or win8 sysenter */
        (instr_is_syscall(instr) &&
         found_eax && !is_wow64) ||
        /* sysenter case */
        (found_edx && found_eax &&
         instr_is_call_indirect(instr) &&
         /* XP SP{0,1}, 2003 SP0: call *edx */
         ((opnd_is_reg(instr_get_target(instr)) &&
           opnd_get_reg(instr_get_target(instr)) == DR_REG_EDX) ||
          /* XP SP2, 2003 SP1: call *(edx) */
          (opnd_is_base_disp(instr_get_target(instr)) &&
           opnd_get_base(instr_get_target(instr)) == DR_REG_EDX &&
           opnd_get_index(instr_get_target(instr)) == DR_REG_NULL &&
           opnd_get_disp(instr_get_target(instr)) == 0))) ||
        /* wow case
         * we don't require found_ecx b/c win8 does not use ecx
         */
        (is_wow64 && found_eax &&
         instr_is_call_indirect(instr) &&
         ((opnd_is_far_base_disp(instr_get_target(instr)) &&
           opnd_get_base(instr_get_target(instr)) == DR_REG_NULL &&
           opnd_get_index(instr_get_target(instr)) == DR_REG_NULL &&
           opnd_get_segment(instr_get_target(instr)) == DR_SEG_FS) ||
          /* win10 has imm in edx and a near call */
          found_edx)))
        return true;
    return false;
}

/* Returns whether found a syscall.
 * - found_eax: whether the caller has seen "mov imm => %eax"
 * - found_edx: whether the caller has seen "mov $0x7ffe0300 => %edx",
 *              xref the comment in process_syscall_instr.
 */
static bool
process_syscall_call(void *dcontext, byte *next_pc, instr_t *call,
                     bool found_eax, bool found_edx)
{
    int num_instr;
    byte *pc;
    instr_t instr;
    bool found_syscall = false;

    assert(instr_get_opcode(call) == OP_call && opnd_is_pc(instr_get_target(call)));
    pc = opnd_get_pc(instr_get_target(call));
    if (pc > next_pc + MAX_SYSENTER_CALLEE_OFFSET ||
        pc <= next_pc /* assuming the call won't go backward */)
        return false;
    /* Handle win8 x86 which has sysenter callee adjacent-"inlined":
     *     ntdll!NtYieldExecution:
     *     77d7422c b801000000  mov     eax,1
     *     77d74231 e801000000  call    ntdll!NtYieldExecution+0xb (77d74237)
     *     77d74236 c3          ret
     *     77d74237 8bd4        mov     edx,esp
     *     77d74239 0f34        sysenter
     *     77d7423b c3          ret
     *
     * Or DrMem-i#1366-c#2:
     *     USER32!NtUserCreateWindowStation:
     *     75caea7a b841110000  mov     eax,0x1141
     *     75caea7f e838000000  call    user32!...+0xd (75caeabc)
     *     75caea84 c22000      ret     0x20
     *     ...
     *     USER32!GetWindowStationName:
     *     75caea8c 8bff        mov     edi,edi
     *     75caea8e 55          push    ebp
     *     ...
     *     75caeabc 8bd4        mov     edx,esp
     *     75caeabe 0f34        sysenter
     *     75caeac0 c3          ret
     */
    /* We expect the win8 x86 sysenter adjacent "inlined" callee to be as simple as
     *     75caeabc 8bd4        mov     edx,esp
     *     75caeabe 0f34        sysenter
     *     75caeac0 c3          ret
     */
    instr_init(dcontext, &instr);
    num_instr = 0;
    do {
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        if (op_verbose_level >= 3)
            dr_print_instr(dcontext, STDOUT, &instr, "");
        if (pc == NULL || !instr_valid(&instr))
            break;
        if (instr_is_syscall(&instr) || instr_is_call_indirect(&instr)) {
            found_syscall = process_syscall_instr(dcontext, &instr, found_eax, found_edx);
            break;
        } else if (instr_is_cti(&instr)) {
            break;
        }
        num_instr++;
    } while (num_instr <= MAX_INSTRS_SYSENTER_CALLEE);
    instr_free(dcontext, &instr);
    return found_syscall;
}

static int
get_syscall_num(void *dcontext, byte *pc, byte *mod_start, byte *mod_end)
{
    bool found_syscall = false, found_eax = false, found_edx = false, found_ecx = false;
    byte *pre_pc;
    int sysnum = -1;
    int num_instr = 0;
    bool is_wow64 = dr_is_wow64();
    bool is_x64 = IF_X64_ELSE(true, false);
    instr_t instr;
    instr_init(dcontext, &instr);

    while (true) {
        instr_reset(dcontext, &instr);
        pre_pc = pc;
        pc = decode(dcontext, pc, &instr);
        if (op_verbose_level >= 3) {
            instr_set_translation(&instr, pre_pc);
            dr_print_instr(dcontext, STDOUT, &instr, "");
        }
        if (pc == NULL || !instr_valid(&instr))
            break;
        if (instr_is_syscall(&instr) || instr_is_call_indirect(&instr)) {
            /* If we see a syscall instr or an indirect call which is not syscall,
             * we assume this is not a syscall wrapper.
             */
            found_syscall = process_syscall_instr(dcontext, &instr, found_eax, found_edx);
            if (!found_syscall)
                break; /* assume not a syscall wrapper, give up gracefully */
        } else if (instr_is_return(&instr)) {
            /* we must break on return to avoid case like win8 x86
             * which has sysenter callee adjacent-"inlined"
             *     ntdll!NtYieldExecution:
             *     77d7422c b801000000  mov     eax,1
             *     77d74231 e801000000  call    ntdll!NtYieldExecution+0xb (77d74237)
             *     77d74236 c3          ret
             *     77d74237 8bd4        mov     edx,esp
             *     77d74239 0f34        sysenter
             *     77d7423b c3          ret
             */
            /* If we wanted the # args we'd get it here, for stdcall. */
            break;
        } else if (instr_get_opcode(&instr) == OP_call) {
            found_syscall = process_syscall_call(dcontext, pc, &instr,
                                                 found_eax, found_edx);
            /* If we see a call and it is not a sysenter callee,
             * we assume this is not a syscall wrapper.
             */
            if (!found_syscall)
                break; /* assume not a syscall wrapper, give up gracefully */
        } else if (instr_is_cti(&instr)) {
            /* We expect only ctis like ret or ret imm, syscall, and call, which are
             * handled above. Give up gracefully if we hit any other cti.
             * XXX: what about jmp to shared ret (seen in the past on some syscalls)?
             */
            /* Update: win10 TH2 1511 x64 has a cti:
             *   ntdll!NtContinue:
             *   00007ff9`13185630 4c8bd1          mov     r10,rcx
             *   00007ff9`13185633 b843000000      mov     eax,43h
             *   00007ff9`13185638 f604250803fe7f01 test byte ptr [SharedUserData+0x308
             *                                                     (00000000`7ffe0308)],1
             *   00007ff9`13185640 7503            jne     ntdll!NtContinue+0x15
             *                                             (00007ff9`13185645)
             *   00007ff9`13185642 0f05            syscall
             *   00007ff9`13185644 c3              ret
             *   00007ff9`13185645 cd2e            int     2Eh
             *   00007ff9`13185647 c3              ret
             */
            if (is_x64 && instr_is_cbr(&instr) &&
                opnd_get_pc(instr_get_target(&instr)) == pc + 3/*syscall;ret*/) {
                /* keep going */
            } else
                break;
        } else if ((!found_eax || !found_edx || !found_ecx) &&
                   instr_get_opcode(&instr) == OP_mov_imm &&
                   opnd_is_reg(instr_get_dst(&instr, 0))) {
            if (!found_eax && opnd_get_reg(instr_get_dst(&instr, 0)) == DR_REG_EAX) {
                sysnum = (int) opnd_get_immed_int(instr_get_src(&instr, 0));
                found_eax = true;
            } else if (!found_edx &&
                       opnd_get_reg(instr_get_dst(&instr, 0)) == DR_REG_EDX) {
                uint imm = (uint) opnd_get_immed_int(instr_get_src(&instr, 0));
                if (imm == 0x7ffe0300 ||
                    /* On Win10 the immed is ntdll!Wow64SystemServiceCall */
                    (is_wow64 && imm > (ptr_uint_t)mod_start &&
                     imm < (ptr_uint_t)mod_end))
                    found_edx = true;
            } else if (!found_ecx &&
                       opnd_get_reg(instr_get_dst(&instr, 0)) == DR_REG_ECX) {
                found_ecx = true;
                /* If we wanted the wow64 fixup index we'd get it here */
            }
        } else if (instr_get_opcode(&instr) == OP_xor &&
                   opnd_is_reg(instr_get_src(&instr, 0)) &&
                   opnd_get_reg(instr_get_src(&instr, 0)) == DR_REG_ECX &&
                   opnd_is_reg(instr_get_dst(&instr, 0)) &&
                   opnd_get_reg(instr_get_dst(&instr, 0)) == DR_REG_ECX) {
            /* xor to 0 */
            found_ecx = true;
        }
        num_instr++;
        if (num_instr > MAX_INSTRS_BEFORE_SYSCALL) /* wrappers should be short! */
            break; /* avoid weird cases like NPXEMULATORTABLE */
    }
    instr_free(dcontext, &instr);

    if (found_syscall)
        return sysnum;
    return -1;
}

typedef struct _search_data_t {
    void *dcontext;
    byte *dll_base;
    size_t dll_size;
    std::string prefix_to_add;
    std::unordered_map<std::string, int> *name2num;
    byte *usercall_addr[NUM_USERCALL];
    size_t usercall_targets_found;
    size_t usercalls_found;
} search_data_t;

static bool
search_syms_cb(const char *name, size_t modoffs, void *data)
{
    NOTIFY(3, "Found symbol \"%s\" at offs "PIFX NL, name, modoffs);
    search_data_t *sd = (search_data_t *) data;
    /* XXX DRi#2715: drsyms sometimes passes bogus offsets so we're robust here */
    if (modoffs >= sd->dll_size)
        return true;
    /* We ignore the Zw variant */
    if (name[0] != 'Z' || name[1] != 'w') {
        int num = get_syscall_num(sd->dcontext, sd->dll_base + modoffs, sd->dll_base,
                                  sd->dll_base + sd->dll_size);
        std::string add_as = std::string(name);
        if (name[0] != 'N' || name[1] != 't')
            add_as = sd->prefix_to_add + add_as;
        if (num != -1)
            (*sd->name2num)[add_as] = num;
    }
    if (sd->usercall_targets_found > 0) {
        std::pair<std::string, int> name_num =
            look_for_usercall(sd->dcontext, sd->dll_base + modoffs, name,
                              sd->dll_base + sd->dll_size, sd->usercall_addr);
        if (name_num.second != -1) {
            NOTIFY(2, "Adding usercall %s = 0x%x" NL, name_num.first.c_str(),
                   name_num.second);
            ++sd->usercalls_found;
            (*sd->name2num)[name_num.first] = name_num.second;
        }
    }
    return true; /* keep iterating */
}

static drmf_status_t
identify_syscalls(void *dcontext, const char **dlls, size_t num_dlls,
                  std::unordered_map<std::string, int> &name2num)
{
    if (drsym_init(NULL) != DRSYM_SUCCESS) {
        NOTIFY(0, "Failed to initialize drsyms" NL);
        return DRMF_ERROR;
    }
    for (size_t i = 0; i < num_dlls; ++i) {
        size_t map_size;
        byte *map_base = dr_map_executable_file(dlls[i], DR_MAPEXE_SKIP_WRITABLE,
                                                &map_size);
        if (map_base == NULL) {
            NOTIFY(0, "Failed to map \"%s\"" NL, dlls[i]);
            return DRMF_ERROR;
        }

        /* We go through all symbols. */
        size_t prev_size = name2num.size();
        std::string pattern(std::string(dlls[i]) + "!*");
        std::string prefix = "";
        if (strcasestr(dlls[i], "user32") != NULL ||
            strcasestr(dlls[i], "win32u") != NULL ||
            strcasestr(dlls[i], "imm32") != NULL)
            prefix = "NtUser";
        else if (strcasestr(dlls[i], "gdi32") != NULL)
            prefix = "NtGdi";
        NOTIFY(1, "Searching for system calls in \"%s\"" NL, dlls[i]);
        search_data_t sd = {dcontext, map_base, map_size, prefix, &name2num, };
        look_for_usercall_targets(dlls[i], map_base, map_size, sd.usercall_addr,
                                  &sd.usercall_targets_found);
        drsym_error_t symres = drsym_search_symbols(dlls[i], pattern.c_str(),
                                                    true, search_syms_cb, &sd);
        if (symres != DRSYM_SUCCESS) {
            NOTIFY(0, "Error %d searching \"%s\"" NL, symres, dlls[i]);
            return DRMF_ERROR;
        }
        NOTIFY(1, "\tFound %d system calls (%d usercalls) in \"%s\"" NL,
               name2num.size() - prev_size, sd.usercalls_found, dlls[i]);
        if (!dr_unmap_executable_file(map_base, map_size))
            NOTIFY(0, "Failed to unmap \"%s\"" NL, dlls[i]);
        /* We may as well clean up.  This also prevents drsyms from searching
         * in other libraries, in case our qualified names fall back to global.
         */
        symres = drsym_free_resources(dlls[i]);
        if (symres != DRSYM_SUCCESS) {
            NOTIFY(0, "Error %d unloading \"%s\"" NL, symres, dlls[i]);
            /* This is a non-fatal error. */
        }
    }
    drsym_exit();
    return DRMF_SUCCESS;
}

/***************************************************************************
 * Write out file
 */

static bool
cmp_names(const std::pair<std::string, int> &left,
          const std::pair<std::string, int> &right)
{
    return left.first < right.first;
}

static drmf_status_t
write_file(const std::unordered_map<std::string, int> &name2num, const std::string &outf)
{
    std::string key("NtGetContextThread");
    if (name2num.find(key) == name2num.end()) {
        NOTIFY(0, "Failed to determine number for %s" NL, key.c_str());
        return DRMF_ERROR;
    }
    file_t f = dr_open_file(outf.c_str(), DR_FILE_WRITE_OVERWRITE);
    if (f == INVALID_FILE) {
        NOTIFY(0, "Failed to open %s" NL, outf.c_str());
        return DRMF_ERROR_ACCESS_DENIED;
    }
    NOTIFY(1, "Writing to \"%s\"" NL, outf.c_str());
    dr_fprintf(f, "%s\n%d\n%s\n", DRSYS_SYSNUM_FILE_HEADER, DRSYS_SYSNUM_FILE_VERSION,
               key.c_str());
    dr_fprintf(f, "START=0x%x\n", name2num.at(key));

    /* It doesn't have to be sorted but it's nicer for humans this way. */
    std::vector<std::pair<std::string, int> > sorted(name2num.size());
    std::partial_sort_copy(name2num.begin(), name2num.end(),
                           sorted.begin(), sorted.end(), cmp_names);
    for (const auto &keyval : sorted) {
        NOTIFY(2, "%s == 0x%x" NL, keyval.first.c_str(), keyval.second);
        dr_fprintf(f, "%s=0x%x\n", keyval.first.c_str(), keyval.second);
    }

    // The usercalls are different because we can't find wrappers for many of them.
    // They run in consecutive numbers so we fill in gaps as best we can.
    std::unordered_map<int, std::string> num2name;
    for (const auto &keyval : name2num) {
        num2name[keyval.second] = keyval.first;
    }
    int num = -1;
#define NONE -1
#define USERCALL(type, name, w2k, xp, w2003, vistaSP01, vistaSP2, w7, w8, w81, w10, \
                 w11, w12, w13, w14, w15) \
    do {                                    \
        std::string sysname(#type"."#name); \
        auto keyval = name2num.find(sysname); \
        if (keyval != name2num.end())               \
            num = (*keyval).second;             \
        else if (w15 != -1) { /* Assume once gone it's not coming back */ \
            ++num; \
            /* If an entry was removed we'll collide.  Just skip in that case. */\
            /* Since the table order is not perfect we'll miss some. */\
            if (num2name.find(num) == num2name.end()) { \
                NOTIFY(2, "%s == 0x%x" NL, sysname.c_str(), num); \
                dr_fprintf(f, "%s=0x%x\n", sysname.c_str(), num); \
                num2name[num] = sysname; \
            }\
        } \
    } while (false);
#include "drsyscall_usercallx.h"
#undef USERCALL
#undef NONE

    dr_fprintf(f, "%s\n", DRSYS_SYSNUM_FILE_FOOTER);
    dr_close_file(f);
    NOTIFY(0, "Successfully wrote \"%s\"" NL, outf.c_str());
    return DRMF_SUCCESS;
}

/***************************************************************************
 * Top-level
 */

DR_EXPORT
drmf_status_t
drsys_generate_sysnum_file(void *drcontext, const char **sysnum_lib_paths,
                           size_t num_sysnum_libs, const char *outfile,
                           const char *cache_dir)
{
    bool ok;
    if (drfront_access(cache_dir, DRFRONT_WRITE, &ok) != DRFRONT_SUCCESS || !ok) {
        NOTIFY(0, "Invalid -cache_dir: cannot find/write %s" NL, cache_dir);
        return DRMF_ERROR_INVALID_PARAMETER;
    }
    NOTIFY(1, "Symbol cache directory is \"%s\"" NL, cache_dir);

    std::unordered_map<std::string, int> name2num;

    drmf_status_t res = fetch_symbols(sysnum_lib_paths, num_sysnum_libs, cache_dir);
    if (res != DRMF_SUCCESS)
        return res;

    res = identify_syscalls(drcontext, sysnum_lib_paths, num_sysnum_libs, name2num);
    if (res != DRMF_SUCCESS)
        return res;

    res = write_file(name2num, outfile);
    if (res != DRMF_SUCCESS)
        return res;

    return DRMF_SUCCESS;
}
