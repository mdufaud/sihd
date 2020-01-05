#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import time
import os
import sys
import select

from sihd.srcs.Readers.IReader import IReader

class StdinReader(IReader):

    def __init__(self, path=None, app=None, name="StdinReader"):
        super(StdinReader, self).__init__(app=app, name=name)
        self._set_default_conf({
            "question": "",
        })
        self._question = ""
        self._asked = False
        self.set_run_method(self._read_input)
        self._inputs = [sys.stdin.fileno()]
        self._buffer = 4096

    """ IConfigurable """

    def _setup_impl(self):
        super(StdinReader, self)._setup_impl()
        q = self.get_conf("question")
        if q is not None:
            self._question = str(q)
        return True

    """ Reader """

    def __ask(self):
        if self._asked:
            return
        print(self._question, end="", flush=True)
        self._asked = True

    def _read_input(self):
        self.__ask()
        inputs = self._inputs
        buf = self._buffer
        r, w, e = select.select(inputs, [], [], 1)
        if e:
            err = "Select exceptional: {}".format(e)
            self.log_error(err)
            self.notify_error(err)
            return False
        if self.is_active() and r:
            try:
                line = os.read(r[0], buf)
                self.notify_observers(line)
            except Exception as e:
                self.stop()
                raise
            if line == b'':
                self.stop()
                return False
            self._asked = False
        return True

    def set_question(self, question):
        self._question = str(question)
