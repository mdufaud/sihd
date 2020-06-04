#!/usr/bin/python
#coding: utf-8

""" System """
import time
import unittest

import utils
import sihd
logger = sihd.set_log('info')

from sihd.Handlers.AHandler import AHandler
from sihd.Core.Channel import *
from sihd.Core import RunnableThread
from sihd.Core import RunnableProcess
from sihd.Core import SihdService
from sihd.Core import SihdRunnableService

try:
    import multiprocessing
except ImportError:
    multiprocessing = None

try:
    if multiprocessing is not None:
        #checks for /dev/shm
        val = multiprocessing.Value('i', 0)
except FileNotFoundError:
    multiprocessing = None

class IncService(SihdRunnableService):

    def __init__(self, name="IncService"):
        super(IncService, self).__init__(name=name)
        self.i = 0
        self.add_channel("output", type='int', default=0)

    def on_step(self):
        self.i += 1
        self.output.write(self.i)
        return True

class PollingService(SihdRunnableService):

    def __init__(self, name="PollingService"):
        super(PollingService, self).__init__(name=name)
        self.add_channel_input("input", type='int')
        self.add_channel("output", type='int')

    def handle(self, channel):
        val = channel.read()
        self.output.write(val)
        return True

class TestServices(unittest.TestCase):

    def setUp(self):
        sihd.clear_tree()
        print()

    def tearDown(self):
        pass

    def step(self):
        logger.info("Step")

    def on_thread_stop(self, thread, iteration):
        self.__iter = iteration

    def test_runnable(self):
        self.__iter = 0
        runnable = RunnableThread(
            step=self.step,
            frequency=50,
            timeout=5,
            max_iter=10,
            on_stop=self.on_thread_stop,
            daemon=True,
        )
        runnable.start()
        time.sleep(1)
        self.assertEqual(self.__iter, 10)
        runnable.pause()
        runnable.resume()
        runnable.stop()
    
    def do_life_cycle(self, service, thread=False, process=False):
        self.assertTrue(service.setup())
        if thread or process:
            self.assertEqual(service.is_service_threading(), thread)
            self.assertEqual(service.is_service_multiprocessing(), process)
        self.assertTrue(service.start())
        self.assertTrue(service.pause())
        time.sleep(0.3)
        self.assertTrue(service.resume())
        self.assertTrue(service.stop())
        self.assertTrue(service.reset())

    def test_iservice(self):
        service = sihd.Core.SihdService("service")
        self.do_life_cycle(service)
        service = sihd.Core.SihdRunnableService("runnable_service")
        self.do_life_cycle(service, thread=True)
        if multiprocessing:
            service = sihd.Core.SihdRunnableService("mp_runnable_service")
            service.set_conf("runnable_type", "process")
            self.do_life_cycle(service, process=True)

    def test_thread_in_process(self):
        #Thread 1
        inc = IncService("inc1")
        #Thread 2
        inc2 = IncService("inc2")
        #Proc 1
        poll = PollingService("poll")
        self.assertTrue(inc.setup())
        self.assertTrue(inc2.setup())
        self.assertTrue(poll.setup())
        #linking thread output to process input
        inc.link("output", poll.input)
        #Start thread 1
        self.assertTrue(inc.start())
        time.sleep(0.1)
        #Start new proc
        self.assertTrue(poll.start())
        #Stop thread 1
        self.assertTrue(inc.stop())
        #Start thread 2
        self.assertTrue(inc2.start())
        time.sleep(0.4)
        #Stop proc
        self.assertTrue(poll.stop())
        #Stop thread 2
        self.assertTrue(inc2.stop())
        poll_val = poll.output.read()
        inc_val = inc.output.read()
        inc2_val = inc2.output.read()
        logger.info("Poll: {} - Inc: {} - Inc2: {}".format(poll_val, inc_val, inc2_val))
        self.assertEqual(inc_val, poll_val)
        self.assertTrue(inc2_val > 0)

if __name__ == '__main__':
    unittest.main(verbosity=2)
