#!/usr/bin/python
#coding: utf-8

""" System """
import time
import unittest

import utils
import sihd
logger = sihd.set_log('debug')

from sihd.Handlers.AHandler import AHandler
from sihd.Core.Channel import *
from sihd.Core import RunnableThread
from sihd.Core import RunnableProcess

class TestChannelCondition(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

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

    def worker_started(self, worker):
        self.worker = worker
        self.cond, self.bool = worker.get_args()

    def do_multiprocess(self, channel, timeout=False):
        kwargs = {
            "frequency": 10,
            "timeout": 5,
            "max_iter": 1,
            "worker_number": 2,
            "daemon": True,
            "step": self.condition_worker_step,
            "on_start": self.worker_started,
        }
        self.passed = {}
        self.channel_cond = channel
        bool1 = ChannelBool(mp=True, default=False)
        bool2 = ChannelBool(mp=True, default=False)
        bool3 = ChannelBool(mp=True, default=False)
        worker1 = RunnableProcess(args=(channel, bool1,), **kwargs)
        worker2 = RunnableProcess(args=(channel, bool2,), **kwargs)
        worker3 = RunnableProcess(args=(channel, bool3,), **kwargs)
        worker1.start() 
        worker2.start() 
        worker3.start()
        time.sleep(1)
        self.assertFalse(bool1.read())
        self.assertFalse(bool2.read())
        self.assertFalse(bool3.read())
        if timeout is False:
            channel.write()
            time.sleep(0.01)
        self.assertEqual(bool1.read(), timeout is False)
        self.assertEqual(bool2.read(), timeout is False)
        self.assertEqual(bool3.read(), timeout is False)
        worker1.stop()
        worker2.stop()
        worker3.stop()

    """ Thread """

    def condition_thread_step(self):
        ident = self.thread.get_id()
        self.passed[ident] = False
        logger.info("Thread {} reading condition".format(ident))
        ret = self.cond.read()
        self.passed[ident] = ret
        logger.info("Thread {} passed condition -> {}".format(ident, ret))

    def thread_started(self, thread):
        self.thread = thread
        self.cond = thread.get_args()[0]

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
        thread1 = RunnableThread(self, **kwargs)
        thread2 = RunnableThread(self, **kwargs)
        thread3 = RunnableThread(self, **kwargs)
        thread1.start()
        thread2.start() 
        thread3.start()
        time.sleep(1)
        for key, value in self.passed.items():
            self.assertFalse(value)
        if timeout is False:
            channel.write()
            time.sleep(0.01)
        for key, value in self.passed.items():
            self.assertEqual(value, timeout is False)
        thread1.stop()
        thread2.stop()
        thread3.stop()

    def test_channel_condition(self):
        print()
        channel = ChannelCondition(timeout=None, block=False)
        self.do_condition(channel)
        channel = ChannelCondition(timeout=0.5)
        self.do_condition(channel, timeout=True)
        
        if utils.is_multiprocessing():
            print()
            channel = ChannelCondition(mp=True, timeout=None, block=False)
            self.do_condition(channel)
            channel = ChannelCondition(mp=True, timeout=0.5)
            self.do_condition(channel, timeout=True)

if __name__ == '__main__':
    unittest.main(verbosity=2)
