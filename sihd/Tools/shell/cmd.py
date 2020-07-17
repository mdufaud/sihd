#!/usr/bin/python
# coding: utf-8

import subprocess
import os
import sys
import shlex

def execute(cmd, devnull=False):
    if isinstance(cmd, str):
        cmd = shlex.split(cmd)
    dn = None
    if devnull == True:
        try:
            dn = open(os.devnull, 'w')
        except IOError:
            pass
    try:
        ipr = subprocess.Popen(cmd, stdout=dn, stderr=dn)
        if dn:
            dn.close()
    except Exception as e:
        if dn and not dn.closed:
            dn.close()
        raise e
