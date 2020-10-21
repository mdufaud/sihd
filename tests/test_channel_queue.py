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

from sihd.handlers.AHandler import AHandler
from sihd.core.Channel import *

class TestChannelQueue(unittest.TestCase):

    def setUp(self):
        print()

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

    """ Channel Queue """

    def queue_do_list(self, channel, lst):
        for el in lst:
            self.assertTrue(utils.write_channel(channel, el))
        for el in lst:
            self.assertEqual(el, channel.read())

    def do_queue(self, channel, default, simple=False):
        logger.info("Testing " + str(channel))
        self.assertEqual(channel.read(), default)
        self.assertFalse(channel.is_readable())
        self.assertTrue(channel.read() == None)
        if not simple:
            #no timeout on simple queues
            self.assertTrue(channel.write('stuff1'))
            self.assertTrue(channel.write('stuff2'))
            self.assertEqual(channel.read(1), 'stuff1')
            self.assertEqual(channel.read(1), 'stuff2')
        self.queue_do_list(channel, ['one', 'two', 'yeeeeeee'])
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, 'smth'))
        self.assertTrue(channel.is_readable())
        channel.clear()
        self.assertFalse(channel.is_readable())
        self.assertTrue(channel.read() == None)
  
    def test_channel_queue(self):
        print()
        default = {"some": "datastruct"}
        channel = ChannelQueue(default=default)
        self.do_queue(channel, default)

        if utils.is_multiprocessing():
            print()
            default = ['my', 'bad']
            channel = ChannelQueue(default=default, mp=True, simple=True)
            self.do_queue(channel, default, simple=True)

            print()
            default = ('hello', 'darkness', 'my', 'man', {"hey": "sup"})
            channel = ChannelQueue(default=default, mp=True)
            time.sleep(0.1)
            self.do_queue(channel, default)

    def do_lifo_queue(self, channel, default):
        #Last in first out
        logger.info("Testing " + str(channel))
        self.assertEqual(channel.read(), default)
        self.assertFalse(channel.is_readable())
        self.assertTrue(channel.read() == None)
        self.assertTrue(channel.write('data'))
        self.assertTrue(channel.write('item'))
        self.assertEqual(channel.read(1), 'item')
        self.assertEqual(channel.read(1), 'data')
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, 'smth'))
        self.assertTrue(channel.is_readable())
        channel.clear()
        self.assertFalse(channel.is_readable())
        self.assertTrue(channel.read() == None)

    def test_channel_lifo_queue(self):
        print()
        default = {"some": "datastruct"}
        channel = ChannelQueue(default=default, lifo=True)
        self.do_lifo_queue(channel, default)

        if utils.is_multiprocessing():
            print()
            default = ('hello', 'darkness', 'my', 'man', {"hey": "sup"})
            channel = ChannelQueue(default=default, mp=True, lifo=True)
            time.sleep(0.1)
            self.do_lifo_queue(channel, default)

    def do_priority_queue(self, channel, default):
        # Priority queue goes write((PRIORITY_NUM, DATA))
        # and sorts queue by priority
        logger.info("Testing " + str(channel))
        self.assertEqual(channel.read(), default)
        self.assertFalse(channel.is_readable())
        self.assertTrue(channel.read() == None)
        self.assertTrue(channel.write((5, 'data')))
        self.assertTrue(channel.write((1, 'item')))
        self.assertTrue(channel.write((3, 'last')))
        self.assertEqual(channel.read(1)[1], 'item')
        self.assertEqual(channel.read(1)[1], 'last')
        self.assertEqual(channel.read(1)[1], 'data')
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, (100, 'smth')))
        self.assertTrue(channel.is_readable())
        channel.clear()
        self.assertFalse(channel.is_readable())
        self.assertTrue(channel.read() == None)

    def test_channel_priority_queue(self):
        print()
        default = {"some": "datastruct"}
        channel = ChannelQueue(default=default, priority=True)
        self.do_priority_queue(channel, default)

        if utils.is_multiprocessing():
            print()
            default = ('hello', 'darkness', 'my', 'man', {"hey": "sup"})
            channel = ChannelQueue(default=default, mp=True, priority=True)
            time.sleep(0.1)
            self.do_priority_queue(channel, default)

if __name__ == '__main__':
    unittest.main(verbosity=2)
