#!/bin/sh

# **********************************************************
# Copyright (c) 2008 VMware, Inc.  All rights reserved.
# **********************************************************

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of VMware, Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

echo "This script will install DynamoRIO to the current directory."
echo "Continue [y/n]"

read do_install

if [ "$do_install" = "n" ] && [ "$do_install" = "N" ]
then
    exit
fi

echo "You must read and accept the licensing agreement before installing."
echo "Press enter to read the licensing agreement"
read tmp
echo ""

# This text is verbatim from the modified PRBA provided by David Howard 4/2/08
echo "NOTICE: BY DOWNLOADING AND INSTALLING, COPYING OR OTHERWISE USING "
echo "THE INTERFACES, SOFTWARE AND OTHER MATERIALS ON THIS CD, YOU "
echo "AGREE TO BE BOUND BY THE TERMS OF THIS AGREEMENT.  IF YOU DO NOT "
echo "AGREE TO THE TERMS OF THIS EULA, YOU MAY NOT DOWNLOAD, INSTALL, "
echo "COPY OR USE THE INTERFACES, SOFTWARE OR OTHER MATERIALS ON THIS "
echo "CD. \"YOU\" MEANS THE NATURAL PERSON OR THE ENTITY THAT IS "
echo "AGREEING TO BE BOUND BY THIS EULA, THEIR EMPLOYEES AND THIRD "
echo "PARTY CONTRACTORS THAT PROVIDE SERVICES TO YOU. YOU SHALL BE "
echo "LIABLE FOR ANY FAILURE BY SUCH EMPLOYEES AND THIRD PARTY "
echo "CONTRACTORS TO COMPLY WITH THE TERMS OF THIS AGREEMENT. "
echo "  "
echo "1. DEFINITIONS. "
echo " "
echo "(a \"Software\" shall mean the VMware software, in object code only, as set "
echo "forth in Exhibit A attached hereto. "
echo " "
echo "(b \"Documentation\" shall mean the printed or online written reference "
echo "material, if any, that may be furnished to Licensee in conjunction with the "
echo "Software, including, without limitation, instructions, testing guidelines, "
echo "and end user guides. "
echo " "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "(c \"Intellectual Property Rights\" shall mean all intellectual property "
echo "rights, including, without limitation, patent, copyright, trademark, and "
echo "trade secret. "
echo " "
echo "(d \"Open Source Software\" shall mean various open source software "
echo "components provided with the Software that are licensed to Licensee under "
echo "the terms of the applicable license agreements included with such open "
echo "source software components or other materials for the Software. "
echo " "
echo "(e \"Update(s\" shall mean any modification, error correction, bug fix, "
echo "patch or other update to or for the Software. "
echo " "
echo "2. LICENSE GRANT, USE AND OWNERSHIP "
echo " "
echo "(a License.  Subject to the terms and conditions of this Agreement, VMware "
echo "grants to Licensee a non-exclusive, revocable, non-transferable license "
echo "(without the right to sublicense to use the Software and Documentation "
echo "solely for purposes of internal testing and evaluation, as well as "
echo "development of Licensee products that communicate with the Software. "
echo " "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "(b Feedback. The purpose of this license is the internal testing and "
echo "evaluation of the Software by Licensee and development by Licensee of "
echo "Licensee products that communicate with the Software.  In furtherance of "
echo "this purpose, Licensee shall from time to time provide feedback to VMware "
echo "concerning the functionality and performance of the Software including, "
echo "without limitation, identifying potential errors and improvements. "
echo "Notwithstanding the foregoing, prior to Licensee disclosing to VMware any "
echo "information in connection with this Agreement which Licensee considers "
echo "proprietary or confidential, Licensee shall obtain VMware's prior written "
echo "approval to disclose such information to VMware, and without such prior "
echo "written approval from VMware, Licensee shall not disclose any such "
echo "information to VMware.  Feedback and other information which is provided by "
echo "Licensee to VMware in connection with the Software, Documentation, or this "
echo "Agreement may be used by VMware to improve or enhance its products and, "
echo "accordingly, VMware shall have a non-exclusive, perpetual, irrevocable, "
echo "royalty-free, worldwide right and license to use, reproduce, disclose, "
echo "sublicense, modify, make, have made, distribute, sell, offer for sale, "
echo "display, perform, create derivative works, permit unmodified binary "
echo "distribution and otherwise exploit such feedback and information without "
echo "restriction. "
echo " "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "(c Restrictions.  Licensee shall not copy or use the Software (including "
echo "the Documentation or disseminate Confidential Information, as defined "
echo "below, to any third party except as expressly permitted in this Agreement. "
echo "Licensee will not, and will not permit any third party to, sublicense, "
echo "rent, copy, modify, create derivative works of, translate, reverse "
echo "engineer, decompile, disassemble, or otherwise reduce to human perceivable "
echo "form any portion of the Software or Documentation.  In no event shall "
echo "Licensee use the Software or Documentation for any commercial purpose "
echo "except as expressly set forth in this Agreement.  The Software, "
echo "Documentation, and all performance data and test results, including without "
echo "limitation, benchmark test results (collectively \"Performance Data\", "
echo "relating to the Software are the Confidential Information of VMware, and "
echo "will be treated in accordance with the terms of Section 4 of this "
echo "Agreement.  Accordingly, Licensee shall not publish or disclose to any "
echo "third party any Performance Data relating to the Software.  Licensee shall "
echo "immediately cease all use of the Software and Documentation, upon notice "
echo "from VMware. "
echo " "
echo "(d Ownership.  VMware shall own and retain all right, title and interest "
echo "in and to the Intellectual Property Rights in the Software, Documentation, "
echo "and any derivative works thereof, subject only to the license expressly set "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "forth in Section 2(a hereof.  Licensee does not acquire any other rights, "
echo "express or implied, in the Software or Documentation.  VMWARE RESERVES ALL "
echo "RIGHTS NOT EXPRESSLY GRANTED HEREUNDER. "
echo " "
echo "(e No Support Services.  VMware is under no obligation to support the "
echo "Software in any way or to provide any Updates to Licensee.  In the event "
echo "VMware, in its sole discretion, supplies any Update to Licensee, such "
echo "Update shall be deemed Software hereunder and shall be subject to the terms "
echo "and conditions of this Agreement. Upon VMware's release of any Update, "
echo "Licensee shall immediately cease all use of the former version of the "
echo "Software. "
echo " "
echo "(f Third-Party Software.  The Software enables a computer to run multiple "
echo "instances of third-party guest operating systems and application programs. "
echo "Licensee acknowledges that Licensee is responsible for obtaining any "
echo "licenses necessary to operate any such third-party software, including "
echo "guest operating systems. "
echo " "
echo "(g Open Source Software.  The terms and conditions of this Agreement shall "
echo "not apply to any Open Source Software accompanying the Software.  Any such "
echo "Open Source Software is provided under the terms of the open source license "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "agreement or copyright notice accompanying such Open Source Software or in "
echo "the open source licenses file accompanying the Software. "
echo " "
echo "(h Demonstration. Subject to the terms and conditions of this Agreement, "
echo "VMware grants to Licensee a non-exclusive, revocable, non-transferable "
echo "license (without the right to sublicense to use the Software for "
echo "demonstration to third parties, provided Licensee (i has received prior "
echo "written authorization from VMware for same, (ii has had such third parties "
echo "first sign a confidentiality agreement that contains nondisclosure "
echo "restrictions substantially similar to those set forth in this Agreement, "
echo "(iii includes in such confidentiality agreement an acknowledgement that "
echo "VMware does not promise or guarantee that features, functionality and/or "
echo "modules in the Software will be included in any generally available version "
echo "of the Software, or will be marketed separately for additional fees, and "
echo "(iv conducts such demonstration solely on Licensee's hardware and such "
echo "hardware remains at all times in Licensee's possession and control. "
echo " "
echo "3. TERM AND TERMINATION. This Agreement is effective as of the Effective "
echo "Date and will continue for a one (1 year period (\"Initial Term\", unless "
echo "amended to establish a later expiration date (\"Subsequent Term\" by a "
echo "written agreement signed by both parties, or until terminated as provided "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "in this Agreement.  Either party may terminate this Agreement at any time "
echo "for any reason or no reason by providing the other party advance written "
echo "notice thereof.  Upon any expiration or termination of this Agreement, the "
echo "rights and licenses granted to Licensee under this Agreement shall "
echo "immediately terminate, and Licensee shall immediately cease using, and will "
echo "return to VMware (or, at VMware's request, destroy, the Software, "
echo "Documentation and all other tangible items in Licensee's possession or "
echo "control that are proprietary to or contain Confidential Information.  The "
echo "rights and obligations of the parties set forth in Sections 1, 2(b 2(c, "
echo "2(d, 2(e, 3, 4, 5, 6 and 7 shall survive termination or expiration of "
echo "this Agreement for any reason. "
echo " "
echo "4. CONFIDENTIALITY. \"Confidential Information\" shall mean all trade "
echo "secrets, know-how, inventions, techniques, processes, algorithms, software "
echo "programs, hardware, schematics, planned product features, functionality, "
echo "methodology, performance and software source documents relating to the "
echo "Software, and other information provided by VMware, whether disclosed "
echo "orally, in writing, or by examination or inspection, other than information "
echo "which Licensee can demonstrate (i was already known to Licensee, other "
echo "than under an obligation of confidentiality, at the time of disclosure; "
echo "(ii was generally available in the public domain at the time of disclosure "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "to Licensee; (iii became generally available in the public domain after "
echo "disclosure other than through any act or omission of Licensee; (iv was "
echo "subsequently lawfully disclosed to Licensee by a third party without any "
echo "obligation of confidentiality; or (v was independently developed by "
echo "Licensee without use of or reference to any information or materials "
echo "disclosed by VMware or its suppliers.  Confidential Information shall "
echo "include without limitation the Software, Documentation, Performance Data, "
echo "any Updates, information relating to VMware products, product roadmaps, and "
echo "other technical, business, financial and product development plans, "
echo "forecasts and strategies.  Licensee shall not use any Confidential "
echo "Information for any purpose other than as expressly authorized under this "
echo "Agreement.  Except as otherwise set forth in this Agreement, in no event "
echo "shall Licensee use the Software, Documentation or any other Confidential "
echo "Information to develop, manufacture, market, sell, or distribute any "
echo "product or service.  Licensee shall limit dissemination of Confidential "
echo "Information to its employees who have a need to know such Confidential "
echo "Information for purposes expressly authorized under this Agreement.  Except "
echo "as otherwise set forth in this Agreement, in no event shall Licensee "
echo "disclose any Confidential Information to any third party. Without limiting "
echo "the foregoing, Licensee shall use at least the same degree of care that it "
echo "uses to prevent the disclosure of its own confidential information of like "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "importance, but in no event less than reasonable care, to prevent the "
echo "disclosure of Confidential Information. "
echo " "
echo "5. LIMITATION OF LIABILITY.  IT IS UNDERSTOOD THAT THE SOFTWARE, "
echo "DOCUMENTATION AND ANY UPDATES ARE PROVIDED WITHOUT CHARGE FOR THE "
echo "PURPOSES OF THIS AGREEMENT ONLY.  ACCORDINGLY, THE TOTAL "
echo "LIABILITY OF VMWARE AND ITS SUPPLIERS ARISING OUT OF OR RELATED "
echo "TO THIS AGREEMENT SHALL NOT EXCEED $100.  IN NO EVENT SHALL "
echo "VMWARE OR ITS SUPPLIERS HAVE LIABILITY FOR ANY INDIRECT, "
echo "INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES (INCLUDING, WITHOUT "
echo "LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS "
echo "INTERRUPTION, OR LOSS OF BUSINESS INFORMATION, HOWEVER CAUSED "
echo "AND ON ANY THEORY OF LIABILITY, EVEN IF VMWARE OR ITS SUPPLIERS "
echo "HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.  THESE "
echo "LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL "
echo "PURPOSE OF ANY LIMITED REMEDY. "
echo " "
echo "6. WARRANTY DISCLAIMER.  IT IS UNDERSTOOD THAT THE SOFTWARE, "
echo "DOCUMENTATION, AND ANY UPDATES MAY CONTAIN ERRORS AND ARE "
echo "PROVIDED FOR THE PURPOSES OF THIS AGREEMENT ONLY.  THE SOFTWARE, "
echo "DOCUMENTATION, AND ANY UPDATES ARE PROVIDED \"AS IS\" WITHOUT "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "WARRANTY OF ANY KIND, WHETHER EXPRESS, IMPLIED, STATUTORY, OR "
echo "OTHERWISE. VMWARE AND ITS SUPPLIERS SPECIFICALLY DISCLAIM ALL "
echo "IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND "
echo "FITNESS FOR A PARTICULAR PURPOSE.  "
echo "Licensee acknowledges that VMware has not publicly "
echo "announced the availability of the Software and/or Documentation, that "
echo "VMware has not promised or guaranteed to Licensee that such Software and/or "
echo "Documentation will be announced or made available to anyone in the future, "
echo "that VMware has no express or implied obligation to Licensee to announce or "
echo "introduce the Software and/or Documentation, and that VMware may not "
echo "introduce a product similar or compatible with the Software and/or "
echo "Documentation. Accordingly, Licensee acknowledges that any research or "
echo "development that it performs regarding the Software or any product "
echo "associated with the Software is done entirely at Licensee's own risk. "
echo "Specifically, the Software may contain features, functionality or modules "
echo "that may not be included in the generally available version of the Software "
echo "and/or Documentation, or that may be marketed separately for additional "
echo "fees. "
echo " "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "7. OTHER PROVISIONS "
echo " "
echo "(a Governing Law.  This Agreement, and all disputes arising out of or "
echo "related thereto, shall be governed by and construed under the laws of the "
echo "State of California without reference to conflict of laws principles.  All "
echo "such disputes shall be subject to the exclusive jurisdiction of the state "
echo "and federal courts located in Santa Clara County, California, and the "
echo "parties agree and submit to the personal and exclusive jurisdiction and "
echo "venue of these courts. "
echo " "
echo "(b Assignment.  Licensee shall not assign this Agreement or any rights or "
echo "obligations hereunder, directly or indirectly, by operation of law, merger, "
echo "acquisition of stock or assets, or otherwise, without the prior written "
echo "consent of VMware.  Subject to the foregoing, this Agreement shall inure to "
echo "the benefit of and be binding upon the parties and their respective "
echo "successors and permitted assigns. "
echo " "
echo "(c Export Regulations.  Licensee understands that VMware is subject to "
echo "regulation by the U.S. government and its agencies, which prohibit export "
echo "or diversion of certain technical products and information to certain "
echo "countries and individuals.  Licensee warrants that it will comply in all "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "respects with all export and re-export restrictions applicable to the "
echo "technology and documentation provided hereunder. "
echo " "
echo "(d Entire Agreement.  This is the entire agreement between the parties "
echo "relating to the subject matter hereof and all other terms are rejected. "
echo "This Agreement supersedes all previous communications, representations, "
echo "understandings and agreements, either oral or written, between the parties "
echo "with respect to said subject matter.  The terms of this Agreement supersede "
echo "any VMware end user license agreement that may accompany the Software "
echo "and/or Documentation.  No waiver or modification of this Agreement shall be "
echo "valid unless made in a writing signed by both parties.  The waiver of a "
echo "breach of any term hereof shall in no way be construed as a waiver of any "
echo "term or other breach hereof.  If any provision of this Agreement is held by "
echo "a court of competent jurisdiction to be contrary to law the remaining "
echo "provisions of this Agreement shall remain in full force and effect. "
echo " "
echo "(e Notices. All notices must be sent by (a registered or certified mail, "
echo "return receipt requested, (b reputable overnight air courier, (c "
echo "facsimile with a confirmation copy sent by registered or certified mail, "
echo "return receipt requested, or (d served personally.  Notices are effective "
echo "immediately when served personally, five (5 days after posting if sent by "

