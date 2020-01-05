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
logging.basicConfig(level=logging.DEBUG)

from sihd.srcs.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self.out = None
        self.err = None

    def handle(self, reader, data):
        print("Received:\n", data.decode())
        self.out = data.decode()
        return True

    def on_error(self, reader, error):
        print("Error rcv: \n", error.decode())
        self.err = error.decode()
        return True

class PipeHandler(IHandler):

    def __init__(self, app=None, name="PipeHandler"):
        super(PipeHandler, self).__init__(app=app, name=name)

    def handle(self, reader, proc):
        print("Received:\n", data.decode())
        return True

    def on_error(self, reader, error):
        print("Error rcv: \n", error)
        return True

def test_service(cmd):
    reader = sihd.Readers.CmdReader()
    reader.set_conf("cmd", cmd)
    test_handler = TestHandler()
    reader.add_observer(test_handler)
    reader.setup()
    assert(reader.start())
    try:
        time.sleep(0.3)
    except KeyboardInterrupt:
        print("\nKeyboard Interruption")
        pass
    assert(reader.stop())

if __name__ == '__main__':
    logger.info("Starting test")
    test_service("ls")
    test_service("ls /smth")
    logger.info("Test ending")
