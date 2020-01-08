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
import unittest

from sihd.srcs.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self._step = 0

    def handle(self, reader, line):
        line = line.decode('ascii')
        if line == "":
            print()
            logger.info("Client has stopped input")
            return
        line = line.strip()
        if line == "":
            return
        print("Received: '{}'".format(line))
        step = self._step
        if step == 0:
            reader.set_question("Great - Type 5 now: ")
            self._step = 1
        elif step == 1 and line == '5':
            reader.set_question("Thanks ! Type q to quit: ")
            self._step = 2
        elif step == 2 and line == 'q':
            reader.stop()
        return True

class TestStdinReader(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_reader(self):
        reader = sihd.Readers.StdinReader()
        test_handler = TestHandler()
        reader.add_observer(test_handler)
        reader.set_conf("question", "How are you ? ")
        reader.setup()
        self.assertTrue(reader.start())
        try:
            while reader.is_active():
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nKeyboard Interruption")
            pass
        self.assertTrue(reader.stop())

if __name__ == '__main__':
    unittest.main(verbosity=2)
