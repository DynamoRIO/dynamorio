# **********************************************************
# Copyright (c) 2010 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
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

###########################################################################
# Code Review Automation
#
# See CodeReviews.wiki for full discussion of our code review process.
# To summarize: reviewer commits to /reviews/<user>/<year>/i###-descr.diff
#   and uploads a diff to Rietveld
# Inline comments for a regular patch diff should be the norm for now
#
# Usage: prepare a notes file with comments for the reviewer after a
# line beginning "toreview:" (and optionally ending at a line
# beginning ":endreview", or the end of the file), and point to it w/
# the NOTES variable (defult is ./diff.notes)
#
# Since we no longer invoke "make" from the soure dir (due to cmake: i#19, i#64),
# we have to invoke this explicitly "cmake -P make/codereview.cmake".
#
# Parameter list:
#
#   REVIEWS:PATH    = Checkout of https://dynamorio.googlecode.com/svn/reviews
#   NOTES:FILEPATH  = File containing notes for this review.
#                     It must contain the string label "toreview:".
#                     Content prior to the label is discarded (private notes).
#                     Content after the label is included in the review request.
#                     If a line begins ":endreview" it will end the notes.
#   LABEL:STRING    = Name for review.  Should follow "i###-descr" format where
#                     ### is the Issue number.  For example: "i64-cmake-review".
#   AUTHOR:STRING   = Username on https://dynamorio.googlecode.com/svn
#   REVIEWER:STRING = Username on https://dynamorio.googlecode.com/svn
#   REVERT:BOOL     = Set to ON to revert locally-added files
#   COMMIT:BOOL     = Set to ON to officially commit the review request
#                     (the default is to simply create and locally add the
#                     .diff file)
#   DIFFARGS:STRING = Arguments to pass to svn or git diff
#   DR_ISSUE:BOOL   = Set to ON to create a DynamoRIO Issue to cover the review
#                     task.  This is no longer recommended as we use Rietveld
#                     and don't want to pollute the DR issue tracker.
#   CR_ISSUE:STRING = Gives the codereview.appspot.com issue number to use, if
#                     the diff was uploaded manually and thus the number is not
#                     stored at the top of the diff itself.
#
# Examples:
#
# Dry run to ensure diff and notes file are as desired:
#   > cmake -DAUTHOR:STRING=derek.bruening -DREVIEWER=qin.zhao -DLABEL=i64-cmake-review -P make/codereview.cmake
#   -- notes file is "diff.notes"
#   -- destination is "../reviews/derek.bruening/2009/i64-cmake-review.diff"
#   -- svn: A         ../reviews/derek.bruening/2009/i64-cmake-review.diff
#   -- ready to commit
#
# Want to abort (maybe decided to change LABEL) so undoing local svn add:
#   > cmake -DAUTHOR=derek.bruening -DREVIEWER=qin.zhao -DLABEL=i64-cmake-review -DREVERT=ON -P make/codereview.cmake
#   -- notes file is "diff.notes"
#   -- destination is "../reviews/derek.bruening/2009/i64-cmake-review.diff"
#   -- svn: D         ../reviews/derek.bruening/2009/i64-cmake-review.diff
#   -- revert complete
#
# Ready to commit:
#   > cmake -DAUTHOR=derek.bruening -DREVIEWER=qin.zhao -DLABEL=i64-cmake-review -DCOMMIT=ON -P make/codereview.cmake
#   -- notes file is "diff.notes"
#   -- destination is "../reviews/derek.bruening/2009/i64-cmake-review.diff"
#   -- svn: A         ../reviews/derek.bruening/2009/i64-cmake-review.diff
#   -- Issue is uploaded to: http://codereview.appspot.com/2419041
#   -- svn: Sending        derek.bruening/2009/i64-cmake-review.diff
#   -- svn: Transmitting file data .
#   -- svn: Committed revision 75.
#   -- committed
#
# From a git-svn checkout you need to specify the "git diff" args.
# For example:
#   > cmake -DAUTHOR=derek.bruening -DREVIEWER=qin.zhao -DLABEL=i64-cmake-review -DCOMMIT=ON -DDIFFARGS=master.. -P make/codereview.cmake
#  For multi-word args, separate with \; (do not quote and use spaces):
#   > cmake -DAUTHOR=derek.bruening -DREVIEWER=qin.zhao -DLABEL=i64-cmake-review -DCOMMIT=ON -DDIFFARGS=8373a5e95043d4096bf32047d6886ac6fd0678cb\;5a9b096e296a5e3066d7429c313e6094749c6595 -P make/codereview.cmake
#
# Note that you can make a cmake script that sets common variables and point at it
# with the -C parameter to cmake.

