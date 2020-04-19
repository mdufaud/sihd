#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import time
import test_utils
import socket
import unittest
import threading

import sihd
logger = sihd.set_log('debug')

from sihd.Handlers.IHandler import IHandler
from sihd.Core.Channel import *
from sihd.Core import SihdThread
from sihd.Core import SihdWorker

try:
    import multiprocessing
    from multiprocessing import Process, Manager
except ImportError:
    pass

try:
    if multiprocessing is not None:
        #checks for /dev/shm
        val = multiprocessing.Value('i', 0)
except FileNotFoundError:
    multiprocessing = None

def write_into_channel(channel, value, value2):
    if value2 is not None:
        channel.write(value, value2)
    else:
        channel.write(value)

class TestChannelCondition(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def write_mp(self, channel, value, value2=None):
        p = Process(target=write_into_channel, args=[channel, value, value2])
        p.start()
        p.join()

    def do_write(self, channel, value, value2=None, expect=True):
        logger.info("Writing -> {}{}".format(value,
            " - " + str(value2) if value2 is not None else ""))
        if channel.is_multiprocess():
            self.write_mp(channel, value, value2)
        else:
            if value2 is not None:
                self.assertTrue(channel.write(value, value2) == expect)
            else:
                self.assertTrue(channel.write(value) == expect)

    """ Channel Condition """

    """ Process """

    def condition_worker_step(self):
        n = self.worker.get_number()
        logger.info("Worker {} reading condition".format(n))
        ret = self.cond.read()
        logger.info("Worker {} passed condition -> {}".format(n, ret))
        self.bool.write(ret)

    def do_condition(self, channel, timeout=False):
        logger.info("Testing " + str(channel))
        if channel.is_multiprocess():
            self.do_multiprocess(channel, timeout)
        else:
            self.do_thread(channel, timeout)

    def worker_started(self, worker, *args):
        self.worker = worker
        self.cond, self.bool = args

    def do_multiprocess(self, channel, timeout=False):
        kwargs = {
            "frequency": 10,
            "timeout": 5,
            "max_iter": 1,
            "worker_number": 2,
            "work": self.condition_worker_step,
            "on_start": self.worker_started,
        }
        self.passed = {}
        self.channel_cond = channel
        bool1 = ChannelBool(mp=True, default=False)
        bool2 = ChannelBool(mp=True, default=False)
        bool3 = ChannelBool(mp=True, default=False)
        worker1 = SihdWorker(args=(channel, bool1,), **kwargs)
        worker2 = SihdWorker(args=(channel, bool2,), **kwargs)
        worker3 = SihdWorker(args=(channel, bool3,), **kwargs)
        worker1.start_workers() 
        worker2.start_workers() 
        worker3.start_workers()
        time.sleep(3)
        self.assertFalse(bool1.read())
        self.assertFalse(bool2.read())
        self.assertFalse(bool3.read())
        if timeout is False:
            channel.write()
            time.sleep(0.01)
        self.assertEqual(bool1.read(), timeout is False)
        self.assertEqual(bool2.read(), timeout is False)
        self.assertEqual(bool3.read(), timeout is False)
        time.sleep(2)
        worker1.clear_workers()
        worker2.clear_workers()
        worker3.clear_workers()

    """ Thread """

    def condition_thread_step(self):
        ident = threading.current_thread().ident
        self.passed[ident] = False
        logger.info("Thread {} reading condition".format(ident))
        ret = self.cond.read()
        self.passed[ident] = ret
        logger.info("Thread {} passed condition -> {}".format(ident, ret))

    def thread_started(self, thread, *args):
        self.cond = args[0]

    def do_thread(self, channel, timeout=False):
        kwargs = {
            "frequency": 10,
            "timeout": 5,
            "max_iter": 1,
            "daemon": True,
            "step": self.condition_thread_step,
            "on_start": self.thread_started,
            "args": (channel,)
        }
        self.passed = {}
        thread1 = SihdThread(self, **kwargs)
        thread2 = SihdThread(self, **kwargs)
        thread3 = SihdThread(self, **kwargs)
        thread1.start()
        thread2.start() 
        thread3.start()
        time.sleep(3)
        for key, value in self.passed.items():
            self.assertFalse(value)
        if timeout is False:
            channel.write()
            time.sleep(0.01)
        for key, value in self.passed.items():
            self.assertEqual(value, timeout is False)
        time.sleep(2)
        thread1.stop()
        thread2.stop()
        thread3.stop()

    def test_channel_condition(self):
        print()
        channel = ChannelCondition()
        self.do_condition(channel)
        channel = ChannelCondition(timeout=2.)
        self.do_condition(channel, timeout=True)
        
        if multiprocessing:
            print()
            channel = ChannelCondition(mp=True)
            self.do_condition(channel)
            channel = ChannelCondition(mp=True, timeout=2.)
            self.do_condition(channel, timeout=True)

if __name__ == '__main__':
    unittest.main(verbosity=2)
