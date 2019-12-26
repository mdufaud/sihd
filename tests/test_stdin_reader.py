#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import os
import sys
import time

import test_utils
import sihd

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.INFO)

import socket

from sihd.srcs.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self._step = 0

    def handle(self, reader, line):
        line = line.strip()
        print("Received {}".format(line))
        step = self._step
        if step is 0:
            reader.set_question("Great - Type 5 now: ")
            self._step = 1
        elif step is 1 and line is '5':
            reader.set_question("Thanks ! Type q to quit\n")
            self._step = 2
        elif step is 2 and line is 'q':
            reader.stop()
        return True

def test_reader():
    reader = sihd.Readers.StdinReader()
    test_handler = TestHandler()
    reader.add_observer(test_handler)
    reader.set_conf("first_question", "How are you ? ")
    reader.load_conf()
    assert(reader.start())
    try:
        time.sleep(30)
    except KeyboardInterrupt:
        print("\n - Interrupted")
        pass
    assert(reader.stop())

if __name__ == '__main__':
    logger.info("Starting test")
    if sys.stdout.isatty():
        test_reader()
    logger.info("Test ending")
