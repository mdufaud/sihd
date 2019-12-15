#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import subprocess
import os

def exec_cmd(cmd, no_redirect=False):
    dn = None
    if no_redirect == False:
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
        return False
    return True
