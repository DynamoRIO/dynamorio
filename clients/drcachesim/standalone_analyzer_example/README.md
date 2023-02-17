This directory contains an example showing how to [create new analysis
tools](https://dynamorio.org/sec_drcachesim_newtool.html)

Building

    cmake -DDynamoRIO_DIR=/path/to/dynamorio/exports/cmake/ \
                          /path/to/dynamorio/clients/drcachesim/standalone_analyzer_example/
    make

Example usage

    $ ./analyzer_example -trace \
        /path/to/dynamorio/clients/drcachesim/tests/drmemtrace.threadsig.x64.tracedir/drmemtrace.threadsig.10506.7343.trace.gz
    Found 4421 instructions

