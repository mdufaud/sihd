#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import time
import os
import sys

from sihd.srcs.Readers.IReader import IReader

try:
    input = raw_input
except NameError:
    pass

class StdinReader(IReader):

    def __init__(self, path=None, app=None, name="StdinReader"):
        super(StdinReader, self).__init__(app=app, name=name)
        self._set_default_conf({
            "first_question": "",
        })
        self._question = ""
        self.set_run_method(self._read_input)

    """ IConfigurable """

    def _load_conf_impl(self):
        super(StdinReader, self)._load_conf_impl()
        q = self.get_conf_val("first_question")
        if q is not None:
            self._question = str(q)
        return True

    """ Reader """

    def _read_input(self):
        try:
            line = input(self._question)
        except EOFError as e:
            line = None
        if self.is_active():
            self.notify_observers(line)
        if line is None:
            self.stop()
            return False
        return True

    def set_question(self, question):
        self._question = str(question)
