# DynamoRIO

![DynamoRIO logo](http://www.burningcutlery.com/images/dynamorio/drlogo.png?)

## About DynamoRIO

DynamoRIO is a runtime code manipulation system that supports code
transformations on any part of a program, while it executes. DynamoRIO
exports an interface for building dynamic tools for a wide variety of uses:
program analysis and understanding, profiling, instrumentation,
optimization, translation, etc. Unlike many dynamic tool systems, DynamoRIO
is not limited to insertion of callouts/trampolines and allows arbitrary
modifications to application instructions via a powerful IA-32/AMD64/ARM/AArch64
instruction manipulation library. DynamoRIO provides efficient,
transparent, and comprehensive manipulation of unmodified applications
running on stock operating systems (Windows, Linux, or Android) and commodity
IA-32, AMD64, ARM, and AArch64 hardware.  Mac OSX support is in progress.

## Existing DynamoRIO-based tools

DynamoRIO is the basis for some well-known external tools:
- The [Arm Instruction Emulator (ArmIE)](https://developer.arm.com/tools-and-software/server-and-hpc/arm-architecture-tools/arm-instruction-emulator)
- [WinAFL](https://github.com/googleprojectzero/winafl), the Windows fuzzing tool, as an instrumentation and code coverage engine
- The fine-grained profiler for ARM [DrCCTProf](https://xl10.github.io/blog/drcctprof.html)

Tools built on DynamoRIO and available in the [release package](https://dynamorio.org/page_download) include:

- The memory debugging tool [Dr. Memory](http://drmemory.org)
- The tracing and analysis framework [drmemtrace](https://dynamorio.org/page_drcachesim.html) with multiple tools that operate on both online (with multi-process support) and offline instruction and memory address traces:
  - The cache simulator [drcachesim](https://dynamorio.org/sec_drcachesim_tools.html#sec_tool_cache_sim)
  - [TLB simulation](https://dynamorio.org/sec_drcachesim_tools.html#sec_tool_TLB_sim)
  - [Reuse distance](https://dynamorio.org/sec_drcachesim_tools.html#sec_tool_reuse_distance)
  - [Reuse time](https://dynamorio.org/sec_drcachesim_tools.html#sec_tool_reuse_time)
  - [Opcode mix](https://dynamorio.org/sec_drcachesim_tools.html#sec_tool_opcode_mix)
  - [Function call tracing](https://dynamorio.org/sec_drcachesim_tools.html#sec_tool_func_view)
- The legacy processor emulator
  [drcpusim](https://dynamorio.org/page_drcpusim.html)
- The "strace for Windows" tool [drstrace](http://drmemory.org/strace_for_windows.html)
- The code coverage tool [drcov](https://dynamorio.org/page_drcov.html)
- The library tracing tool [drltrace](http://dynamorio.org/page_drltrace.html)
- The memory address tracing tool [memtrace](https://github.com/DynamoRIO/dynamorio/blob/master/api/samples/memtrace_x86.c) ([drmemtrace](https://dynamorio.org/page_drcachesim.html)'s offline traces are faster with more surrounding infrastructure, but this is a simpler starting point for customized memory address tracing)
- The memory value tracing tool [memval](https://github.com/DynamoRIO/dynamorio/blob/master/api/samples/memval_simple.c)
- The instruction tracing tool [instrace](https://github.com/DynamoRIO/dynamorio/blob/master/api/samples/instrace_x86.c) ([drmemtrace](https://dynamorio.org/page_drcachesim.html)'s offline traces are faster with more surrounding infrastructure, but this is a simpler starting point for customized instruction tracing)
- The basic block tracing tool [bbbuf](https://github.com/DynamoRIO/dynamorio/blob/master/api/samples/bbbuf.c)
- The instruction counting tool [inscount](https://github.com/DynamoRIO/dynamorio/blob/master/api/samples/inscount.c)
- The dynamic fuzz testing tool [Dr. Fuzz](http://drmemory.org/page_drfuzz.html)
- The disassembly tool [drdisas](https://dynamorio.org/page_drdisas.html)
- And more, including opcode counts, branch instrumentation, etc.: see [API samples](https://dynamorio.org/API_samples.html)

## Building your own custom tools

DynamoRIO's powerful API abstracts away the details of the underlying
infrastructure and allows the tool builder to concentrate on analyzing or
modifying the application's runtime code stream.  API documentation is
included in the release package and can also be [browsed
online](http://dynamorio.org/page_user_docs.html).  [Slides from our past tutorials are
also available](https://dynamorio.org/page_slides.html).

## Downloading DynamoRIO

DynamoRIO is available free of charge as a [binary package for both Windows
and Linux](https://dynamorio.org/page_download.html).
DynamoRIO's [source code is
available](https://github.com/DynamoRIO/dynamorio) primarily under a [BSD
license](https://dynamorio.org/page_license.html).

## Obtaining Help

Use the [discussion list](http://groups.google.com/group/DynamoRIO-Users)
to ask questions.

To report a bug, use the [issue
tracker](https://github.com/DynamoRIO/dynamorio/issues).

See also [the DynamoRIO home page](http://dynamorio.org/): [http://dynamorio.org/](http://dynamorio.org/)
