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

from sihd.Handlers.IHandler import IHandler
from sihd.Core.Channel import *
from sihd.Core import SihdThread

class TestChannelDict(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    """ Channel Bool """

    def do_bool(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(), default)
        channel.task_done()
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, not default))
        self.assertEqual(channel.read(), not default)
        self.assertEqual(channel.read(), not default)
        self.assertTrue(utils.write_channel(channel, default))
        self.assertEqual(channel.read(), default)

    def test_channel_bool(self):
        print()
        default = True
        channel = ChannelBool(default=default)
        self.do_bool(channel, default)
        
        if utils.is_multiprocessing():
            print()
            default = True
            channel = ChannelBool(default=default, mp=True)
            self.do_bool(channel, default)

if __name__ == '__main__':
    unittest.main(verbosity=2)
