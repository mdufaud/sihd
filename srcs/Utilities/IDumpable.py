#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import os
from .INamedObject import INamedObject

class IDumpable(INamedObject):

    def __init__(self, name="IDumpable"):
        super(IDumpable, self).__init__(name)

    # Should return a buffer
    def dump(self):
        return None

    # Fill var with buffer
    def load(self, buf):
        return False

    # Check if file is good format
    def has_load_magic(self, buf):
        return True

    # Auto dump to file
    def dump_to_file(self, filename):
        with open(filename, "w") as f:
            f.write(self.dump())
        return True

    # Load from file if has good magic
    def load_from_file(self, filename):
        with open(filename, "r") as f:
            buf = f.read()
            if self.has_load_magic(buf):
                self.load(buf)
            else:
                return False
        return True
