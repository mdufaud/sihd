#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import time
import utils
import socket
import unittest

import sihd
logger = sihd.set_log('debug')

from sihd.Handlers.IHandler import IHandler
from sihd.Core.Channel import *
from sihd.Core import SihdThread

class TestChannelArray(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

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

    def test_channel_array(self):
        print()
        default = [1, 2, 3]
        channel = ChannelArray(default=default, ctype='i', size=len(default))
        self.do_array(channel, default)

        print()
        default = [1.2, 2.3, 3.4]
        channel = ChannelArray(default=default, ctype='d', size=len(default))
        self.do_array(channel, default)

        if utils.is_multiprocessing():
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
