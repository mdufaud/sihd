#!/usr/bin/python
# coding: utf-8

import sys
import logging
from sihd.Tools.shell.cmd import execute as __cmd_exe
from sihd.Tools.shell import compiler

logger = logging.getLogger('sihd.shell')

def execute(cmd, *args, **kwargs):
    try:
        __cmd_exe(cmd, *args, **kwargs)
        return True
    except Exception as e:
        logger.error(e)
    return False

def silent_execute(cmd):
    return execute(cmd, devnull=True)

def is_interactive():
    return sys.stdin and sys.stdin.isatty()
