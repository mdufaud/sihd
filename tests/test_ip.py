#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import time
import unittest

import utils
import sihd
logger = sihd.log.setup()

from sihd.Handlers.AHandler import AHandler

class TestHandler(AHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self.add_channel_input('input', type='queue')

    def handle(self, channel):
        if channel == self.input_msg:
            data = channel.read()
            self.log_info("Received msg: {}".format(msg))
        return True

def get_udp_sender():
    interactor = sihd.Interactors.IpInteractor()
    interactor.set_conf({
        "runnable_type": "thread",
        "host": "localhost",
        "port": 4200,
        "protocol": "udp",
        "runnable_frequency": 10,
    })
    interactor.set_conf({
        "runnable_timeout": 0.5
    }, dynamic=True)
    interactor.setup()
    interactor.set_interaction("Some udp message")
    return interactor

def get_tcp_sender():
    interactor = sihd.Interactors.IpInteractor()
    interactor.set_conf({
        "runnable_type": "thread",
        "host": "localhost",
        "port": 4200,
        "runnable_frequency": 100,
    })
    interactor.set_conf({
        "runnable_steps": 5
    }, dynamic=True)
    interactor.setup()
    interactor.set_interaction("Some tcp message")
    return interactor

class TestIpServer(unittest.TestCase):

    def setUp(self):
        print()
        sihd.tree.clear()

    def tearDown(self):
        pass

    def set_reader_queues(self, r):
        for name in r._get_channels_to_add(): 
            r.set_channel_conf(name, type='queue')

    def run_sender(self, get_interactor, type):
        reader = sihd.Readers.IpReader()
        ip_handler = sihd.Handlers.IpServerHandler()
        if utils.is_multiprocessing():
            reader.set_conf("runnable_type", "process")
            self.set_reader_queues(reader)
            ip_handler.set_dyn_conf("channels_mp", True)
        reader.set_conf("port", 4200)
        reader.set_conf("protocol", type)
        self.assertTrue(reader.setup())
        self.assertTrue(ip_handler.setup())

        ip_handler.handle_service(reader)

        self.assertTrue(reader.start())
        self.assertTrue(ip_handler.start())
        interactor = get_interactor()
        try:
            time.sleep(0.1)
            interactor.start()
            time.sleep(1.5)
            interactor.stop()
            time.sleep(0.1)
        except Exception as e:
            interactor.stop()
            logger.error("{}".format(e))
        self.assertTrue(reader.stop())
        self.assertTrue(ip_handler.stop())

    @unittest.skipIf(os.getenv("IP_NC") is not None, "No env var IP_NC")
    def test_udp_sender(self):
        self.run_sender(get_udp_sender, "udp")

    @unittest.skipIf(os.getenv("IP_NC") is not None, "No env var IP_NC")
    def test_tcp_sender(self):
        self.run_sender(get_tcp_sender, "tcp")

    @unittest.skipIf(os.getenv("IP_NC") is None, "No env var IP_NC")
    def test_with_net_cat(self):
        reader = sihd.Readers.IpReader()
        ip_handler = sihd.Handlers.IpServerHandler()
        reader.set_conf("runnable_type", "process")
        self.set_reader_queues(reader)
        ip_handler.set_conf("channels_mp", True)
        reader.set_conf("port", 4200)
        reader.set_conf("protocol", "tcp")
        self.assertTrue(reader.setup())
        self.assertTrue(ip_handler.setup())

        ip_handler.handle_service(reader)

        self.assertTrue(reader.start())
        self.assertTrue(ip_handler.start())
        try:
            time.sleep(20)
        except Exception as e:
            logger.error("{}".format(e))
        self.assertTrue(reader.stop())
        self.assertTrue(ip_handler.stop())

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_gui(self):
        reader = sihd.Readers.IpReader()
        handler = sihd.Handlers.IpServerHandler()
        gui = sihd.GUI.WxPython.ip.WxPythonIpGui()

        reader.set_conf("runnable_type", "process")
        self.set_reader_queues(reader)
        ip_handler.set_conf("channels_mp", True)
        reader.set_conf("port", 4200)
        reader.set_conf("protocol", type)

        self.assertTrue(reader.setup())
        self.assertTrue(handler.setup())
        self.assertTrue(gui.setup())

        self.assertTrue(reader.start())
        self.assertTrue(gui.start())
        self.assertTrue(gui.stop())
        self.assertTrue(reader.stop())
        time.sleep(0.5)

if __name__ == '__main__':
    unittest.main(verbosity=2)
