#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import os
from .INamedObject import INamedObject

class IDumpable(INamedObject):

    def __init__(self, name="IDumpable"):
        super(IDumpable, self).__init__(name)

    def _dump(self):
        """ Should return a buffer """
        return None

    def _load(self, buf):
        """ Fill vars with read buffer """
        return False

    def _has_load_magic(self, buf):
        """ Check if file is good format """
        return True

    def dump_to_file(self, filename):
        """ Auto dump to file """
        dump = self._dump()
        if dump is None:
            return False
        with open(filename, "w+") as f:
            f.write(dump)
        return True

    def load_from_file(self, filename):
        """ Load from file if has good magic """
        with open(filename, "r") as f:
            buf = f.read()
            if self._has_load_magic(buf):
                return self._load(buf)
        return False