# FIXME i#66: should include pre-commit regression test suite results here

if (NOT DEFINED LABEL)
  message(FATAL_ERROR "must set LABEL variable")
endif (NOT DEFINED LABEL)

if (NOT DEFINED AUTHOR)
  message(FATAL_ERROR "must set AUTHOR variable")
endif (NOT DEFINED AUTHOR)

if (NOT DEFINED REVIEWER)
  message(FATAL_ERROR "must set REVIEWER variable")
endif (NOT DEFINED REVIEWER)

if (NOT DEFINED NOTES)
  set(NOTES diff.notes)
endif (NOT DEFINED NOTES)
if (NOT EXISTS ${NOTES})
  message(FATAL_ERROR "cannot find NOTES=\"${NOTES}\"")
endif (NOT EXISTS ${NOTES})
message(STATUS "notes file is \"${NOTES}\"")

if (NOT DEFINED REVIEWS)
  set(REVIEWS ../reviews)
endif (NOT DEFINED REVIEWS)
if (NOT EXISTS ${REVIEWS})
  message(FATAL_ERROR "cannot find REVIEWS=\"${REVIEWS}\"")
endif (NOT EXISTS ${REVIEWS})

# Unfortunately there's no way to get the dir this script is in so we
# assume we're in top level of repository
if (NOT EXISTS "make/upload.py")
  message(FATAL_ERROR "cannot find make/upload.py")
endif ()

find_program(PYTHON python DOC "subversion client")
if (NOT PYTHON)
  message(FATAL_ERROR "python not found")
endif (NOT PYTHON)

find_program(SVN svn DOC "subversion client")
if (NOT SVN)
  message(FATAL_ERROR "svn not found")
endif (NOT SVN)
# we run svn in the review checkout
function(run_svn)
  execute_process(COMMAND ${SVN} ${ARGV}
    WORKING_DIRECTORY ${REVIEWS}
    RESULT_VARIABLE svn_result
    ERROR_VARIABLE svn_err
    OUTPUT_VARIABLE svn_out)
  if (svn_result OR svn_err)
    message(FATAL_ERROR "*** ${SVN} ${ARGV} failed: ***\n${svn_err}")
  endif (svn_result OR svn_err)
  string(STRIP "${svn_out}" svn_out)
  string(REGEX REPLACE "\r?\n" "\n-- svn: " svn_out "${svn_out}")
  message(STATUS "svn: ${svn_out}")
endfunction(run_svn)

find_program(DIFFSTAT diffstat DOC "diff statistics displayer")
if (NOT DIFFSTAT)
  message(STATUS "diffstat not found: will not have diff statistics")
endif (NOT DIFFSTAT)

# get the year
if (UNIX)
  find_program(DATE date DOC "unix date")
  if (NOT DATE)
    message(FATAL_ERROR "date not found")
  endif (NOT DATE)
  execute_process(COMMAND ${DATE} +%Y
    RESULT_VARIABLE date_result
    ERROR_VARIABLE date_err
    OUTPUT_VARIABLE year)
  if (date_result OR date_err)
    message(FATAL_ERROR "*** ${DATE} failed: ***\n${date_err}")
  endif (date_result OR date_err)
  string(STRIP "${year}" year)
else (UNIX)
  find_program(CMD cmd DOC "cmd shell")
  if (NOT CMD)
    message(FATAL_ERROR "cmd not found")
  endif (NOT CMD)
  # If use forward slashes => "The syntax of the command is incorrect."
  file(TO_NATIVE_PATH "${CMD}" CMD)
  execute_process(COMMAND ${CMD} /c date /T
    RESULT_VARIABLE date_result
    ERROR_VARIABLE date_err
    OUTPUT_VARIABLE date_out)
  if (date_result OR date_err)
    message(FATAL_ERROR "*** ${CMD} /c date /T failed: ***\n${date_err}")
  endif (date_result OR date_err)
  string(STRIP "${date_out}" date_out)
  # result should be like this: "Fri 03/06/2009"
  # cmake regexp doesn't have {N}
  string(REGEX REPLACE "^....../../([0-9][0-9][0-9][0-9])" "\\1" year "${date_out}")
