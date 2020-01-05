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

import socket

def test_ihm():
    reader = sihd.Readers.IpReader()
    handler = sihd.Handlers.IpServerHandler()
    gui = sihd.GUI.WxPython.ip.WxPythonIpGui()
    reader.add_observer(handler)
    handler.add_observer(gui)
    reader.set_conf("port", 4200)
    reader.set_conf("type", "tcp")
    reader.setup()
    handler.setup()
    gui.setup()
    assert(reader.start())
    assert(gui.start())
    assert(gui.stop())
    assert(reader.stop())

if __name__ == '__main__':
    logger.info("Starting test")
    if sys.stdout.isatty():
        test_ihm()
    logger.info("Test ending")
