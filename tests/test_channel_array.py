#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import time
import socket
import unittest

import utils
import sihd
logger = sihd.set_log('debug')

from sihd.Handlers.AHandler import AHandler
from sihd.Core.Channel import *

from array import array

class TestChannelArray(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    """ Channel Array """

    def do_array(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        print(channel.read(0, 3))
        self.assertEqual(channel.read(0), default[0])
        self.assertEqual(channel.read(-1), default[-1])
        self.assertEqual(channel.read(len(default)), None)
        self.assertEqual(channel.read(len(default) + 1), None)
        channel.consumed_data()
        self.assertFalse(channel.is_readable())

        self.assertFalse(utils.write_channel(channel, default[0], len(default)))
        self.assertFalse(utils.write_channel(channel, default[0], len(default) + 1))

        self.assertEqual(channel.get_data()[0], default[0])

        self.assertTrue(utils.write_channel(channel, default[0], -1))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(-1), default[0])

        self.assertTrue(utils.write_channel(channel, default[-1], 0))
        self.assertEqual(channel.read(0), default[-1])

        self.assertTrue(utils.write_channel(channel, default))
        self.assertEqual(channel.read(-1), default[-1])
        self.assertEqual(channel.read(0), default[0])

        channel.clear()
        self.assertEqual(channel.read(0), 0)
        self.assertEqual(channel.read(-1), 0)

    def test_channel_int(self):
        print()
        default = [1, 2, 3]
        channel = ChannelArray(name="chan", default=default, ctype='i',
                                size=len(default))
        self.do_array(channel, default)

        if utils.is_multiprocessing():
            print()
            default = [1, 2, 3]
            channel = ChannelArray(name="mpchan", default=default, ctype='i',
                                    size=len(default), mp=True)
            self.do_array(channel, default)

    def test_channel_double(self):
        print()
        default = [1.2, 2.3, 3.4]
        channel = ChannelArray(name="chan", default=default, ctype='d',
                                size=len(default))
        self.do_array(channel, default)

        if utils.is_multiprocessing():
            print()
            default = [1.2, 2.3, 3.4]
            channel = ChannelArray(name="mpchan", default=default,
                                    ctype='d', size=len(default), mp=True)
            self.do_array(channel, default)

    def do_byte(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(0), default[0])
        self.assertEqual(channel.read(-1), 0)
        self.assertEqual(channel.get_stripped(), default.decode())
        channel.consumed_data()
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, b"world"))
        self.assertEqual(channel.read(0, 5), b"world")
        self.assertTrue(utils.write_channel(channel, b"AB", 3))
        self.assertEqual(channel.read(0, 5), b"worAB")
        self.assertTrue(utils.write_channel(channel, b"A", 0))
        self.assertEqual(channel.read(0, 5), b"AorAB")
        channel.clear()
        self.assertTrue(utils.write_channel(channel, b"world"))
        #too long
        self.assertFalse(channel.write(b"hello world"))
        self.assertEqual(channel.read(0, 5), b"world")
        self.assertEqual(channel.get_stripped(), "world")
        channel.clear()
        self.assertEqual(channel.get_stripped(), "")

    def test_channel_byte_array(self):
        print()
        default = b'hello 1!!'
        channel = ChannelArray(name="chan", ctype='b', default=default, size=10)
        self.do_byte(channel, default)

        if utils.is_multiprocessing():
            print()
            channel = ChannelArray(name="mpchan", ctype='b', default=default,
                                    size=10, mp=True)
            self.do_byte(channel, default)

if __name__ == '__main__':
    unittest.main(verbosity=2)
