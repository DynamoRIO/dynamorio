@echo off
REM **********************************************************
REM Copyright (c) 2009 VMware, Inc.  All rights reserved.
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

set USAGE="usage: <VisStudio root> <source path> <publish path> <ver#> <build#> <uniquebuild#>";
if ""=="%6" (
    echo %USAGE%
    exit /b 1
) else if not ""=="%7" (
    echo %USAGE%
    exit /b 1
)
set VSROOT=%1
set SRCDIR=%2
set PUBLISHDIR=%3
set VERNUM=%4
set BUILDNUM=%5
set UNIQUE_BUILDNUM=%6

set CUR_DIR=%CD%

set DEFS=-DVERSION_NUMBER:STRING=%VERNUM% -DBUILD_NUMBER:STRING=%BUILDNUM% -DUNIQUE_BUILD_NUMBER:STRING=%UNIQUE_BUILDNUM%

set VS80COMNTOOLS=%VSROOT%\Common7\Tools\
call %VSROOT%\VC\vcvarsall.bat x86

rd /s /q build32rel
rd /s /q build32dbg
rd /s /q build64rel
rd /s /q build64dbg

md build32rel
cd build32rel
cmake -G"NMake Makefiles" %DEFS% -DX64:BOOL=OFF %SRCDIR%
nmake

md ..\build32dbg
cd ..\build32dbg
cmake -G"NMake Makefiles" %DEFS% -DX64:BOOL=OFF -DDEBUG:BOOL=ON -DBUILD_TOOLS:BOOL=OFF -DBUILD_DOCS:BOOL=OFF -DBUILD_DRGUI:BOOL=OFF -DBUILD_SAMPLES:BOOL=OFF %SRCDIR%
nmake

call %VSROOT%\VC\vcvarsall.bat x64

md ..\build64rel
cd ..\build64rel
cmake -G"NMake Makefiles" %DEFS% -DBUILD_DOCS:BOOL=OFF %SRCDIR%
nmake

md ..\build64dbg
cd ..\build64dbg
cmake -G"NMake Makefiles" %DEFS% -DDEBUG:BOOL=ON -DBUILD_TOOLS:BOOL=OFF -DBUILD_DOCS:BOOL=OFF -DBUILD_DRGUI:BOOL=OFF -DBUILD_SAMPLES:BOOL=OFF %SRCDIR%
nmake

REM cmake handles backslashes in env vars fine, but complains if
REM in a direct path or string, so be sure to go through $ENV
echo string(REGEX REPLACE "\\\\" "/" curdir "$ENV{CUR_DIR}") >> CPackConfig.cmake
echo set(CPACK_INSTALL_CMAKE_PROJECTS >> CPackConfig.cmake
REM debug first, since we do have some files that overlap: drinject,
REM and the lib/ files, for which we want release build
echo   "${curdir}/build32dbg;DynamoRIO;ALL;/" >> CPackConfig.cmake
echo   "${curdir}/build32rel;DynamoRIO;ALL;/" >> CPackConfig.cmake
echo   "${curdir}/build64dbg;DynamoRIO;ALL;/" >> CPackConfig.cmake
echo   "${curdir}/build64rel;DynamoRIO;ALL;/" >> CPackConfig.cmake
echo   ) >> CPackConfig.cmake
nmake package

rd /s /q %PUBLISHDIR%
md %PUBLISHDIR%
copy DynamoRIO-*.* %PUBLISHDIR%
