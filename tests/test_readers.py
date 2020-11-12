#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import time
import socket
import unittest

import utils
import sihd
logger = sihd.log.setup('info')

from sihd.handlers.AHandler import AHandler

class StdinHandler(AHandler):

    def __init__(self, reader, name="StdinHandler", app=None):
        super(StdinHandler, self).__init__(app=app, name=name)
        self._step = 0
        #self.add_channel_input("input", type='queue')
        self.add_channel_input("input")
        self.reader = reader

    def handle(self, channel):
        line = channel.read()
        if line is None:
            return None
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
            self.reader.question.write("Great - Type 5 now: ")
            self._step = 1
        elif step == 1 and line == '5':
            self.reader.question.write("Thanks ! Type q to quit: ")
            self._step = 2
        elif step == 2 and line == 'q':
            self.reader.stop()
        return True

class TestReader(unittest.TestCase):

    def setUp(self):
        print()
        sihd.tree.clear()

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_stdin_reader(self):
        reader = sihd.readers.StdinReader()
        handler = StdinHandler(reader)
        self.assertTrue(reader.setup())
        self.assertTrue(handler.setup())
        handler.link_channel("input", reader.answer)
        #reader.answer.add_observer(handler.input)
        reader.question.write("How are you ? ")
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        try:
            i = 0
            while reader.is_active() and i < 15:
                time.sleep(1)
                i += 1
        except KeyboardInterrupt:
            print("\nKeyboard Interruption")
            pass
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())

if __name__ == '__main__':
    unittest.main(verbosity=2)
