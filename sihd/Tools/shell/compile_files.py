#!/usr/bin/python
#coding: utf-8

""" System """

import os
import time
import logging

from .exec_cmd import exec_cmd

def c_file(name, dest):
    log = logging.getLogger()
    if os.path.exists(name) is False:
        log.error("File {} not found".format(name))
        return False
    log.info("Compiling %s file to %s" % (os.path.basename(name),
                                            os.path.basename(dest)))
    if exec_cmd(['/usr/bin/gcc', name, '-o', dest]) is False:
        log.error("Could not compile file %s: %s" % (name, e.message))
        return False
    time.sleep(0.1)
    return True

def c_lib(name):
    log = logging.getLogger()
    if os.path.exists(name) is False:
        log.error("File {} not found".format(name))
        return False
    basename = os.path.basename(name)
    dest = os.path.splitext(name)[0] + ".so"
    log.info("Compiling %s file to %s" % (basename, os.path.basename(dest)))
    if exec_cmd(['/usr/bin/gcc', "-shared", "-Wl,-soname,{}".format(basename), "-o", dest, "-fPIC", name], no_redirect=True) is False:
        log.error("Could not compile file %s: %s" % (name, e.message))
        return False
    time.sleep(0.1)
    return True
