' **********************************************************
' Copyright (c) 2006 VMware, Inc.  All rights reserved.
' **********************************************************

' Redistribution and use in source and binary forms, with or without
' modification, are permitted provided that the following conditions are met:
'
' * Redistributions of source code must retain the above copyright notice,
'   this list of conditions and the following disclaimer.
'
' * Redistributions in binary form must reproduce the above copyright notice,
'   this list of conditions and the following disclaimer in the documentation
'   and/or other materials provided with the distribution.
'
' * Neither the name of VMware, Inc. nor the names of its contributors may be
'   used to endorse or promote products derived from this software without
'   specific prior written permission.
'
' THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
' IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
' ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
' FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
' DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
' SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
' CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
' LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
' OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
' DAMAGE.

strComputer = "."   ' "." for local computer

On Error Resume Next
Set objWMIService = GetObject("winmgmts:" _
        & "{impersonationLevel=impersonate}!\\" & strComputer _
        & "\root\cimv2")

If Err.Number = 0 Then
     On Error Goto 0
     strQuery = "select * from Win32_PerfRawData_PerfOS_System"
     Set colObjects = objWMIService.ExecQuery(strQuery)

     For Each objWmiObject In colObjects
       intPerfTimeStamp = objWmiObject.Timestamp_Object
       intPerfTimeFreq = objWmiObject.Frequency_Object
       intCounter = objWmiObject.SystemUpTime
     Next

     ' Calculation in seconds:
     'Calculations for Raw Counter Data:PERF_ELAPSED_TIME
     'http://msdn.microsoft.com/library/en-us/perfmon/base/perf_elapsed_time.asp
     iUptimeInSec = (intPerfTimeStamp - intCounter)/intPerfTimeFreq
     WScript.Echo "Uptime in seconds: " & iUptimeInSec \ 1

     ' convert the seconds
     sUptime = ConvertTime(iUptimeInSec)
     WScript.Echo "Uptime: " & sUptime

Else
     Wscript.Echo "Could not connect to computer with WMI: " _
         & strComputer
End If


Function ConvertTime(seconds)
     ConvSec = seconds Mod 60
     ConvMin = (seconds Mod 3600) \ 60
     ConvHour =  (seconds Mod (3600 * 24)) \ 3600
     ConvDays =  seconds \ (3600 * 24)
     ConvertTime = ConvDays & " days " & ConvHour & " hours " _
                 & ConvMin & " minutes " & ConvSec & " seconds "
End Function
