# Contributing to DynamoRIO

We welcome contributions to the DynamoRIO project.

If you would like to contribute code to DynamoRIO, we do not require a
formal contributor license agreement.  Contributions are implicitly assumed
to be offered under terms of [DynamoRIO's
license](https://github.com/DynamoRIO/dynamorio/blob/master/License.txt).

Our wiki contains further information on policies, how to check out the
code, and how to add new code:

- [Contribution policies and suggestions](https://dynamorio.org/page_contributing.html)
- [Git workflow](https://dynamorio.org/page_workflow.html)
- [Code style guide](https://dynamorio.org/page_code_style.html)
- [Code content guidelines](https://dynamorio.org/page_code_content.html)
- [Code reviews](https://dynamorio.org/page_code_reviews.html)

## Reporting issues

DynamoRIO is a tool platform, with end-user tools built on top of it.  If
you encounter a crash in a tool provided by a third party, please locate
the issue tracker for the tool you are using and report the crash there.

To report issues in DynamoRIO itself, please follow the guidelines below.

For the Summary, please follow the [guidelines in our
bug reporting page](https://dynamorio.org/issues) and use
one of the CRASH, APP CRASH, HANG, or ASSERT keywords.

Please fill in the body of the issue with this template:

```
What version of DynamoRIO are you using?

Does the latest build from
https://github.com/DynamoRIO/dynamorio/releases solve the problem?

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
https://dynamorio.org/issues and set the title
appropriately)?


Please provide any additional information below.
```

### Including code in issues

The text in an issue is interpreted as Markdown.  To include any kind of
raw output or code that contains Markdown symbols, place it between lines
that consist solely of three backtics:
<pre>
```
put code here
```
</pre>

### Attaching images or files to issues

Place the attachment on Google Drive or some other location and include a
link to it in the issue text.

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
