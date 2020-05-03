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

class TestChannelList(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

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
        self.assertTrue(utils.write_channel(channel, 10))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(0), 10)
        self.assertEqual(channel.read(-1), 10)
        self.assertTrue(utils.write_channel(channel, 13))
        self.assertTrue(utils.write_channel(channel, 37))
        self.assertEqual(channel.read(1), 13)
        self.assertEqual(channel.read(2), 37)
        self.assertEqual(channel.read(-1), 37)
        channel.task_done()
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, 42, -1))
        self.assertEqual(channel.read(-1), 42)
        channel.clear()
        self.assertTrue(utils.write_channel(channel, [1, 1, 2, 3, 5, 7]))
        self.assertEqual(channel.read(0), 1)
        self.assertEqual(channel.read(-1), 7)
        channel.clear()
        self.assertEqual(channel.read(-1), None)

    def test_channel_list(self):
        print()
        default = [1, 2, 3]
        channel = ChannelList(default=default)
        self.do_list(channel, default)

        if utils.is_multiprocessing():
            print()
            default = [4, 5, 6]
            channel = ChannelList(default=default, mp=True)
            self.do_list(channel, default)

if __name__ == '__main__':
    unittest.main(verbosity=2)
