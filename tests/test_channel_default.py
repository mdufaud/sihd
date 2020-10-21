#!/usr/bin/python
# coding: utf-8

""" System """
import time
import unittest

import utils
import sihd
logger = sihd.log.setup('info')

from sihd.core.Channel import *
from sihd.core import RunnableThread
from sihd.core import RunnableProcess

class TestChannelDefault(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    """ Channel Condition """

    """ Process """

    def worker_step(self):
        n = self.worker.get_number()
        logger.info("Worker {} reading condition".format(n))
        ret = self.chan.wait(5)
        logger.info("Worker {} passed condition -> {}".format(n, ret))
        self.bool.write(ret)

    def worker_started(self, worker):
        self.worker = worker
        self.chan, self.bool = worker.get_args()

    def do_mp(self, channel, timeout=False):
        logger.info("Testing " + str(channel))
        kwargs = {
            "frequency": 10,
            "timeout": 5,
            "max_iter": 1,
            "worker_number": 2,
            "daemon": True,
            "step": self.worker_step,
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
        time.sleep(0.1)
        self.assertFalse(bool1.read())
        self.assertFalse(bool2.read())
        self.assertFalse(bool3.read())
        if timeout is False:
            channel.write(1)
            time.sleep(0.01)
        self.assertEqual(bool1.read(), timeout is False)
        self.assertEqual(bool2.read(), timeout is False)
        self.assertEqual(bool3.read(), timeout is False)
        worker1.stop()
        worker2.stop()
        worker3.stop()

    """ Thread """

    def thread_step(self):
        ident = self.thread.get_id()
        self.passed[ident] = False
        logger.info("Thread {} reading condition".format(ident))
        ret = self.chan.wait(5)
        self.passed[ident] = ret
        logger.info("Thread {} passed condition -> {}".format(ident, ret))

    def thread_started(self, thread):
        self.thread = thread
        self.chan = thread.get_args()[0]

    def do_thread(self, channel, timeout=False):
        logger.info("Testing " + str(channel))
        kwargs = {
            "frequency": 10,
            "timeout": 5,
            "max_iter": 1,
            "daemon": True,
            "step": self.thread_step,
            "on_start": self.thread_started,
            "args": (channel,)
        }
        self.passed = {}
        thread1 = RunnableThread(**kwargs)
        thread2 = RunnableThread(**kwargs)
        thread3 = RunnableThread(**kwargs)
        thread1.start()
        thread2.start() 
        thread3.start()
        time.sleep(0.1)
        for key, value in self.passed.items():
            self.assertFalse(value)
        if timeout is False:
            channel.write(1)
            time.sleep(0.01)
        for key, value in self.passed.items():
            self.assertEqual(value, timeout is False)
        thread1.stop()
        thread2.stop()
        thread3.stop()

    def test_channel_wait(self):
        print()
        channel = Channel()
        self.do_thread(channel)
        
        if utils.is_multiprocessing():
            print()
            channel = Channel(mp=True)
            self.do_mp(channel)

    def test_channel_notify(self):
        c1 = Channel(default='slt')
        c2 = Channel(default=0)
        self.assertEqual(c1.read(), 'slt')
        self.assertEqual(c2.read(), 0)
        c1.add_observer(c2)
        self.assertTrue(c1.write('somedata'))
        self.assertEqual(c2.read(), 'somedata')

    def test_channel_lock(self):
        channel = Channel(default=4, timeout=0.001)
        self.assertEqual(channel.read(), 4)
        self.assertTrue(channel.lock())
        self.assertFalse(channel.write('pls'))
        self.assertFalse(channel.write({'d': 'dd'}))
        self.assertFalse(channel.write(888))
        self.assertTrue(channel.unlock())
        self.assertEqual(channel.read(), 4)
        self.assertTrue(channel.write(5))
        self.assertEqual(channel.read(), 5)

    def test_channel_filter(self):
        channel = Channel(lfilter=lambda x: x < 40)
        self.assertTrue(channel.write(10))
        self.assertEqual(channel.read(), 10)
        self.assertTrue(channel.write(40))
        self.assertEqual(channel.read(), 10)
        self.assertTrue(channel.write(1000))
        self.assertEqual(channel.read(), 10)
        self.assertTrue(channel.write(-41234))
        self.assertEqual(channel.read(), -41234)

if __name__ == '__main__':
    unittest.main(verbosity=2)
