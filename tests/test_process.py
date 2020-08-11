#!/usr/bin/python
# coding: utf-8

""" System """
import time
import os
import sys
import unittest

import utils
import sihd
logger = sihd.log.setup()

from sihd.Readers.AReader import AReader
from sihd.Handlers.AHandler import AHandler
from sihd.Core.AConsumer import AConsumer

sihd.log.set_color(True)

def do_process():
    time.sleep(0.001)
    pass

class TestHandler(AHandler):

    def __init__(self, name="TestHandler", app=None):
        super(TestHandler, self).__init__(app=app, name=name)
        self.n = 0
        self.once = False
        self.configuration.set("runnable_frequency", 2000)
        self.add_channel_input('input', type='queue')
        self.add_channel('output', type='queue')

    def post_setup(self):
        s = "processed" if self.is_service_multiprocessing() else "threaded"
        self.log_info("Has setup as " + s)
        return True

    def handle(self, channel):
        line = channel.read()
        if line is None:
            return True
        do_process()
        self.output.write(line)
        self.n += 1
        return True

class InfiniteReader(AReader):

    def __init__(self, data="-- Infinite Data ! --", name="InfiniteReader"):
        super(InfiniteReader, self).__init__(name=name)
        self.configuration.set("runnable_frequency", 8000)
        self.add_channel('output', type='queue')
        self.data = data

    def on_step(self):
        self.output.write(self.data)
        return True

class TestMultiprocess(unittest.TestCase):

    def setUp(self):
        self.begin = time.time()
        self.sleep = 0.5
        print()
        sihd.tree.clear()

    def tearDown(self):
        pass

    def end(self, processed):
        end = time.time()
        total = end - self.begin
        logger.info("Test took {:.3f} s to complete".format(total))
        logger.info("======> Total processed: {} ({} per sec)".format(processed, int(processed/total)))

    def __get_total(self, handler, check=True):
        read = handler.output.read
        i = 0
        while True:
            data = read()
            if data is None:
                break
            i += 1
        if check is not False:
            self.assertTrue(i > 0)
        return i

    @unittest.skipIf(utils.is_multiprocessing() is False, "No support for multiprocess")
    def test_worker_spam_reader(self):
        print()
        logger.info("Starting multiprocess 1 worker 4 processes")
        reader1 = InfiniteReader("hello", "InfiniteReader1")
        reader1.configuration.set("runnable_type", "process")
        reader1.configuration.set("runnable_workers", 1, dynamic=True)
        handler = TestHandler()
        handler.configuration.set("runnable_type", "process")
        handler.configuration.set("runnable_workers", 4, dynamic=True)
        self.assertTrue(handler.setup())
        self.assertTrue(reader1.setup())

        reader1.link("output", handler.input)

        self.assertTrue(reader1.start())
        self.assertTrue(handler.start())
        time.sleep(self.sleep)
        self.assertTrue(handler.stop())
        self.assertTrue(reader1.stop())
        self.end(self.__get_total(handler))

    @unittest.skipIf(utils.is_multiprocessing() is False, "No support for multiprocess")
    def test_mult_sihdworkers(self):
        print()
        logger.info("Starting multiprocess 3 workers 1 process")
        reader = InfiniteReader()
        reader.configuration.set("runnable_type", "process")
        handler1 = TestHandler(name="Handler1")
        handler2 = TestHandler(name="Handler2")
        handler3 = TestHandler(name="Handler3")

        handler1.configuration.set("runnable_type", "process")
        handler2.configuration.set("runnable_type", "process")
        handler3.configuration.set("runnable_type", "process")

        self.assertTrue(handler1.setup())
        self.assertTrue(handler2.setup())
        self.assertTrue(handler3.setup())
        self.assertTrue(reader.setup())

        handler1.link("input", reader.output)
        handler2.link("input", reader.output)
        handler3.link("input", reader.output)

        self.assertTrue(reader.start())
        self.assertTrue(handler1.start())
        self.assertTrue(handler2.start())
        self.assertTrue(handler3.start())
        time.sleep(self.sleep)
        self.assertTrue(handler1.stop())
        self.assertTrue(handler2.stop())
        self.assertTrue(handler3.stop())
        self.assertTrue(reader.stop())
        self.end(self.__get_total(handler1)
                    + self.__get_total(handler2)
                    + self.__get_total(handler3))

    @unittest.skipIf(utils.is_multiprocessing() is False, "No support for multiprocess")
    def test_one_sihdworker(self):
        print()
        logger.info("Starting multiprocess 1 worker 3 processes")
        reader = InfiniteReader()
        reader.configuration.set("runnable_type", "process")
        handler = TestHandler()
        handler.configuration.set("runnable_type", "process")
        handler.configuration.set("runnable_workers", 3, dynamic=True)
        self.assertTrue(handler.setup())
        self.assertTrue(reader.setup())

        handler.link("input", reader.output)

        self.assertTrue(reader.start())
        self.assertTrue(handler.start())
        time.sleep(self.sleep)
        self.assertTrue(handler.stop())
        self.assertTrue(reader.stop())
        self.end(self.__get_total(handler))

    def test_base(self):
        print()
        logger.info("Starting no multiprocess")
        reader = InfiniteReader()
        handler1 = TestHandler(name="Handler1")
        handler2 = TestHandler(name="Handler2")
        handler3 = TestHandler(name="Handler3")
        handler1.configuration.set("runnable_type", "thread")
        handler2.configuration.set("runnable_type", "thread")
        handler3.configuration.set("runnable_type", "thread")
        self.assertTrue(reader.setup())
        self.assertTrue(handler1.setup())
        self.assertTrue(handler2.setup())
        self.assertTrue(handler3.setup())

        handler1.link("input", reader.output)
        handler2.link("input", reader.output)
        handler3.link("input", reader.output)

        self.assertTrue(reader.start())
        self.assertTrue(handler1.start())
        self.assertTrue(handler2.start())
        self.assertTrue(handler3.start())
        time.sleep(self.sleep)
        self.assertTrue(handler1.stop())
        self.assertTrue(handler2.stop())
        self.assertTrue(handler3.stop())
        self.assertTrue(reader.stop())
        self.end(self.__get_total(handler1)
                    + self.__get_total(handler2)
                    + self.__get_total(handler3))

    @unittest.skipIf(utils.is_multiprocessing() is False, "No support for multiprocess")
    def test_workers_life_cycle(self):
        print()
        logger.info("Starting multiprocess 1 worker 3 processes")
        reader = InfiniteReader()
        reader.configuration.set("runnable_type", "thread")
        reader.configuration.set("channels_mp", 1, dynamic=True)
        handler = TestHandler()
        handler.configuration.set("runnable_type", "process")
        handler.configuration.set("runnable_workers", 3, dynamic=True)
        self.assertTrue(handler.setup())
        self.assertTrue(reader.setup())

        handler.link("input", reader.output)

        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(self.sleep / 2)

        self.assertTrue(handler.pause())
        do_process()
        consumed = self.__get_total(handler)
        time.sleep(self.sleep / 2)
        self.assertEqual(self.__get_total(handler, False), 0)
        self.assertTrue(handler.resume())
        time.sleep(self.sleep / 2)

        self.assertTrue(handler.stop())
        consumed = self.__get_total(handler)
        do_process()
        self.assertTrue(reader.stop())
        self.assertEqual(self.__get_total(handler, False), 0)

if __name__ == '__main__':
    unittest.main(verbosity=2)
