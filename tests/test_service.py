#!/usr/bin/python
#coding: utf-8

""" System """
import time
import unittest

import utils
import sihd
logger = sihd.set_log('debug')

from sihd.Handlers.IHandler import IHandler
from sihd.Core.Channel import *
from sihd.Core import SihdThread
from sihd.Core import SihdWorker

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

class TestServices(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    def step(self):
        logger.info("Step")

    def on_thread_stop(self, thread, iteration):
        self.__iter = iteration

    def test_runnable(self):
        self.__iter = 0
        runnable = sihd.Core.IRunnable()
        runnable.setup_thread(
            step=self.step,
            frequency=50,
            timeout=5,
            max_iter=10,
            on_stop=self.on_thread_stop,
        )
        runnable.start_thread()
        time.sleep(1)
        self.assertEqual(self.__iter, 10)
    
    def do_life_cycle(self, service):
        self.assertTrue(service.setup())
        self.assertTrue(service.start())
        self.assertTrue(service.pause())
        time.sleep(0.3)
        self.assertTrue(service.resume())
        self.assertTrue(service.stop())
        self.assertTrue(service.reset())

    def test_iservice(self):
        service = sihd.Core.IService()
        self.do_life_cycle(service)
        service = sihd.Core.IThreadedService()
        self.do_life_cycle(service)
        if multiprocessing:
            service = sihd.Core.IProcessedService()
            self.do_life_cycle(service)
        service = sihd.Core.IPolyService()
        self.do_life_cycle(service)

if __name__ == '__main__':
    unittest.main(verbosity=2)
