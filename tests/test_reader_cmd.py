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

from sihd.srcs.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self.out = None
        self.err = None

    def handle(self, reader, data):
        print("Received: ", data)
        self.out = data.decode()
        return True

    def on_error(self, reader, error):
        print("Error rcv: ", error)
        self.err = error.decode()
        return True

def test_service(cmd, out=None, err=None):
    reader = sihd.Readers.CmdReader()
    reader.set_conf({
        "cmd": cmd
    })
    h = TestHandler()
    reader.add_observer(h)
    reader.setup()
    assert(reader.start())
    try:
        time.sleep(0.3)
    except KeyboardInterrupt:
        print("\nKeyboard Interruption")
        pass
    assert(reader.stop())
    if out:
        assert(h.out == out)
    if err:
        assert(h.err.find(err) != -1)

def test_pipe_service(cmd1, cmd2, out=None, err=None):
    reader1 = sihd.Readers.CmdReader(name="FirstCmd")
    reader1.set_conf("cmd", cmd1)
    reader2 = sihd.Readers.CmdReader(name="SecondCmd")
    reader2.set_conf("cmd", cmd2)
    reader1.setup()
    reader2.setup()
    reader2.configure_pipe_cmd_reader(reader1)
    h = TestHandler()
    reader2.add_observer(h)
    assert(reader1.start())
    assert(reader2.start())
    try:
        time.sleep(0.5)
    except KeyboardInterrupt:
        print("\nKeyboard Interruption")
        pass
    assert(reader2.stop())
    assert(reader1.stop())
    if out:
        assert(h.out == out)
    if err:
        assert(h.err.find(err) != -1)


if __name__ == '__main__':
    logger.info("Starting test")
    test_service("echo Hello World", out="Hello World\n")
    test_service("ls /smth", err="/smth")
    test_pipe_service("echo Hello World", "wc -c", out="12\n")
    logger.info("Test ending")
