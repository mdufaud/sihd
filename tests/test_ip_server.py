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

from sihd.srcs.Handlers.IHandler import IHandler

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

def test(get_interactor, t):
    reader = sihd.Readers.IpReader()
    ip_handler = sihd.Handlers.IpServerHandler()
    test_handler = TestHandler()
    reader.add_observer(ip_handler)
    ip_handler.add_observer(test_handler)
    reader.set_conf("port", 4200)
    reader.set_conf("type", t)
    reader.setup()
    ip_handler.setup()
    assert(reader.start())
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
    assert(reader.stop())

if __name__ == '__main__':
    try:
        logger.info("Starting test UDP")
        test(get_udp_sender, "udp")
        logger.info("Starting test TCP")
        test(get_tcp_sender, "tcp")
        logger.info("Test ending")
    except KeyboardInterrupt as e:
        sys.exit(1)
