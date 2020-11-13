#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import os

from .AFileReader import AFileReader

class LineReader(AFileReader):

    def __init__(self, name="LineReader", app=None):
        super().__init__(app=app, name=name)
        self.reader = None

    # override
    def open_file(self, path):
        try:
            fp = open(path, 'r')
            self.reader = fp
        except IOError as err:
            self.log_error("Cannot open: {}".format(err))
            return False
        self.log_info("Reading file {name}".format(name=path))
        return True

    # override
    def read_once(self):
        try:
            line = self.reader.readline()
        except EOFError as e:
            return self.read_end()
        if line == "":
            return self.read_end()
        line = line.strip()
        if line != "":
            self.write(line)

    # override
    def close_file(self):
        if self.reader:
            self.reader.close()
            self.reader = None
