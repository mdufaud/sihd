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

from sihd.handlers.AHandler import AHandler
from sihd.readers.ip import *
from sihd.handlers.ip import *
from sihd.interactors.ip import *

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
    interactor = IpInteractor()
    interactor.configuration.load({
        "runnable_type": "thread",
        "host": "localhost",
        "port": 4200,
        "protocol": "udp",
        "runnable_frequency": 10,
    })
    interactor.configuration.load({
        "runnable_timeout": 0.5
    }, dynamic=True)
    interactor.setup()
    interactor.set_interaction("Some udp message")
    return interactor

def get_tcp_sender():
    interactor = IpInteractor()
    interactor.configuration.load({
        "runnable_type": "thread",
        "host": "localhost",
        "port": 4200,
        "runnable_frequency": 100,
    })
    interactor.configuration.load({
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
        reader = IpReader()
        ip_handler = IpServerHandler()
        if utils.is_multiprocessing():
            reader.configuration.set("runnable_type", "process")
            self.set_reader_queues(reader)
            ip_handler.configuration.set_dynamic("channels_mp", 1)
        reader.configuration.set("port", 4200)
        reader.configuration.set("protocol", type)
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
        reader = IpReader()
        ip_handler = IpServerHandler()
        reader.configuration.set("runnable_type", "process")
        self.set_reader_queues(reader)
        ip_handler.configuration.set("channels_mp", 1)
        reader.configuration.set("port", 4200)
        reader.configuration.set("protocol", "tcp")
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

if __name__ == '__main__':
    unittest.main(verbosity=2)
