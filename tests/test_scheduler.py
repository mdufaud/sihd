#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import time
import socket
import unittest

import utils
import sihd
logger = sihd.log.setup('info')

from sihd.core import Scheduler

class TestReader(unittest.TestCase):

    def setUp(self):
        self.i1 = 0
        self.i2 = 0
        self.arg1 = None
        self.arg2 = None
        print()
        sihd.tree.clear()

    def tearDown(self):
        pass

    def step2(self, arg1, key="Nope"):
        self.i2 += 1
        self.arg1 = arg1
        self.arg2 = key

    def step1(self):
        self.i1 += 1

    def make_sched(self):
        sched = Scheduler("sched")
        sched.set(timeout=0.11, maxsteps=5)
        sched.add(self.step1, 0.01)
        sched.add(self.step2, 0.1, "test", key="value")
        return sched

    def test_sched_thread(self):
        sched = self.make_sched()
        sched.thread = True
        before = time.time()
        sched.start()
        after = time.time()
        diff = after - before
        logger.info("Time spent in scheduler: " + str(diff))
        self.assertTrue(diff < 0.01)
        time.sleep(0.11)
        self.assertEqual(self.i1, 10)
        self.assertEqual(self.i2, 1)
        self.assertEqual(self.arg1, "test")
        self.assertEqual(self.arg2, "value")

    def test_scheduler(self):
        sched = self.make_sched()
        before = time.time()
        sched.start()
        after = time.time()
        diff = after - before
        logger.info("Time spent in scheduler: " + str(diff))
        self.assertTrue(diff < 0.11 and diff > 0.09)
        self.assertEqual(self.i1, 10)
        self.assertEqual(self.i2, 1)
        self.assertEqual(self.arg1, "test")
        self.assertEqual(self.arg2, "value")

if __name__ == '__main__':
    unittest.main(verbosity=2)
