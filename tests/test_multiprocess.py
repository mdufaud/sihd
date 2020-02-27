#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os
import sys

import test_utils
import sihd
import unittest

try:
    import multiprocessing
    import queue
except ImportError:
    multiprocessing = None
    queue = None

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

from sihd.srcs.Readers.IReader import IReader
from sihd.srcs.Handlers.IHandler import IHandler
from sihd.srcs.Core.IThreadedService import IThreadedService
from sihd.srcs.Core.IConsumer import IConsumer
from sihd.srcs.Core.IProcessedService import IProcessedService

def do_process():
    time.sleep(0.01)

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self.set_step_method(self.consume)
        self.n = 0

    def handle(self, service, line):
        do_process()
        self.n += 1
        #self.log_info("{}: {}".format(service, line))
        return True

class ProcessHandler(IHandler):

    def __init__(self, name="ProcessHandler"):
        super(ProcessHandler, self).__init__(name=name)

    def handle(self, service, line):
        do_process()
        self.deliver(line)
        #self.log_info("{}: {}".format(service, line))
        return True

class InfiniteReader(IReader):

    def __init__(self, data="-- Infinite Data ! --", name="InfiniteReader"):
        super(InfiniteReader, self).__init__(name=name)
        self._set_default_conf({"thread_frequency": 200})
        self.data = data 

    def step_method(self):
        self.deliver(self.data)
        return True

class TestMultiprocess(unittest.TestCase):

    def setUp(self):
        self.sleep = 2

    def tearDown(self):
        pass

    def __get_total(self, handler, check=True):
        q = handler.get_producing_queue()
        i = 0
        while True:
            try:
                d = q.get(False)
            except queue.Empty:
                break
            except OSError:
                return None
            i += 1
        if check is not False:
            self.assertTrue(i > 0)
        return i

    @unittest.skipIf(multiprocessing is None, "No support for multiprocess")
    def test_reader_workers(self):
        print()
        logger.info("Starting multiple readers and multiprocess 1 worker 4 processes")
        reader1 = InfiniteReader("hello", "InfiniteReader1")
        reader2 = InfiniteReader("world", "InfiniteReader2")
        reader1.set_service_multiprocess()
        reader2.set_service_multiprocess()
        handler = ProcessHandler()
        handler.set_service_multiprocess()
        handler.set_conf("process_workers", 4)
        handler.add_to_consume(reader1)
        handler.add_to_consume(reader2)
        self.assertTrue(handler.setup())
        self.assertTrue(reader1.setup())
        self.assertTrue(reader2.setup())
        self.assertTrue(handler.start())
        self.assertTrue(reader1.start())
        self.assertTrue(reader2.start())
        time.sleep(self.sleep)
        self.assertTrue(reader1.stop())
        self.assertTrue(reader2.stop())
        logger.info("Total processed: {}".format(self.__get_total(handler)))
        self.assertTrue(handler.stop())

    @unittest.skipIf(multiprocessing is None, "No support for multiprocess")
    def test_workers(self):
        print()
        logger.info("Starting multiprocess 3 workers 1 process")
        reader = InfiniteReader()
        reader.set_service_multiprocess()
        handler1 = ProcessHandler(name="Handler1")
        handler2 = ProcessHandler(name="Handler2")
        handler3 = ProcessHandler(name="Handler3")

        handler1.set_service_multiprocess()
        handler2.set_service_multiprocess()
        handler3.set_service_multiprocess()

        handler1.add_to_consume(reader)
        handler2.add_to_consume(reader)
        handler3.add_to_consume(reader)

        self.assertTrue(handler1.setup())
        self.assertTrue(handler2.setup())
        self.assertTrue(handler3.setup())
        self.assertTrue(reader.setup())

        self.assertTrue(handler1.start())
        self.assertTrue(handler2.start())
        self.assertTrue(handler3.start())
        self.assertTrue(reader.start())
        time.sleep(self.sleep)
        self.assertTrue(reader.stop())
        logger.info("Total processed: {}".format(
            self.__get_total(handler1)
            + self.__get_total(handler2)
            + self.__get_total(handler3)))
        self.assertTrue(handler1.stop())
        self.assertTrue(handler2.stop())
        self.assertTrue(handler3.stop())

    @unittest.skipIf(multiprocessing is None, "No support for multiprocess")
    def test_worker(self):
        print()
        logger.info("Starting multiprocess 1 worker 3 processes")
        reader = InfiniteReader()
        reader.set_service_multiprocess()
        handler = ProcessHandler()
        handler.set_service_multiprocess()
        handler.set_conf("process_workers", 3)
        handler.add_to_consume(reader)
        self.assertTrue(handler.setup())
        self.assertTrue(reader.setup())
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(self.sleep)
        self.assertTrue(reader.stop())
        logger.info("Total processed: {}".format(self.__get_total(handler)))
        self.assertTrue(handler.stop())

    @unittest.skipIf(multiprocessing is None, "No support for multiprocess")
    def test_base(self):
        print()
        logger.info("Starting no multiprocess")
        reader = InfiniteReader()
        handler = TestHandler()
        handler.set_service_threading()
        reader.add_observer(handler)
        self.assertTrue(reader.setup())
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(self.sleep)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        logger.info("Total processed: {}".format(handler.n))

    @unittest.skipIf(multiprocessing is None, "No support for multiprocess")
    def test_workers_life_cycle(self):
        print()
        logger.info("Starting multiprocess 1 worker 3 processes")
        reader = InfiniteReader()
        reader.set_service_multiprocess()
        handler = ProcessHandler()
        handler.set_service_multiprocess()
        handler.set_conf("process_workers", 3)
        handler.add_to_consume(reader)
        self.assertTrue(handler.setup())
        self.assertTrue(reader.setup())

        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(self.sleep / 2)

        self.assertTrue(handler.pause())
        consumed = self.__get_total(handler)
        time.sleep(self.sleep / 2)
        self.assertEqual(self.__get_total(handler, False), 0)
        self.assertTrue(handler.resume())
        time.sleep(self.sleep / 2)

        consumed = self.__get_total(handler)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        self.assertEqual(self.__get_total(handler), None)

if __name__ == '__main__':
    unittest.main(verbosity=2)
