# Contributing to DynamoRIO

We welcome contributions to the DynamoRIO project.

If you would like to contribute code to DynamoRIO, you will need to first sign a
[Contributor License Agreement](https://developers.google.com/open-source/cla/individual).

Our wiki contains further information on policies, how to check out the
code, and how to add new code:

- [Contribution policies and suggestions](https://github.com/DynamoRIO/dynamorio/wiki/Contributing)
- [Git workflow](https://github.com/DynamoRIO/dynamorio/wiki/Workflow)
- [Code style guide](https://github.com/DynamoRIO/dynamorio/wiki/Code-Style)
- [Code content guidelines](https://github.com/DynamoRIO/dynamorio/wiki/Code-Content)
- [Code reviews](https://github.com/DynamoRIO/dynamorio/wiki/Code-Reviews)

## Reporting issues

DynamoRIO is a tool platform, with end-user tools built on top of it.  If
you encounter a crash in a tool provided by a third party, please locate
the issue tracker for the tool you are using and report the crash there.

To report issues in DynamoRIO itself, please follow the guidelines below.

For the Summary, please follow the [guidelines in our
wiki](https://github.com/DynamoRIO/dynamorio/wiki/Bug-Reporting) and use
one of the CRASH, APP CRASH, HANG, or ASSERT keywords.

Please fill in the body of the issue with this template:

```
What version of DynamoRIO are you using?

Does the latest build from
http://build.chromium.org/p/client.dynamorio/builds/ solve the problem?

What operating system version are you running on?

What application are you running?

Is your application 32-bit or 64-bit?

How are you running the application under DynamoRIO?

What happens when you run without any client?

What happens when you run with debug build ("-debug" flag to
drrun/drconfig/drinject)?

What steps will reproduce the problem?
1.
2.
3.

What is the expected output? What do you see instead?  Is this an
application crash, a DynamoRIO crash, a DynamoRIO assert, or a hang (see
https://github.com/DynamoRIO/dynamorio/wiki/Bug-Reporting and set the title
appropriately)?


Please provide any additional information below.
```

## Filing feature requests

Before filing a feature request, check the documentation to ensure it is
not already provided by the existing interface.

Please provide the following information in an issue filed as a feature
request:

```
Describe the desired functionality and its intended usage to give some
context for how it would be used.

Is there currently a method for implementing this functionality using the
existing API and this requested feature will add convenience?  Or is there
no method for accomplishing the desired task using the current API?

Do you have any implementation in mind for this feature?
```
