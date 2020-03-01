#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys
import time

import sihd
import unittest

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

import socket

from sihd.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)

    def handle(self, observable, msg, co):
        if observable._reader.is_tcp():
            self.log_info("Received: {} from {}".format(msg, co.getpeername()))
        else:
            self.log_info("Received: {}".format(msg))
        return True

def get_udp_sender():
    interactor = sihd.Interactors.IpInteractor()
    interactor.set_conf({
        "host": "localhost",
        "port": 4200,
        "type": "udp",
        "thread_frequency": 10,
        "thread_timeout": 0.5,
    })
    interactor.setup()
    interactor.set_interaction("Some udp message")
    return interactor

def get_tcp_sender():
    interactor = sihd.Interactors.IpInteractor()
    interactor.set_conf({
        "host": "localhost",
        "port": 4200,
        "thread_frequency": 100,
        "thread_max_iterations": 5
    })
    interactor.setup()
    interactor.set_interaction("Some tcp message")
    return interactor

class TestIpServer(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def run_sender(self, get_interactor, type):
        reader = sihd.Readers.IpReader()
        ip_handler = sihd.Handlers.IpServerHandler()
        test_handler = TestHandler()
        reader.add_observer(ip_handler)
        ip_handler.add_observer(test_handler)
        reader.set_conf("port", 4200)
        reader.set_conf("type", type)
        reader.setup()
        ip_handler.setup()
        self.assertTrue(reader.start())
        interactor = get_interactor()
        try:
            time.sleep(1)
            interactor.start()
            time.sleep(2)
            interactor.stop()
            time.sleep(1)
        except Exception as e:
            interactor.stop()
            logger.error("{}".format(e))
        self.assertTrue(reader.stop())

    def test_udp_sender(self):
        self.run_sender(get_udp_sender, "udp")

    def test_tcp_sender(self):
        self.run_sender(get_tcp_sender, "tcp")

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_gui(self):
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
        self.assertTrue(reader.start())
        self.assertTrue(gui.start())
        self.assertTrue(gui.stop())
        self.assertTrue(reader.stop())
        time.sleep(0.5)

if __name__ == '__main__':
    unittest.main(verbosity=2)
