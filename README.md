# DynamoRIO

![DynamoRIO logo](http://www.burningcutlery.com/images/dynamorio/drlogo.png)

## About DynamoRIO

DynamoRIO is a runtime code manipulation system that supports code
transformations on any part of a program, while it executes. DynamoRIO
exports an interface for building dynamic tools for a wide variety of uses:
program analysis and understanding, profiling, instrumentation,
optimization, translation, etc. Unlike many dynamic tool systems, DynamoRIO
is not limited to insertion of callouts/trampolines and allows arbitrary
modifications to application instructions via a powerful IA-32/AMD64
instruction manipulation library. DynamoRIO provides efficient,
transparent, and comprehensive manipulation of unmodified applications
running on stock operating systems (Windows or Linux) and commodity IA-32
and AMD64 hardware.  Both Mac OSX and ARM support are in progress.

## Existing DynamoRIO-based tools

Tools built on DynamoRIO include:

- The memory debugging tool [Dr. Memory](http://drmemory.org)
- The multi-process online cache simulator
  [drcachesim](http://dynamorio.org/docs/page_drcachesim.html)
- The legacy processor emulator
  [drcpusim](http://dynamorio.org/docs/page_drcpusim.html)
- The "strace for Windows" tool [drstrace](http://drmemory.org/strace_for_windows.html)
- The code coverage tool [drcov](http://dynamorio.org/docs/page_drcov.html)
- The library tracing tool [drltrace](http://dynamorio.org/docs/page_drltrace.html)
- The memory tracing tool [memtrace](https://github.com/DynamoRIO/dynamorio/blob/master/api/samples/memtrace.c)
- The basic block tracing tool [bbbuf](https://github.com/DynamoRIO/dynamorio/blob/master/api/samples/bbbuf.c)
- The instruction counting tool [inscount](https://github.com/DynamoRIO/dynamorio/blob/master/api/samples/inscount.c)

## Building your own custom tools

DynamoRIO's powerful API abstracts away the details of the underlying
infrastructure and allows the tool builder to concentrate on analyzing or
modifying the application's runtime code stream.  API documentation is
included in the release package and can also be [browsed
online](http://dynamorio.org/docs/).  [Slides from our past tutorials are
also available](https://github.com/DynamoRIO/dynamorio/wiki/Downloads).

## Downloading DynamoRIO

DynamoRIO is available free of charge as a [binary package for both Windows
and Linux](https://github.com/DynamoRIO/dynamorio/wiki/Downloads).
DynamoRIO's [source code is
available](https://github.com/DynamoRIO/dynamorio) under a [BSD
license](https://github.com/DynamoRIO/dynamorio/blob/master/License.txt).

## Obtaining Help

Use the [discussion list](http://groups.google.com/group/DynamoRIO-Users)
to ask questions.

To report a bug, use the [issue
tracker](https://github.com/DynamoRIO/dynamorio/issues).

See also [the DynamoRIO home page](http://dynamorio.org/): [http://dynamorio.org/](http://dynamorio.org/)
