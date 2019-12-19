#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import os
import sys
import time

import test_utils
import sihd

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

import socket

def test_ihm():
    reader = sihd.srcs.Readers.IpReader()
    handler = sihd.srcs.Handlers.IpHandler()
    gui = sihd.srcs.GUI.WxPython.ip.WxPythonIpGui()
    reader.add_observer(handler)
    handler.add_observer(gui)
    reader.set_conf("port", 4200)
    reader.set_conf("type", "tcp")
    reader.load_conf()
    handler.load_conf()
    gui.load_conf()
    assert(reader.start())
    assert(gui.start())
    assert(gui.stop())
    assert(reader.stop())

if __name__ == '__main__':
    logger.info("Starting test")
    if sys.stdout.isatty():
        test_ihm()
    logger.info("Test ending")