echo "--"
echo "Press Enter to Continue"
read tmp
echo "--"

echo "registered or certified mail, two (2 days after being sent by overnight "
echo "courier, or one (1 day after being transmitted by facsimile.  Notices to "
echo "either party shall be directed to the party's address set forth in this "
echo "Agreement.  Either party may change its address for notification under this "
echo "Agreement, by notifying the other party in accordance with this Section. "
echo ""

# This is pretty mean - just exiting if they're just hitting enter through the license
# but this way they will have to pay some attention at least.  If this is too mean
# switch to a while loop to check for yes.
echo "--"
echo "Have you read and understood the terms of this agreement? [yes/no]"
read understood
if [ "$understood" != "yes" ] && [ "$understood" != "Yes" ] && [ "$understood" != "YES" ]
then
    echo "You must read and understand the terms of the licensing agreement"
    echo "before installation can continue."
    exit
fi
echo "Do you accept the terms of this agreement? Type \"I Accept\" to accept."
read acceptance
if [ "$acceptance" != "I Accept" ] && [ "$acceptance" != "i accept" ] && \
   [ "$acceptance" != "I accept" ] && [ "$acceptance" != "I ACCEPT" ]
then
    echo "You must accept the licensing agreement before installation can continue."
    exit
fi

# Okay ready to install
unzip -P UId3E4c2WPSiFlOzJDQx dynamorio.zip

echo "Finished"