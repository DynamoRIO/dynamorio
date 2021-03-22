---
name: Bug report
about: Create a report about a specific problem to help us improve
title: <EDIT ME> (following https:///dynamorio.org/issues with one of the CRASH, APP CRASH, HANG, or ASSERT keywords)
labels: ''
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.  

If you just have a question, please use the users list https://groups.google.com/forum/#!forum/DynamoRIO-Users instead of this issue tracker, as it will reach a wider audience of people who might have an answer, and it will reach other users who may find the information beneficial. The issue tracker is for specific detailed bugs. If you are not sure it's a bug, please start by asking on the users list.  If you have already asked on the users list and the consensus was to file a new issue, please include the URL to the discussion thread here.

DynamoRIO is a tool platform, with end-user tools built on top of it. If you encounter a crash in a tool provided by a third party, please locate the issue tracker for the tool you are using and report the crash there.

**To Reproduce**
Steps to reproduce the behavior:
1. Pointer to a minimized application (ideally the source code for it and instructions on which toolchain it was built with).
2. Precise command line for running the application.
3. Exact output or incorrect behavior.

Please also answer these questions:
 - What happens when you run without any client?
 - What happens when you run with debug build ("-debug" flag to drrun/drconfig/drinject)?

**Expected behavior**
A clear and concise description of what you expected to happen.

**Screenshots or Pasted Text**
If applicable, add screenshots to help explain your problem.  For text, please cut and paste the text here, delimited by lines consisting of three backtics to render it verbatim, like this:
<pre>
```
paste output here
```
</pre>

**Versions**
 - What version of DynamoRIO are you using?
 - Does the latest build from https://github.com/DynamoRIO/dynamorio/releases solve the problem?
- What operating system version are you running on? ("Windows 10" is *not* sufficient: give the release number.)
- Is your application 32-bit or 64-bit?

**Additional context**
Add any other context about the problem here.
