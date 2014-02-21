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

set USAGE="usage: <cl32.bat> <cl64.bat> <cmake extra args> <source path> <publish path (w/ backslashes)> <ver#> <build#> <uniquebuild#>";
REM Pass "" for ver# to use the default.
if ""=="%8" (
    echo %USAGE%
    exit /b 1
) else if not ""=="%9" (
    echo %USAGE%
    exit /b 1
)
set CL32BAT=%1
set CL64BAT=%2
set CUSTOM=%3
set SRCDIR=%4
set PUBLISHDIR=%5
set VERNUM=%6
set BUILDNUM=%7
set UNIQUE_BUILDNUM=%8

REM remove surrounding quotes since "-C foo.cmake" => vague
REM "Error processing foo.cmake" error
for /f "useback tokens=*" %%a in ('%CUSTOM%') do set CUSTOM=%%~a

set CUR_DIR=%CD%

REM I filed a CMake feature request http://www.cmake.org/Bug/view.php?id=8726
REM TODO: once 2.6.4 is released we should add -DCMAKE_RULE_MESSAGES=OFF to
REM turn off both progress and status messages and speed up the build

set DEFS=-DBUILD_NUMBER:STRING=%BUILDNUM% -DUNIQUE_BUILD_NUMBER:STRING=%UNIQUE_BUILDNUM%
if not ""==%VERNUM% (
   set DEFS=-DVERSION_NUMBER:STRING=%VERNUM% %DEFS%
)

call %CL32BAT%

rd /s /q build32rel
rd /s /q build32dbg
rd /s /q build64rel
rd /s /q build64dbg

md build32rel
cd build32rel
cmake -G"NMake Makefiles" %DEFS% %CUSTOM% %SRCDIR%
nmake

md ..\build32dbg
cd ..\build32dbg
cmake -G"NMake Makefiles" %DEFS% %CUSTOM% -DDEBUG:BOOL=ON -DBUILD_TOOLS:BOOL=OFF -DBUILD_DOCS:BOOL=OFF -DBUILD_DRSTATS:BOOL=OFF -DBUILD_SAMPLES:BOOL=OFF %SRCDIR%
nmake

call %CL64BAT%

md ..\build64rel
cd ..\build64rel
cmake -G"NMake Makefiles" %DEFS% %CUSTOM% -DBUILD_DOCS:BOOL=OFF %SRCDIR%
nmake

md ..\build64dbg
cd ..\build64dbg
cmake -G"NMake Makefiles" %DEFS% %CUSTOM% -DDEBUG:BOOL=ON -DBUILD_TOOLS:BOOL=OFF -DBUILD_DOCS:BOOL=OFF -DBUILD_DRSTATS:BOOL=OFF -DBUILD_SAMPLES:BOOL=OFF %SRCDIR%
nmake

REM handle read-only sources
attrib -R CPackConfig.cmake
REM we're generating raw cmake so we can't put single backslashes in:
REM so we have cmake read in via $ENV
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
