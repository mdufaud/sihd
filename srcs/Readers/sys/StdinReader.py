#!/usr/bin/python
#coding: utf-8

""" System """

import time
import os
import sys
import select

from sihd.srcs.Readers.IReader import IReader

class StdinReader(IReader):

    def __init__(self, app=None, name="StdinReader"):
        super(StdinReader, self).__init__(app=app, name=name)
        self._set_default_conf({
            "question": "",
        })
        self._question = ""
        self._asked = False
        self.set_step_method(self.diffuse_input)
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
        if self._asked is True:
            return
        print(self._question, end="", flush=True)
        self._asked = True

    def get_input(self, timeout=0):
        inputs = self._inputs
        buf = self._buffer
        r, w, e = select.select(inputs, [], [], timeout)
        if e:
            self.log_error("Select exceptional: {}".format(e))
            return None
        line = None
        if self.is_active() and r:
            line = os.read(r[0], buf)
        return line

    def diffuse_input(self):
        self.__ask()
        try:
            line = self.get_input(timeout=1.0)
        except Exception as e:
            self.stop()
            raise
        #Timeout
        if line is None:
            return True
        if line == b'':
            self.stop()
            return False
        self.deliver(line)
        self._asked = False
        return True

    def set_question(self, question):
        self._question = str(question)
