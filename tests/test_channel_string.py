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

class TestChannelString(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    """ Channel String """

    def do_string(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertEqual(channel.read(), default)
        channel.consumed_data()
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, "world"))
        self.assertEqual(channel.read(), "world")
        #too long
        self.assertFalse(channel.write("hello world"))
        self.assertEqual(channel.read(), "world")
        channel.clear()
        self.assertEqual(channel.read(), "")

    def test_channel_string(self):
        print()
        channel = ChannelString(default='hello', size=10)
        self.do_string(channel, "hello")

        if utils.is_multiprocessing():
            print()
            channel = ChannelString(default='hello', size=10, mp=True)
            self.do_string(channel, "hello")

if __name__ == '__main__':
    unittest.main(verbosity=2)
