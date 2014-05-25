REM **********************************************************
REM Copyright (c) 2006 VMware, Inc.  All rights reserved.
REM **********************************************************

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

rem "Set up machine for medium load (beyond Lionfish Desktop configuration"
rem "default specifed by PM"
rem "make 26 more services listed in PM's spreadsheet automatic"

rem "default list of services enabled (see below)"
svccntrl "Alerter" -auto
svccntrl "Application Layer Gateway Service" -auto
svccntrl "Application Management" -auto
svccntrl "Background Intelligent Transfer Service" -auto
svccntrl "Client Service for NetWare" -disabled
svccntrl "Clipbook" -auto
svccntrl "COM+ System Application" -auto
svccntrl "Distributed Transaction Coordinator" -auto
svccntrl "Fast User Switching Compatibility" -auto
svccntrl "Fax" -auto
svccntrl "HTTP SSL" -auto
svccntrl "Human Interface Device Access" -disabled
svccntrl "IMAPI CD-Burning COM Service" -auto
svccntrl "Indexing Service" -auto
svccntrl "Infrared Monitor" -disabled
svccntrl "IPv6 Helper Service" -disabled
svccntrl "Logical Disk Manager Administrative Service" -disabled
svccntrl "Microsoft Software Shadow Copy Provider" -disabled
svccntrl "Net Logon" -auto
svccntrl "NetMeeting Remote Desktop Sharing" -disabled
svccntrl "Network DDE" -disabled
svccntrl "Network DDE DSDM" -disabled
svccntrl "NT LM Security Support Provider" -disabled
svccntrl "Performance Logs and Alerts" -disabled
svccntrl "QoS RSVP" -disabled
svccntrl "Remote Access Auto Connection Manager" -auto
svccntrl "Remote Desktop Help Session Manager" -auto
svccntrl "Remote Procedure Call (RPC) Locator" -auto
svccntrl "Removable Storage" -disabled
svccntrl "RIP Listener" -disabled
svccntrl "Routing and Remote Access" -disabled
svccntrl "SAP Agent" -disabled
svccntrl "Simple TCP/IP Services" -auto
svccntrl "Smart Card" -auto
svccntrl "TCP/IP Print Server" -auto
svccntrl "Telnet" -auto
svccntrl "Uninterruptible Power Supply" -disabled
svccntrl "Universal Plug and Play Device Host" -auto
svccntrl "Volume Shadow Copy" -disabled
svccntrl "Windows Management Instrumentation Driver Extensions" -auto
svccntrl "WMI Performance Adapter" -auto
svccntrl "World Wide Web Publishing Service" -auto
svccntrl "Portable Media Serial Number Service" -auto
svccntrl "Network Provisioning" -auto

rem "make services in defult configuration automatic"

svccntrl "Automatic Updates" -auto
svccntrl "COM+ Event System" -auto
svccntrl "Computer Browser" -auto
svccntrl "Cryptographic Services" -auto
svccntrl "DHCP Client" -auto
svccntrl "Distributed Link Tracking Client" -auto
svccntrl "DNS Client" -auto
svccntrl "Error Reporting Service" -auto
svccntrl "Event Log" -auto
svccntrl "Help and Support" -auto
svccntrl "IPSEC Services" -auto
svccntrl "Logical Disk Manager" -auto
svccntrl "Messenger" -auto
svccntrl "Network Connections" -auto
svccntrl "Network Location Awareness (NLA)" -auto
svccntrl "Plug and Play" -auto
svccntrl "Portable Media Serial Number" -auto
svccntrl "Print Spooler" -auto
svccntrl "Protected Storage" -auto
svccntrl "Remote Access Connection Manager" -auto
svccntrl "Remote Procedure Call (RPC)" -auto
svccntrl "Remote Registry" -auto
svccntrl "Secondary Logon" -auto
svccntrl "Security Accounts Manager" -auto
svccntrl "Server" -auto
svccntrl "Shell Hardware Detection" -auto
svccntrl "SSDP Discovery Service" -auto
svccntrl "System Event Notification" -auto
svccntrl "System Restore Service" -auto
svccntrl "Task Scheduler" -auto
svccntrl "TCP/IP NetBIOS Helper" -auto
svccntrl "Telephony" -auto
svccntrl "Terminal Services" -auto
svccntrl "Themes" -auto
svccntrl "Upload Manager" -auto
svccntrl "WebClient" -auto
svccntrl "Windows Audio" -auto
svccntrl "Windows Image Acquisition (WIA)" -auto
svccntrl "Windows Installer" -auto
svccntrl "Windows Management Instrumentation" -auto
svccntrl "Windows Time" -auto
svccntrl "Wireless Configuration" -auto
svccntrl "Workstation" -auto
svccntrl "DCOM Server Process Launcher" -auto
svccntrl "Windows Firewall/Internet Connection Sharing (ICS)" -auto
svccntrl "Security Center" -auto
