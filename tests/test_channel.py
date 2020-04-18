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

class TestChannel(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def chan_limit(self, cls, **kwargs):
        print()
        channel = cls(default=0, unsigned=True, **kwargs)
        logger.info("Testing limit of " + str(channel))
        self.assertTrue(channel.write(-1))
        logger.info("Wrote -1 -> {}".format(channel.read()))
        self.assertFalse(channel.read() == -1)

        print()
        channel = cls(default=0, unsigned=True, mp=True, **kwargs)
        logger.info("Testing MP limit of " + str(channel))
        self.assertTrue(channel.write(-1))
        logger.info("Wrote -1 -> {}".format(channel.read()))
        self.assertFalse(channel.read() == -1)

    def chan_value(self, cls, default=0, write=0, **kwargs):
        print()
        channel = cls(default=default, **kwargs)
        logger.info("Testing " + str(channel))

        logger.info("Test polling/read/write")
        self.assertTrue(channel.is_readable())
        logger.info("read -> {} ?= {}".format(channel.read(), default))
        self.assertEqual(channel.read(), default)
        channel.task_done()
        self.assertTrue(channel.is_readable() == False)
        self.assertTrue(channel.write(write))
        self.assertTrue(channel.is_readable())
        logger.info("read -> {} ?= {}".format(channel.read(), write))
        self.assertEqual(channel.read(), write)
        self.assertTrue(channel.is_readable())

        logger.info("Test lock")
        self.assertFalse(channel.is_locked())
        self.assertTrue(channel.lock())
        self.assertTrue(channel.is_locked())
        self.assertFalse(channel.is_readable())
        self.assertFalse(channel.write(default))
        self.assertFalse(channel.write(write))
        self.assertTrue(channel.unlock())
        self.assertTrue(channel.is_readable())
        self.assertTrue(channel.write(default))
        self.assertTrue(channel.write(write))
    
    def test_channel_value(self):
        #Int
        self.chan_value(ChannelInt, default=1, write=101)
        self.chan_value(ChannelInt, default=42, write=1337, mp=True)
        self.chan_limit(ChannelInt)
        #Short
        self.chan_value(ChannelShort, default=-20, write=1)
        self.chan_value(ChannelShort, default=42, write=1337, mp=True)
        self.chan_limit(ChannelShort)
        #Byte
        self.chan_value(ChannelByte, default=ord('l'), write=ord('o'))
        self.chan_value(ChannelByte, default=ord('l'), write=127, mp=True)
        self.chan_limit(ChannelByte)
        #Char
        self.chan_value(ChannelChar, default=b'm', write=b'd')
        self.chan_value(ChannelChar, default=u'r', write=u'l', mp=True, unicode=True)
        #Long
        self.chan_value(ChannelLong, default=123456789, write=9876543211)
        self.chan_value(ChannelLong, default=123456789, write=9876543211, mp=True, unsigned=True)
        self.chan_limit(ChannelLong)

if __name__ == '__main__':
    unittest.main(verbosity=2)
