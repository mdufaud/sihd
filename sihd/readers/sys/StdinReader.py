#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import os
import sys
import select

from sihd.readers.AReader import AReader

class StdinReader(AReader):

    def __init__(self, name="StdinReader", app=None):
        super().__init__(app=app, name=name)
        self.configuration.add_defaults({
            "question": "",
        })
        self.configuration.set('runnable_frequency', 5)
        self.__question = ""
        self.__asked = False
        self.__inputs = [sys.stdin.fileno()]
        self.__buffer = 4096
        self.__has_new_question = False
        self.add_channel_input("question")
        self.add_channel("answer")

    #
    # Channels
    #

    def handle(self, channel):
        if self.__asked is False and channel == self.question:
            question = channel.read()
            if question:
                self.set_question(question)

    def set_question(self, question):
        self.__question = str(question)
        self.__has_new_question = True

    #
    # Configuration
    #

    def on_setup(self):
        super().on_setup()
        self.set_question(str(conf.get("question")))
        return True

    #
    # Reader
    #

    def __ask(self):
        if self.__asked is True:
            return
        q = self.__question
        if q:
            print(q, end="", flush=True)
            self.__asked = True

    def get_input(self, timeout=0):
        inputs = self.__inputs
        r, w, e = select.select(inputs, [], [], timeout)
        if e:
            self.log_error("Select exceptional: {}".format(e))
            return None
        line = None
        if self.is_active() and r:
            line = os.read(r[0], self.__buffer)
        return line

    def on_thread_start(self, thread):
        super().on_thread_start(thread)
        time.sleep(0.5)

    def on_step(self):
        if self.__has_new_question is False:
            return
        self.__ask()
        line = self.get_input(timeout=0.3)
        if line is None:
            #Timeout
            return
        if line == b'':
            #Ctrl+d
            self.stop()
            return False
        self.answer.write(line)
        self.__has_new_question = False
        self.__asked = False
