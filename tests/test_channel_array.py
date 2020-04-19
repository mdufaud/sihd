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

class TestChannelArray(unittest.TestCase):

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

    """ Channel Array """

    def do_array(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(0), default[0])
        self.assertEqual(channel.read(-1), default[-1])
        self.assertEqual(channel.read(len(default)), None)
        self.assertEqual(channel.read(len(default) + 1), None)
        channel.task_done()
        self.assertFalse(channel.is_readable())

        self.do_write(channel, default[0], len(default), expect=False)
        self.do_write(channel, default[0], len(default) + 1, expect=False)

        self.assertEqual(channel.get_data()[0], default[0])

        self.do_write(channel, default[0], -1)
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(-1), default[0])

        self.do_write(channel, default[-1], 0)
        self.assertEqual(channel.read(0), default[-1])

        self.do_write(channel, default)
        self.assertEqual(channel.read(-1), default[-1])
        self.assertEqual(channel.read(0), default[0])

        channel.clear()
        self.assertEqual(channel.read(0), 0)
        self.assertEqual(channel.read(-1), 0)

    def test_channel_array(self):
        print()
        default = [1, 2, 3]
        channel = ChannelArray(default=default, ctype='i', size=len(default))
        self.do_array(channel, default)

        print()
        default = [1.2, 2.3, 3.4]
        channel = ChannelArray(default=default, ctype='d', size=len(default))
        self.do_array(channel, default)

        if multiprocessing:
            print()
            default = [1, 2, 3]
            channel = ChannelArray(default=default, ctype='i', size=len(default), mp=True)
            self.do_array(channel, default)

            print()
            default = [1.2, 2.3, 3.4]
            channel = ChannelArray(default=default, ctype='d', size=len(default), mp=True)
            self.do_array(channel, default)

if __name__ == '__main__':
    unittest.main(verbosity=2)
