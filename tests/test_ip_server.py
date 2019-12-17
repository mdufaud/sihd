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

from sihd.srcs.Handlers.IHandler import IHandler

class TestHandler(IHandler):

    def __init__(self, app=None, name="WordHandler"):
        super(TestHandler, self).__init__(app=app, name=name)

    def handle(self, observable, data):
        msg, co = data
        self.log_info("Received: {} from {}".format(msg, co.getpeername()))
        return True

def send():
    interactor = sihd.srcs.Interactors.IpInteractor(port=4200)
    interactor.interact("Some message")

def sender_thread():
    thread = sihd.srcs.Utilities.SihdThread(msleep=100)
    thread.set_stepfun(send)
    return thread

if __name__ == '__main__':
    logger.info("Starting test")
    reader = sihd.srcs.Readers.IpReader()
    handler = TestHandler()
    reader.add_observer(handler)
    reader.set_conf("port", 4200)
    reader.load_conf()
    reader.start()
    thread = sender_thread()
    time.sleep(2)
    thread.start()
    time.sleep(2)
    thread.stop()
    time.sleep(1)
    reader.stop()
