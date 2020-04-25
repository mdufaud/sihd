#!/usr/bin/python
#coding: utf-8

""" System """

import time
import os
import sys
import select

from sihd.Readers.IReader import IReader

class StdinReader(IReader):

    def __init__(self, app=None, name="StdinReader"):
        super(StdinReader, self).__init__(app=app, name=name)
        self._set_default_conf({"question": ""})
        self._question = ""
        self._asked = False
        self._inputs = [sys.stdin.fileno()]
        self._buffer = 4096
        self.add_channel_input("question", type='queue')
        self.add_channel_output("answer")

    def handle(self, channel):
        if self._asked is False and channel == self.question:
            question = channel.read()
            if question:
                self.set_question(question)

    def set_question(self, question):
        self._question = str(question)

    """ IConfigurable """

    def on_setup(self):
        super().on_setup()
        self._question = str(self.get_conf("question"))
        return True

    """ Reader """

    def __ask(self):
        if self._asked is True:
            return
        q = self._question
        if q:
            print(q, end="", flush=True)
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

    def on_step(self):
        self.__ask()
        try:
            line = self.get_input(timeout=0.3)
        except Exception as e:
            self.stop()
            raise
        #Timeout
        if line is None:
            return True
        if line == b'':
            self.stop()
            return False
        self.answer.write(line)
        self._asked = False
        return True
