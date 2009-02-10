REM **********************************************************
REM Copyright (c) 2008 VMware, Inc.  All rights reserved.
REM **********************************************************
REM
REM Redistribution and use in source and binary forms, with or without
REM modification, are permitted provided that the following conditions are met:
REM 
REM * Redistributions of source code must retain the above copyright notice,
REM   this list of conditions and the following disclaimer.
REM 
REM * Redistributions in binary form must reproduce the above copyright notice,
REM   this list of conditions and the following disclaimer in the documentation
REM   and/or other materials provided with the distribution.
REM 
REM * Neither the name of VMware, Inc. nor the names of its contributors may be
REM   used to endorse or promote products derived from this software without
REM   specific prior written permission.
REM 
REM THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
REM AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
REM IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
REM ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
REM FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
REM DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
REM SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
REM CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
REM LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
REM OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
REM DAMAGE.

@echo off

REM This script provides the functionality common to all
REM the batch scripts in this module: it sets up the cygwin
REM mount points and provides the proper path to the
REM cygwin utilities in our local toolchain module.  All
REM .bat files launching a cygwin command should call this
REM script first and then invoke a bash shell with the
REM path set to BASH_PATH.

if {%TCROOT%}=={} goto error

if not {%DYNAMORIO_TOOLCHAIN%}=={} goto set_reg

REM If DYNAMORIO_TOOLCHAIN is not set, provide a default
REM value relative to the location of this script.
set CUR_DIR=%CD%
cd %~dp0\..
set DYNAMORIO_TOOLCHAIN=%CD%\toolchain
cd %CUR_DIR%

REM Cygwin utils won't work correctly without the mount points
REM specified in the registry.  Unmount first and then force 
REM /, /usr/bin, and /usr/lib to point to our toolchain module.
:set_reg
set CYGDIR=%DYNAMORIO_TOOLCHAIN%\win32\cygwin-1.5.25-7
set CYGBIN=%CYGDIR%\bin
set CYGX11=%CYGDIR%\usr\X11R6\bin

%CYGBIN%\umount -A
%CYGBIN%\mount -f %CYGDIR% /
%CYGBIN%\mount -f %CYGDIR%\bin /usr/bin
%CYGBIN%\mount -f %CYGDIR%\lib /usr/lib

REM Provide a variable the calling .bat script can use to set
REM the path when invoking its cygwin command.
set BASH_PATH='%CYGBIN%':'%CYGX11%':$PATH
goto :eof

:error
echo ERROR: you must set TCROOT
exit /b 1
