#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os
import sys
import test_utils
import unittest

try:
    import multiprocessing
    import queue
except ImportError:
    multiprocessing = None
    queue = None

try:
    if multiprocessing is not None:
        #checks for /dev/shm
        val = multiprocessing.Value('i', 0)
except FileNotFoundError:
    multiprocessing = None

import sihd
logger = sihd.set_log()

from sihd.Readers.IReader import IReader
from sihd.Handlers.IHandler import IHandler
from sihd.Core.IThreadedService import IThreadedService
from sihd.Core.IConsumer import IConsumer
from sihd.Core.IProcessedService import IProcessedService

sihd.set_log_color(True)

def do_process():
    time.sleep(0.01)

class TestHandler(IHandler):

    def __init__(self, app=None, name="TestHandler"):
        super(TestHandler, self).__init__(app=app, name=name)
        self.n = 0
        self.once = False
        self._set_default_conf({"thread_frequency": 200})
        self._set_default_conf({"process_frequency": 200})
        self.add_channel_input('input', type='queue')
        self.add_channel_output('output', type='queue')

    def on_setup(self):
        s = "processed" if self.is_service_multiprocess() else "threaded"
        self.log_info("Has setup as " + s)
        return True
    
    def handle(self, channel):
        line = channel.read()
        if line is None:
            return True
        do_process()
        self.output.write(line)
        self.n += 1
        """
        if self.once == False:
            self.log_warning("Handle: PID={}".format(os.getpid()))
            self.once = True
            self.log_info("{}: {}".format(channel.get_name(), line))
        """
        return True

class InfiniteReader(IReader):

    def __init__(self, data="-- Infinite Data ! --", name="InfiniteReader"):
        super(InfiniteReader, self).__init__(name=name)
        self._set_default_conf({"thread_frequency": 200})
        self._set_default_conf({"process_frequency": 200})
        self.add_channel_output('output', type='queue')
        self.data = data 

    def do_step(self):
        self.output.write(self.data)
        return True

class TestMultiprocess(unittest.TestCase):

    def setUp(self):
        self.sleep = 2

    def tearDown(self):
        pass

    def __get_total(self, handler, check=True):
        q = handler.output
        i = 0
        while True:
            d = q.read()
            if d is None:
                break
            """
            except OSError:
                return None
            """
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
        reader1.set_conf("service_type", "process")
        reader2.set_conf("service_type", "process")
        handler = TestHandler()
        handler.set_conf("service_type", "process")
        handler.set_conf("process_workers", 4)
        self.assertTrue(handler.setup())
        self.assertTrue(reader1.setup())
        self.assertTrue(reader2.setup())

        reader1.output.add_observer(handler.input)
        reader2.output.add_observer(handler.input)

        self.assertTrue(handler.start())
        self.assertTrue(reader1.start())
        self.assertTrue(reader2.start())

        time.sleep(self.sleep)
        self.assertTrue(reader1.stop())
        self.assertTrue(reader2.stop())
        logger.info("======> Total processed: {}".format(self.__get_total(handler)))
        self.assertTrue(handler.stop())

    @unittest.skipIf(multiprocessing is None, "No support for multiprocess")
    def test_workers(self):
        print()
        logger.info("Starting multiprocess 3 workers 1 process")
        reader = InfiniteReader()
        reader.set_conf("service_type", "process")
        handler1 = TestHandler(name="Handler1")
        handler2 = TestHandler(name="Handler2")
        handler3 = TestHandler(name="Handler3")

        handler1.set_conf("service_type", "process")
        handler2.set_conf("service_type", "process")
        handler3.set_conf("service_type", "process")

        self.assertTrue(handler1.setup())
        self.assertTrue(handler2.setup())
        self.assertTrue(handler3.setup())
        self.assertTrue(reader.setup())

        reader.output.add_observer(handler1.input)
        reader.output.add_observer(handler2.input)
        reader.output.add_observer(handler3.input)

        self.assertTrue(handler1.start())
        self.assertTrue(handler2.start())
        self.assertTrue(handler3.start())
        self.assertTrue(reader.start())
        time.sleep(self.sleep)
        self.assertTrue(reader.stop())
        logger.info("======> Total processed: {}".format(
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
        reader.set_conf("service_type", "process")
        handler = TestHandler()
        handler.set_conf("service_type", "process")
        handler.set_conf("process_workers", 3)
        self.assertTrue(handler.setup())
        self.assertTrue(reader.setup())
        reader.output.add_observer(handler.input)
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(self.sleep)
        self.assertTrue(reader.stop())
        logger.info("======> Total processed: {}".format(self.__get_total(handler)))
        self.assertTrue(handler.stop())

    @unittest.skipIf(multiprocessing is None, "No support for multiprocess")
    def test_base(self):
        print()
        logger.info("Starting no multiprocess")
        reader = InfiniteReader()
        handler = TestHandler()
        handler.set_conf("service_type", "thread")
        self.assertTrue(reader.setup())
        self.assertTrue(handler.setup())
        reader.output.add_observer(handler.input)
        self.assertTrue(handler.start())
        self.assertTrue(reader.start())
        time.sleep(self.sleep)
        self.assertTrue(reader.stop())
        self.assertTrue(handler.stop())
        logger.info("======> Total processed: {}".format(handler.n))
        self.assertTrue(handler.n > 0)

    @unittest.skipIf(multiprocessing is None, "No support for multiprocess")
    def test_workers_life_cycle(self):
        print()
        logger.info("Starting multiprocess 1 worker 3 processes")
        reader = InfiniteReader()
        reader.set_conf("service_type", "process")
        handler = TestHandler()
        handler.set_conf("service_type", "process")
        handler.set_conf("process_workers", 3)
        self.assertTrue(handler.setup())
        self.assertTrue(reader.setup())

        reader.output.add_observer(handler.input)

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
