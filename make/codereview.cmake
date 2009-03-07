# **********************************************************
# Copyright (c) 2009 VMware, Inc.    All rights reserved.
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
#   with a commit log that auto-opens an Issue
# Inline comments for a regular patch diff should be the norm for now
# FIXME: we should also produce web diffs for quick reviews
# FIXME: xref discussion on using Google Code's post-commit review options
#
# Usage: prepare a notes file with comments for the reviewer after a line
# beginning "toreview:", and point to it w/ the NOTES variable
# (defult is ./diff.notes)
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
#   LABEL:STRING    = Name for review.  Should follow "i###-descr" format where
#                     ### is the Issue number.  For example: "i64-cmake-review".
#   AUTHOR:STRING   = Username on https://dynamorio.googlecode.com/svn
#   REVIEWER:STRING = Username on https://dynamorio.googlecode.com/svn
#   REVERT:BOOL     = Set to ON to revert locally-added files
#   COMMIT:BOOL     = Set to ON to officially commit the review request
#                     (the default is to simply create and locally add the
#                     .diff and .notes files)
#
# Examples:
#
# Dry run to ensure diff and notes file are as desired:
#   > cmake -DAUTHOR:STRING=derek.bruening -DREVIEWER:STRING=qin.zhao -DLABEL:STRING=i64-cmake-review -P make/codereview.cmake
#   -- notes file is "diff.notes"
#   -- destination is "../reviews/derek.bruening/2009/i64-cmake-review.{diff,notes}"
#   -- svn: A         ../reviews/derek.bruening/2009/i64-cmake-review.diff
#   -- svn: A         ../reviews/derek.bruening/2009/i64-cmake-review.notes
#   -- ready to commit
#   
# Want to abort (maybe decided to change LABEL) so undoing local svn add:
#   > cmake -DAUTHOR:STRING=derek.bruening -DREVIEWER:STRING=qin.zhao -DLABEL:STRING=i64-cmake-review -DREVERT:BOOL=ON -P make/codereview.cmake
#   -- notes file is "diff.notes"
#   -- destination is "../reviews/derek.bruening/2009/i64-cmake-review.{diff,notes}"
#   -- svn: D         ../reviews/derek.bruening/2009/i64-cmake-review.diff
#   -- svn: D         ../reviews/derek.bruening/2009/i64-cmake-review.notes
#   -- revert complete
#
# Ready to commit:
#   > cmake -DAUTHOR:STRING=derek.bruening -DREVIEWER:STRING=qin.zhao -DLABEL:STRING=i64-cmake-review -DCOMMIT:BOOL=ON -P make/codereview.cmake
#   -- notes file is "diff.notes"
#   -- destination is "../reviews/derek.bruening/2009/i64-cmake-review.{diff,notes}"
#   -- svn: A         ../reviews/derek.bruening/2009/i64-cmake-review.diff
#   -- svn: A         ../reviews/derek.bruening/2009/i64-cmake-review.notes
#   -- svn: Sending        derek.bruening/2009/i64-cmake-review.diff
#   -- svn: Sending        derek.bruening/2009/i64-cmake-review.notes
#   -- svn: Transmitting file data .
#   -- svn: Committed revision 75.
#   -- committed
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

find_program(SVN svn DOC "subversion client")
if (SVN-NOTFOUND OR NOT EXISTS "${SVN}")
  message(FATAL_ERROR "svn not found")
endif (SVN-NOTFOUND OR NOT EXISTS "${SVN}")
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
if (DIFFSTAT-NOTFOUND OR NOT EXISTS "${DIFFSTAT}")
  message(STATUS "diffstat not found: will not have diff statistics")
endif (DIFFSTAT-NOTFOUND OR NOT EXISTS "${DIFFSTAT}")

# get the year
if (UNIX)
  find_program(DATE date DOC "unix date")
  if (DATE-NOTFOUND OR NOT EXISTS "${DATE}")
    message(FATAL_ERROR "date not found")
  endif (DATE-NOTFOUND OR NOT EXISTS "${DATE}")
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
  if (CMD-NOTFOUND OR NOT EXISTS "${CMD}")
    message(FATAL_ERROR "cmd not found")
  endif (CMD-NOTFOUND OR NOT EXISTS "${CMD}")
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

