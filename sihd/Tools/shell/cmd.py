#!/usr/bin/python
# coding: utf-8

""" System """
import subprocess
import os
import sys
import logging

def execute(cmd, devnull=False):
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

class Cmd(object):

    def __init__(self):
        self.logger = logging.getLogger('sihd.cmd')

    def execute(self, cmd, *args, **kwargs):
        try:
            execute(cmd, *args, **kwargs)
            return True
        except Exception as e:
            self.logger.error(e)
        return False

    def silent_execute(self, cmd):
        return self.execute(cmd, devnull=True)

cmd = Cmd()
