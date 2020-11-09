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
from sihd.core.SihdRunnableObject import SihdRunnableObject

class TestRunnable(SihdRunnableObject):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.configuration.set("runnable_frequency", 100)
        self.add_channel_input("input")
        self.add_channel("a_channel")
        self.add_channel("output", type='counter')

    def handle(self, channel):
        data = channel.read()
        self.a_channel.write(data)

    def on_step(self):
        self.output.write()

class TestScheduler(unittest.TestCase):

    def setUp(self):
        self.i1 = 0
        self.i2 = 0
        self.i3 = 0
        self.arg1 = None
        self.arg2 = None
        self.evtlst = []
        print()
        sihd.tree.clear()

    def tearDown(self):
        time.sleep(0.001)
        pass

    def step1(self):
        self.i1 += 1
        self.evtlst.append(1)

    def step2(self, arg1, key="Nope"):
        self.i2 += 1
        self.arg1 = arg1
        self.arg2 = key
        self.evtlst.append(2)

    def step3(self):
        time.sleep(0.001)
        self.i3 += 1
        self.evtlst.append(3)

    def test_scheduler_sihd_object(self):
        obj = TestRunnable("obj")
        sched = Scheduler("sched", thread=True)
        sched.schedule_sihd_obj(obj)
        obj.start()
        sched.start()
        time.sleep(0.1)
        self.assertEqual(obj.output.read(), 9)
        obj.input.write("test")
        # Wait for last data to be written
        obj.output.consumed_data()
        obj.output.wait(1)
        sched.stop()
        self.assertEqual(obj.output.read(), 10)
        self.assertEqual(obj.a_channel.read(), "test")

    def test_scheduler_timer(self):
        sched = Scheduler("sched", timeout=0.11)
        sched.schedule_once(self.step2, 0, "test", key="value")
        now = time.time()
        sched.plan.ms(1)
        sched.start()
        time.sleep(0.002)
        sched.stop()
        self.assertEqual(self.i2, 1)
        self.assertEqual(self.arg1, "test")
        self.assertEqual(self.arg2, "value")

    def test_scheduler_tasks_thread(self):
        sched = Scheduler("sched", thread=True, timeout=1)
        now = time.time()
        sched.schedule_once(self.step3, now + 1)
        sched.schedule_once(self.step3, now + 0.5)
        sched.schedule_once(self.step3, now + 0.1)
        sched.schedule_period(self.step1, 0.01)
        sched.schedule_period(self.step1, 0.2)
        sched.schedule_frequency(self.step1, 1)
        sched.start()
        time.sleep(0.11)
        sched.stop()
        self.assertEqual(self.i1, 10)
        self.assertEqual(self.i3, 1)

    def test_scheduler_once_thread(self):
        sched = Scheduler("sched", thread=True, timeout=0.11)
        now = time.time()
        sched.schedule_once(self.step2, now + 0.01, "test", key="value")
        sched.start()
        time.sleep(0.1)
        sched.stop()
        self.assertEqual(self.i2, 1)
        self.assertEqual(self.arg1, "test")
        self.assertEqual(self.arg2, "value")

    def test_scheduler_frequency_thread(self):
        sched = Scheduler("sched", thread=True)
        sched.schedule_frequency(self.step1, 100)
        sched.start()
        time.sleep(0.11)
        sched.stop()
        self.assertEqual(self.i1, 10)

    def test_scheduler_period_thread(self):
        sched = Scheduler("sched", timeout=0.11)
        sched.thread = True
        sched.schedule_period(self.step1, 0.01)
        sched.schedule_period(self.step2, 0.1, "test", key="value")
        before = time.time()
        sched.start()
        after = time.time()
        diff = after - before
        logger.info("Time spent in scheduler: " + str(diff))
        self.assertTrue(diff < 0.01)
        time.sleep(0.11)
        sched.stop()
        self.assertEqual(self.i1, 10)
        self.assertEqual(self.i2, 1)
        self.assertEqual(self.arg1, "test")
        self.assertEqual(self.arg2, "value")

    def test_scheduler_period(self):
        sched = Scheduler("sched", timeout=0.11)
        sched.schedule_period(self.step1, 0.01)
        sched.schedule_period(self.step2, 0.1, "test", key="value")
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
        sched.stop()

if __name__ == '__main__':
    unittest.main(verbosity=2)