set(DEST_BASE "${REVIEWS}/${AUTHOR}/${year}")
set(DEST "${DEST_BASE}/${LABEL}")
message(STATUS "destination is \"${DEST}.{diff,notes}\"")

if (REVERT)

  execute_process(COMMAND ${SVN} status ${DEST}.diff
    RESULT_VARIABLE svn_result 
    ERROR_VARIABLE svn_err
    OUTPUT_VARIABLE svn_out)
  if (svn_result OR svn_err)
    message(FATAL_ERROR "*** ${SVN} status failed: ***\n${svn_err}")
  endif (svn_result OR svn_err)
  if ("${svn_out}" MATCHES "^\\A")
    # svn revert doesn't delete local copy if new => svn delete
    # FIXME: if AUTHOR was mistyped should we remove the directories too?
    run_svn("delete;--force;${DEST}.diff;${DEST}.notes")
  else ("${svn_out}" MATCHES "^\\A")
    run_svn("revert;${DEST}.diff;${DEST}.notes")
  endif ("${svn_out}" MATCHES "^\\A")

  message(STATUS "revert complete")

else (REVERT)

  if (NOT EXISTS "${DEST_BASE}")
    if (NOT EXISTS "${REVIEWS}/${AUTHOR}")
      run_svn("mkdir;${REVIEWS}/${AUTHOR}")
    endif (NOT EXISTS "${REVIEWS}/${AUTHOR}")
    run_svn("mkdir;${DEST_BASE}")
  endif (NOT EXISTS "${DEST_BASE}")

  # If someone manually created these files then this script
  # will fail: we assume not already there if haven't been
  # added to svn yet.
  if (NOT EXISTS "${DEST}.diff")
    file(WRITE ${DEST}.diff "")
    run_svn("add;${DEST}.diff")
  endif (NOT EXISTS "${DEST}.diff")
  if (NOT EXISTS "${DEST}.notes")
    file(WRITE ${DEST}.notes "")
    run_svn("add;${DEST}.notes")
  endif (NOT EXISTS "${DEST}.notes")

  # We want context diffs with procedure names for better readability
  # svn diff does show new files for us but we pass -N just in case
  execute_process(COMMAND ${SVN} diff --diff-cmd diff -x "-c -p -N"
    RESULT_VARIABLE svn_result 
    ERROR_VARIABLE svn_err
    OUTPUT_FILE ${DEST}.diff)
  if (svn_result OR svn_err)
    message(FATAL_ERROR "*** ${SVN} diff failed: ***\n${svn_err}")
  endif (svn_result OR svn_err)

  file(READ ${NOTES} string)
  string(REGEX REPLACE "^.*\ntoreview:\r?\n" "" pubnotes "${string}")
  if ("${string}" STREQUAL "${pubnotes}")
    message(FATAL_ERROR "${NOTES} is missing \"toreview:\" marker")
  endif ("${string}" STREQUAL "${pubnotes}")

  # Do "wc -l" ourselves
  file(READ ${DEST}.diff string)
  string(REGEX MATCHALL "\n" newlines "${string}")
  list(LENGTH newlines lines )

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
  string(REGEX MATCH " *(if|while) *[^{\n]*\r?\n[^;\n]*\r?\n" bad "${string}")
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
  string(REGEX MATCH "//" bad "${string}")
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

  # We commit the diff to review/ using special syntax to auto-generate
  # an Issue that covers performing the review
  file(WRITE ${DEST}.notes
    "new review:\nowner: ${REVIEWER}\nsummary: ${AUTHOR}/${year}/${LABEL}.diff\n")
  file(APPEND ${DEST}.notes "${pubnotes}")
  file(APPEND ${DEST}.notes "\nstats: ${lines} diff lines\n")
  if (EXISTS "${DIFFSTAT}")
    execute_process(COMMAND ${DIFFSTAT} ${DEST}.diff
      ERROR_QUIET OUTPUT_VARIABLE diffstat_out)
    file(APPEND ${DEST}.notes "${diffstat_out}")
  endif (EXISTS "${DIFFSTAT}")
  message(STATUS "ready to commit")

  if (COMMIT)
    run_svn("commit;--force-log;-F;${DEST}.notes")
    message(STATUS "committed")
  endif (COMMIT)

endif (REVERT)
