#!/usr/bin/env python

# XXX #1767: Python3 introduces a print function instead of the print
# statement in python2. This import allows us to access the print
# function in python2+.
from __future__ import print_function

import gdb
import os
import traceback
import re

# XXX #1767: Python3 unified the int and long types. To maintain
# compatibility with python2, we will use long and alias it to int
# for python3.
if sys.version_info > (3,):
    long = int

print('Loading gdb scripts for debugging DynamoRIO...')

# FIXME i#531: Support loading symbols after attaching.

# If we were sourced directly instead of auto-loaded, try to guess where DR is
# so we can make RunDR work.  We don't need this for anything else yet.
try:
    DR_LIBDIR = os.path.dirname(os.path.abspath(__file__))
except:
    DR_LIBDIR = None


class DROption(gdb.Parameter):

    def __init__(self, dr_option, param_class):
        super(DROption, self).__init__("dr-" + dr_option, gdb.COMMAND_OBSCURE,
                                       param_class)
        self.dr_option = dr_option

    set_doc = ("DynamoRIO option of the same name.")
    show_doc = set_doc
    def get_set_string(self):
        return str(self.value)
    def get_show_string(self, svalue):
        return svalue


# The client and client-args options are special in that they do not go in
# DYNAMORIO_OPTIONS without being massaged first.  We also specify
# documentation strings for them.
class DRClient(DROption):
    def __init__(self):
        super(DRClient, self).__init__("client", gdb.PARAM_OPTIONAL_FILENAME)

    set_doc = ("Path to DynamoRIO client to run when invoking DR.  "
               "Leave blank to run without a client.")
    show_doc = set_doc

class DRClientArgs(DROption):
    def __init__(self):
        super(DRClientArgs, self).__init__("client-args", gdb.PARAM_STRING)

    set_doc = ("DynamoRIO client arguments.")
    show_doc = set_doc


# Client specification options.
dr_client = DRClient()
dr_client_args = DRClientArgs()

# Other useful flags to pass to DynamoRIO.
dr_options = [
        DROption('msgbox_mask', gdb.PARAM_ZINTEGER),
        DROption('loglevel', gdb.PARAM_ZINTEGER),
        DROption('logmask', gdb.PARAM_ZINTEGER),
        ]


class RunDR(gdb.Command):

    """Run the application under DynamoRIO with the current options.

    Goes through the drrun script to avoid depending on the config file format.
    """

    def __init__(self):
        super(RunDR, self).__init__("rundr", gdb.COMMAND_OBSCURE)

    def invoke(self, arg, from_tty):
        # Find drrun.
        parts = DR_LIBDIR.split(os.sep)
        build_mode = parts[-1]
        arch = parts[-2][-2:]
        if build_mode not in ('debug', 'release'):
            print("Unrecognized build_mode {0}.".format(build_mode))
            return
        if arch not in ('32', '64'):
            print("Unable to find drrun using libdir {0}, unrecognized arch {1}.".format(
                DR_LIBDIR, arch))
            return
        drrun_path = os.sep.join(parts[:-2])
        drrun_path = os.path.join(drrun_path, 'bin{0}/drrun'.format(arch))
        gdb.execute("set exec-wrapper {0} -{1}".format(drrun_path, build_mode))

        # Build options string.
        # FIXME: The escaping is most likely wrong here.  It's tricky because
        # the command is parsed by gdb and DynamoRIO.
        env_opts = os.environ.get('DYNAMORIO_OPTIONS', '')
        param_opts = ' '.join("-{0} {1}".format(p.dr_option, p.value)
                              for p in dr_options)
        client_opts = ''
        if dr_client.value:
            client_opts = ('-code_api -client_lib {0};0;{1}'.format(
                           dr_client.value, dr_client_args.value))
        dr_opts = ' '.join([env_opts, param_opts, client_opts])

        gdb.execute("set env DYNAMORIO_OPTIONS " + dr_opts)
        gdb.execute("run " + arg)

RunDR()


def gdb_has_breakpoints():
    match = re.match(r'^(\d+)\.(\d+)', gdb.VERSION)
    if not match:
        # Some version strings are like this: "Fedora 7.7.1-21.fc20"
        match = re.match(r'.*\s+(\d+)\.(\d+)', gdb.VERSION)
        if not match:
            print("Error parsing gdb version ({0})".format(gdb.VERSION))
            return False
    major = int(match.group(1))
    minor = int(match.group(2))
    if major > 7:
        return True
    elif major == 7 and minor >= 3:
        return True
    return False


# gdb 7.2 doesn't really support breakpoints in the Python API.  We do a version
# check so we can print an informative message rather than dying with an
# exception on "class PrivloadBP(gdb.Breakpoint)".
if gdb_has_breakpoints():

    class PrivloadBP(gdb.Breakpoint):

        # Enable to debug this breakpoint.
        DEBUG = False
        DYNAMORIO_BP = True

        def __init__(self):
            super(PrivloadBP, self).__init__("dr_gdb_add_symbol_file",
                                             internal=not self.DEBUG)

        def stop(self):
            try:
                frame = gdb.newest_frame()
                filename = frame.read_var("filename").string()
                textaddr = long(frame.read_var("textaddr"))
                cmd = "add-symbol-file '{0}' {1}".format(filename, hex(textaddr))
                print("Executing gdb command:", cmd)
                # We suppress output to the screen with to_string unless we're
                # debugging.
                gdb.execute(cmd, to_string=not self.DEBUG)
                return self.DEBUG  # Controls whether the user stops here or not.
            except:
                # gdb won't print a Python stack trace if we raise an exception,
                # so we do it ourselves.
                traceback.print_exc()
                return True


    # Delete all breakpoints set from previous runs and initializations and
    # replace them with new ones.
    def remove_old_bps():
        bps = gdb.breakpoints()
        if not bps:
            return
        for bp in bps:
            if getattr(bp, 'DYNAMORIO_BP', False):
                bp.delete()
    remove_old_bps()

    # We need pending breakpoints in order to wait for LD_PRELOAD to bring in
    # the library with the symbol.
    gdb.execute("set breakpoint pending on")
    PrivloadBP()

else:
    print("This version of gdb does not support breakpoints from Python.  "
          "Libraries loaded by DynamoRIO will not be automatically "
          "registered with gdb.")
