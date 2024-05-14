#!/usr/bin/env python3

"""
Run a program as a subprocess after configuring the vector length of the
system to match the value in the first argument.
"""

import ctypes
import subprocess
import sys

# These constants must match linux/prctl.h
PR_SVE_SET_VL = 50

PR_SVE_VL_INHERIT = 1 << 17
PR_SVE_SET_VL_ONEXEC = 1 << 18
PR_SVE_VL_LEN_MASK = 0xffff

def set_vector_length(vector_length):
    """
    Use Prctl(2) to set the process tree's sve vector length. See
    https://man7.org/linux/man-pages/man2/prctl.2.html for more details
    """
    vector_length_bytes =  vector_length // 8
    libc = ctypes.cdll.LoadLibrary('libc.so.6')
    return_code = libc.prctl(
        PR_SVE_SET_VL,
        PR_SVE_SET_VL_ONEXEC | PR_SVE_VL_INHERIT | vector_length_bytes)
    if return_code < 0:
        print(f'Failed to set VL, rc: {return_code}')
        sys.exit(1)
    set_vector_length = (return_code & PR_SVE_VL_LEN_MASK) * 8

    if set_vector_length != vector_length:
        print(
            f"Requested vector length {vector_length} not set, maximum of {set_vector_length} available",
            file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    if any(arg in ["--help", "-h"] for arg in sys.argv) or len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} [vector_length] [program] [program args]...")
        print("\nSet the vector length of this process tree to [vector_length] bits")
        print("and then execute [program]")
        sys.exit(0)

    set_vector_length(int(sys.argv[1]))

    command = sys.argv[2:]
    sys.exit(subprocess.run(command, check=False).returncode)
