#!/usr/bin/env python3

"""
Print the vector length in the current context
"""

import ctypes
import sys

# These constants must match linux/prctl.h
PR_SVE_GET_VL = 51
PR_SVE_VL_LEN_MASK = 0xffff

def get_vector_length():
    """
    Use Prctl(2) to set the process tree's sve vector length. See
    https://man7.org/linux/man-pages/man2/prctl.2.html for more details
    """
    libc = ctypes.cdll.LoadLibrary('libc.so.6')
    return_code = libc.prctl(
        PR_SVE_GET_VL)
    if return_code < 0:
        print(f'Failed to get VL, rc: {return_code}')
        sys.exit(1)
    print((return_code & PR_SVE_VL_LEN_MASK) * 8)

if __name__ == '__main__':
    if any(arg in ["--help", "-h"] for arg in sys.argv) or len(sys.argv) > 1:
        print(f"usage: {sys.argv[0]}")
        print("\nGets the current vector length of this process tree")
        sys.exit(0)

    get_vector_length()

