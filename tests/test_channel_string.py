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

class TestChannelString(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    """ Channel String """

    def do_string(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(), default)
        channel.task_done()
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, "world"))
        self.assertEqual(channel.read(), "world")
        #too long
        self.assertFalse(channel.write("hello world"))
        self.assertEqual(channel.read(), "world")

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
