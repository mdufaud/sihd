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

class TestChannelDict(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    """ Channel Bool """

    def do_bool(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertEqual(channel.read(), default)
        channel.consumed_data()
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
