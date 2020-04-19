#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import time
import test_utils
import socket
import unittest

import sihd
logger = sihd.set_log('debug')

from sihd.Handlers.IHandler import IHandler
from sihd.Core.Channel import *
from sihd.Core import SihdThread

try:
    import multiprocessing
    from multiprocessing import Process, Manager
except ImportError:
    pass

try:
    if multiprocessing is not None:
        #checks for /dev/shm
        val = multiprocessing.Value('i', 0)
except FileNotFoundError:
    multiprocessing = None

def write_into_channel(channel, value, value2):
    if value2 is not None:
        channel.write(value, value2)
    else:
        channel.write(value)

class TestChannelList(unittest.TestCase):

    def setUp(self):
        pass

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

    """ Channel List """

    def do_list(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(0), default[0])
        self.assertEqual(channel.read(-1), default[-1])
        self.assertEqual(channel.get_data()[0], default[0])
        channel.task_done()
        self.assertFalse(channel.is_readable())
        channel.clear()
        self.assertEqual(channel.read(0), None)
        self.do_write(channel, 10)
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(0), 10)
        self.assertEqual(channel.read(-1), 10)
        self.do_write(channel, 13)
        self.do_write(channel, 37)
        self.assertEqual(channel.read(1), 13)
        self.assertEqual(channel.read(2), 37)
        self.assertEqual(channel.read(-1), 37)
        channel.task_done()
        self.assertFalse(channel.is_readable())
        self.do_write(channel, 42, -1)
        self.assertEqual(channel.read(-1), 42)
        channel.clear()
        self.do_write(channel, [1, 1, 2, 3, 5, 7])
        self.assertEqual(channel.read(0), 1)
        self.assertEqual(channel.read(-1), 7)
        channel.clear()
        self.assertEqual(channel.read(-1), None)

    def test_channel_list(self):
        print()
        default = [1, 2, 3]
        channel = ChannelList(default=default)
        self.do_list(channel, default)

        if multiprocessing:
            print()
            default = [4, 5, 6]
            channel = ChannelList(default=default, mp=True)
            self.do_list(channel, default)

if __name__ == '__main__':
    unittest.main(verbosity=2)
