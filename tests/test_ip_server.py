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

def send_udp():
    interactor = sihd.srcs.Interactors.IpInteractor()
    interactor.set_conf({"host": "localhost", "port": 4200, "type": "udp"})
    interactor.load_conf()
    return interactor.interact("Some message")

def send_tcp():
    interactor = sihd.srcs.Interactors.IpInteractor()
    interactor.set_conf({"host": "localhost", "port": 4200})
    interactor.load_conf()
    return interactor.interact("Some message")

def sender_thread(fun):
    thread = sihd.srcs.Utilities.SihdThread(frequency=10, timeout=1.2)
    thread.set_stepfun(fun)
    return thread

def test(fun, t):
    reader = sihd.srcs.Readers.IpReader()
    ip_handler = sihd.srcs.Handlers.IpHandler()
    test_handler = TestHandler()
    reader.add_observer(ip_handler)
    ip_handler.add_observer(test_handler)
    reader.set_conf("port", 4200)
    reader.set_conf("type", t)
    reader.load_conf()
    ip_handler.load_conf()
    assert(reader.start())
    try:
        thread = sender_thread(fun)
        time.sleep(1)
        thread.start()
        time.sleep(2)
        thread.stop()
        time.sleep(1)
    except Exception as e:
        logger.error("{}".format(e))
    assert(reader.stop())

if __name__ == '__main__':
    logger.info("Starting test UDP")
    test(send_udp, "udp")
    logger.info("Starting test TCP")
    test(send_tcp, "tcp")
    logger.info("Test ending")