endif (UNIX)

# we run from the REVIEWS dir so no need to include it here
set(DEST_BASE "${AUTHOR}/${year}")
set(DEST "${DEST_BASE}/${LABEL}")
message(STATUS "destination is \"${REVIEWS}/${DEST}.diff\"")

if (REVERT)

  execute_process(COMMAND ${SVN} status ${DEST}.diff
    WORKING_DIRECTORY ${REVIEWS}
    RESULT_VARIABLE svn_result
    ERROR_VARIABLE svn_err
    OUTPUT_VARIABLE svn_out)
  if (svn_result OR svn_err)
    message(FATAL_ERROR "*** ${SVN} status failed: ***\n${svn_err}")
  endif (svn_result OR svn_err)
  if ("${svn_out}" MATCHES "^\\A")
    # svn revert doesn't delete local copy if new => svn delete
    # FIXME: if AUTHOR was mistyped should we remove the directories too?
    run_svn("delete;--force;${DEST}.diff")
  else ("${svn_out}" MATCHES "^\\A")
    run_svn("revert;${DEST}.diff")
  endif ("${svn_out}" MATCHES "^\\A")

  message(STATUS "revert complete")

else (REVERT)

  if (NOT EXISTS "${REVIEWS}/${DEST_BASE}")
    if (NOT EXISTS "${REVIEWS}/${AUTHOR}")
      run_svn("mkdir;${AUTHOR}")
    endif (NOT EXISTS "${REVIEWS}/${AUTHOR}")
    run_svn("mkdir;${DEST_BASE}")
  endif (NOT EXISTS "${REVIEWS}/${DEST_BASE}")

  set(DIFF_LOCAL "${DEST}.diff")
  set(DIFF_FILE "${REVIEWS}/${DIFF_LOCAL}")

  # If someone manually created these files then this script
  # will fail: we assume not already there if haven't been
  # added to svn yet.
  if (CR_ISSUE)
    set(cur_issue -i ${CR_ISSUE})
  else (CR_ISSUE)
    set(cur_issue )
  endif (CR_ISSUE)
  if (NOT EXISTS "${DIFF_FILE}")
    file(WRITE "${DIFF_FILE}" "")
    run_svn("add;${DIFF_LOCAL}")
  else (NOT EXISTS "${DIFF_FILE}")
    if (NOT CR_ISSUE)
      # Get Rietveld issue #
      file(READ "${DIFF_FILE}" curdiff)
      string(REGEX MATCH "http://codereview.appspot.com/[0-9]*" issue "${curdiff}")
      string(REGEX REPLACE "http://codereview.appspot.com/" "" inum "${issue}")
      if (NOT "${inum}" STREQUAL "")
        set(cur_issue -i ${inum})
      endif (NOT "${inum}" STREQUAL "")
    endif (NOT CR_ISSUE)
  endif (NOT EXISTS "${DIFF_FILE}")
  if ("${inum}" STREQUAL "")
    set(issue "Not auto-uploaded to codereview.appspot.com")
  endif ("${inum}" STREQUAL "")

  file(READ "${NOTES}" string)
  string(REGEX REPLACE "^.*\ntoreview:\r?\n" "" pubnotes "${string}")
  string(REGEX REPLACE "\n:endreview\r?\n.*" "\n" pubnotes "${pubnotes}")
  string(REGEX REPLACE "^toreview:\r?\n" "" pubnotes "${pubnotes}")
  if ("${string}" STREQUAL "${pubnotes}")
    message(FATAL_ERROR "${NOTES} is missing \"toreview:\" marker")
  endif ("${string}" STREQUAL "${pubnotes}")
  string(REGEX MATCH "^[^\n]*" first_line "${pubnotes}")

  # Create the diff
  # Support git-svn
  if (EXISTS ".git")
    find_program(GIT git DOC "git client")
    if (NOT GIT)
      message(FATAL_ERROR "git not found")
    endif (NOT GIT)
    execute_process(COMMAND ${GIT} diff ${DIFFARGS}
      RESULT_VARIABLE svn_result
      ERROR_VARIABLE svn_err
      OUTPUT_FILE "${DIFF_FILE}")
    if (svn_result OR svn_err)
      message(FATAL_ERROR "*** ${GIT} diff ${DIFFARGS} failed: ***\n${svn_err}")
    endif (svn_result OR svn_err)
  else (EXISTS ".git")
    # We want context diffs with procedure names for better readability
    # svn diff does show new files for us but we pass -N just in case
    execute_process(COMMAND ${SVN} diff --diff-cmd diff -x "-c -p -N" ${DIFFARGS}
      RESULT_VARIABLE svn_result
      ERROR_VARIABLE svn_err
      OUTPUT_FILE "${DIFF_FILE}")
    if (svn_result OR svn_err)
      message(FATAL_ERROR "*** ${SVN} diff ${DIFFARGS} failed: ***\n${svn_err}")
    endif (svn_result OR svn_err)
  endif (EXISTS ".git")

  # Read into string var
  file(READ "${DIFF_FILE}" string)

  ##################################################
  # i#78: coding style checks
  #
  # Since these are on the diff, not the code, remember that there
  # are two extra columns at the start of the line!
  set(ugly "*** STYLE VIOLATION:")

  # the old "make ugly" rules
  # to make the regexps simpler I'm assuming no non-alphanums run up
  # against keywords: assuming spaces delimit
  string(REGEX MATCH "(TRACELOG\\([^l]|NOCHECKIN)" bad "${string}")
  if (bad)
    message("${ugly} remove debugging facilities: \"${bad}\"")
  endif (bad)
  string(REGEX MATCH "DO_ONCE\\( *{? *SYSLOG_INTERNAL" bad "${string}")
  if (bad)
    message("${ugly} use SYSLOG_INTERNAL_*_ONCE, not DO_ONCE(SYSLOG_: \"${bad}\"")
  endif (bad)
  # note that ASSERT_NOT_TESTED is already DO_ONCE so shouldn't see DO_ONCE on any ASSERT
  string(REGEX MATCH "ONCE\\( *{? *ASSERT" bad "${string}")
  if (bad)
    message("${ugly} use ASSERT_CURIOSITY_ONCE, not DO_ONCE(ASSERT: \"${bad}\"")
  endif (bad)
  string(REGEX MATCH "\n[^-\\*][^\n]*\t" bad "${string}")
  if (bad)
    message("${ugly} clean TABs w/ M-x untabify (cat -T | grep \"\^I\" to see)")
  endif (bad)
  string(REGEX MATCH " *(if|while) +[^{\n]*\r?\n[^;\n]*\r?\n" bad "${string}")
  if (bad)
    message("${ugly} use {} for multi-line body: \"${bad}\"")
  endif (bad)
  string(REGEX MATCH " if *\\([^\n]*;" bad "${string}")
  if (bad)
    message("${ugly} put if body on separate line: \"${bad}\"")
  endif (bad)
  # loop bodies
  string(REGEX MATCH "\n[^}]* while *\\([^\n]*;" bad "${string}")
  if (bad)
    message("${ugly} put loop body on separate line: \"${bad}\"")
  endif (bad)
  string(REGEX MATCH "\n[^ ]+} *while *\\([^\n]*;" bad "${string}")
  if (bad)
    message("${ugly} put loop body on separate line: \"${bad}\"")
  endif (bad)
  string(REGEX MATCH " do *{[^\n]*;" bad "${string}")
  if (bad)
    message("${ugly} put loop body on separate line: \"${bad}\"")
  endif (bad)
  string(REGEX MATCH " for *([^\n]*;[^\n]*;[^\n]*)[^\n]*;" bad "${string}")
  if (bad)
    message("${ugly} put loop body on separate line: \"${bad}\"")
  endif (bad)
  string(REGEX MATCH "\\( *([0-9]+|NULL) *== *[^0-9]" bad "${string}")
  if (bad)
    message("${ugly} (1 == var) vs. (var == 1): \"${bad}\"")
  endif (bad)
  # check that empty param list is declared void
  # FIXME: should we only require for decls by only running on header files?
  # But we do want to catch static decls.
  string(REGEX MATCH "\n.. *[A-Za-z0-9][A-Za-z0-9_\\* ]+\\(\\);" bad "${string}")
  if (bad)
    message("${ugly} empty param list should be (void) not (): \"${bad}\"")
  endif (bad)

  # the old "make pretty_ugly"
  # since we're only checking the diff and not the existing core we
  # go ahead and apply all the rules
  # Rules that should pass (and almost do, all have only a few violations left and
  # no false positives). FIXME - should be cleaned up and moved to ugly xref 8806.
  string(REGEX MATCH " (do|if|while|for|else|switch)\\(" bad "${string}")
  if (bad)
    message("${ugly} spacing, if( or if  ( vs if (: \"${bad}\"")
  endif (bad)
  string(REGEX MATCH "[}\\);](  +)?(do|if|while|for|else|switch)[ \n]" bad "${string}")
  if (bad)
    message("${ugly} spacing, }else or }  else vs } else: \"${bad}\"")
  endif (bad)

  # The old "really_ugly": see comments above.
  # Rules that don't pass at all, either because of excessive violations in the
  # codebase or because of false positives. FIXME - for rules with few or no false
  # positives but numerous violations we could use a count or something to try and
  # keep things from getting worse xref case 8806.

  # no false positives, but numerous violations
  # that's right, cmake regexp has no {N} so we do it the hard way
  string(REGEX MATCH "\n[^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n][^\n]" bad "${string}")
  if (bad)
    message("${ugly} line is too long: \"${bad}\"")
  endif (bad)

  # FIXME: not bothering to try and catch multiple statements on one line
  # at this time: look at old core/Makefile if interested.  Keeping comment below
  # for all the issues:
  #
  # With all the exemptions very few false positives, but still many violations
  # (and prob. some false negatives). The first grep looks for lines with two ; not
  # within quotes. 2nd, 3rd and 4th greps subtract out lines where the extra
  # semicolon is prob. part of a comment. NOTE, for some reason   doesn't
  # work for matching before the * in the 3rd grep. 5th grep subtracts out for (;;)
  # loop usage and 6th grep subtracts out the very common 'case: foo; break;' motif
  # (even though it technically breaks the rule). Final grep subtracts out certain
  # common macros whose usage often breaks the rule.

  # only a few false positives, but numerous violations
  string(REGEX MATCH "[A-Za-z0-9]\\*[^A-Za-z0-9/\\)\\(]" bad "${string}")
  if (bad)
    message("${ugly} instr_t* foo vs. instr_t *foo, etc.: \"${bad}\"")
  endif (bad)
  # only a few false positives, but numerous violations (even allowing for usage
  # inside /* */ comments)
  string(REGEX MATCH "[^:]//" bad "${string}") # rule out http://
  if (bad)
    message("${ugly} use C style comments: \"${bad}\"")
  endif (bad)
  # mostly false positives (we're too grammatical ;) due to using ; in comments
  string(REGEX MATCH "/\\*[^\\*].*;[^\\*]\\*/" bad "${string}")
  if (bad)
    message("${ugly} no commented-out code: \"${bad}\"")
  endif (bad)
  # some false positives and numerous violations
  string(REGEX MATCH "\n[^\\*\n]*long " bad "${string}")
  if (bad)
    message("${ugly} use int/uint instead of long/ulong: \"${bad}\"")
  endif (bad)
  # Only a few false positives and not that many violations (for the "( " rule at
  # least). FIXME - these could probably be moved to pretty_ugly at some point.
  string(REGEX MATCH "\\( +[^\\ ]+" bad "${string}")
  if (bad)
    message("${ugly} spacing, ( foo, bar ) vs. (foo, bar): \"${bad}\"")
  endif (bad)
  string(REGEX MATCH " \\)" bad "${string}")
  if (bad)
    message("${ugly} spacing, ( foo, bar ) vs. (foo, bar): \"${bad}\"")
  endif (bad)
  # no real false positives, but tons of violations (even ignoring asm listings)
  string(REGEX MATCH "\n[^\\*-][^\n]*,[^ \r\n]" bad "${string}")
  if (bad)
    message("${ugly} put spaces after commas: \"${bad}\"")
  endif (bad)

  #
  ##################################################

  # Do "wc -l" ourselves
  string(REGEX MATCHALL "\n" newlines "${string}")
  list(LENGTH newlines lines )

  if (DIFFSTAT)
    execute_process(COMMAND ${DIFFSTAT} "${DIFF_FILE}"
      ERROR_QUIET OUTPUT_VARIABLE diffstat_out)
  endif (DIFFSTAT)

  # We used to create a separate .notes file but it's simpler to
  # put the notes at the top of the .diff file
  if (DR_ISSUE)
    # We commit to review/ using special syntax
    # to auto-generate an Issue that covers performing the review
    file(WRITE "${DIFF_FILE}"
      "new review:\nowner: ${REVIEWER}\nsummary: ${AUTHOR}/${year}/${LABEL}.diff\n")
    file(APPEND "${DIFF_FILE}" "${pubnotes}")
  else (DR_ISSUE)
    file(WRITE "${DIFF_FILE}" "DynamoRIO code review: ${AUTHOR}/${year}/${LABEL}.diff\n")
    file(APPEND "${DIFF_FILE}" "Reviewer: ${REVIEWER}\n\n")
    file(APPEND "${DIFF_FILE}" "Description:\n${pubnotes}")
  endif (DR_ISSUE)

 if (COMMIT)
    # Upload to Rietveld with notes as description
    # Unfortunately there's no way in CMake to execute upload.py when it
    # asks for a password (it needs control over the terminal to do that)
    # so we only automate when cookies are in place.
    set(home "$ENV{HOME}")
    if (WIN32 AND "${home}" STREQUAL "")
      set(home "$ENV{USERPROFILE}")
    endif ()
    set(cookie_file "${home}/.codereview_upload_cookies")
    set(upload_cmd "${PYTHON}" "make/upload.py" -y
      -e "${AUTHOR}" -r "${REVIEWER}" -m "${first_line}" -f "${DIFF_FILE}"
      --send_mail --cc=dynamorio-devs@googlegroups.com
      ${cur_issue} ${DIFFARGS})
    # list to string
    foreach(arg ${upload_cmd})
      if (arg MATCHES " ")
        set(upload_cmd_str "${upload_cmd_str} \"${arg}\"")
      else (arg MATCHES " ")
        set(upload_cmd_str "${upload_cmd_str} ${arg}")
      endif (arg MATCHES " ")
    endforeach(arg ${upload_cmd})
    if (NOT EXISTS "${cookie_file}")
      message(STATUS "It looks like upload.py does not have cookies in place and will prompt you for a password.  To do so you must run it separately (and then use -DCR_ISSUE=<issue#> arg with future invocations of this script in order to update the same issue).\nPlease run this command, but copy the header only\nof ${DIFF_FILE} to a new temp file and point -f at it:\n\t${upload_cmd_str}")
    else (NOT EXISTS "${cookie_file}")
      message(STATUS "running ${upload_cmd_str}")
      execute_process(COMMAND ${upload_cmd}
        RESULT_VARIABLE cmd_result
        OUTPUT_VARIABLE cmd_out
        ERROR_VARIABLE cmd_err)
      if (cmd_result OR cmd_err)
        message(FATAL_ERROR "*** upload.py failed: ***\n${cmd_out}\n${cmd_err}.")
      endif (cmd_result OR cmd_err)
      string(REGEX MATCHALL "http://codereview.appspot.com/[0-9]*" issue "${cmd_out}")
      message(STATUS "Issue is uploaded to: ${issue}")
    endif (NOT EXISTS "${cookie_file}")
  endif (COMMIT)
  # Save issue for next time
  file(APPEND "${DIFF_FILE}" "\n${issue}\n")

  # Append stats
  file(APPEND "${DIFF_FILE}" "\nstats: ${lines} diff lines\n")
  if (DIFFSTAT)
    file(APPEND "${DIFF_FILE}" "${diffstat_out}")
  endif (DIFFSTAT)
  # Append diff proper
  file(APPEND "${DIFF_FILE}" "\n${string}")

  message(STATUS "ready to commit to ${REVIEWS}")
  if (COMMIT)
    run_svn("commit;-m;${first_line}")
    message(STATUS "committed")
  endif (COMMIT)

endif (REVERT)
