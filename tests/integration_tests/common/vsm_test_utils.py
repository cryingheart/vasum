'''! Module containing utilities for zone-tests

@author Lukasz Kostyra (l.kostyra@samsung.com)
'''
import subprocess
import os

def launchProc(cmd):
    '''! Launch specified command as a subprocess.

    Launches provided command using Python's subprocess module. Returns output from stdout and
    stderr.

    @param cmd Command to be launched
    @return Tuple containing subprocess exit code and text output received
    '''
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    rc = p.wait()
    output = p.stdout.read()
    return (rc,output)


def mount(dir, opts=[]):
    '''! Mounts specified directory with additional command line options.

    @param dir Directory to be mounted
    @param opts Additional command line options for mount. This argument is optional.
    @see launchProc
    '''
    launchProc(" ".join(["mount"] + opts + [dir]))


def umount(dir):
    '''! Unmounts specified directory.

    @param dir Directory to be umounted
    @see launchProc
    '''
    launchProc(" ".join(["umount"] + [dir]))


def isNumber(str):
    '''! Checks if provided String is a number.

    @param str String to be checked if is a number.
    @return True if string is a number, False otherwise.
    '''
    try:
        int(str)
        return True
    except ValueError:
        return False
