#!/usr/bin/python
# coding: utf-8

import os
import time
import logging
logger = logging.getLogger("sihd.compiler")

from .cmd import execute

def c_file(name, dest):
    if os.path.exists(name) is False:
        logger.error("File {} not found".format(name))
        return False
    logger.info("Compiling %s file to %s" % (os.path.basename(name),
                                            os.path.basename(dest)))
    if execute(['/usr/bin/gcc', name, '-o', dest]) is False:
        logger.error("Could not compile file %s: %s" % (name, e.message))
        return False
    return True

def c_lib(name):
    if os.path.exists(name) is False:
        logger.error("File {} not found".format(name))
        return False
    basename = os.path.basename(name)
    dest = os.path.splitext(name)[0] + ".so"
    logger.info("Compiling %s file to %s" % (basename, os.path.basename(dest)))
    if execute(['/usr/bin/gcc', "-shared", "-Wl,-soname,{}"\
            .format(basename), "-o", dest, "-fPIC", name]) is False:
        logger.error("Could not compile file %s: %s" % (name, e.message))
        return False
    return True
