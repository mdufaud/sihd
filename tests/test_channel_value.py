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

class TestChannelValue(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    """ Channel Value """

    def chan_limit(self, cls, **kwargs):
        print()
        channel = cls(default=0, unsigned=True, **kwargs)
        logger.info("Testing limit of " + str(channel))
        self.assertTrue(utils.write_channel(channel, -1))
        logger.info("Wrote -1 -> {}".format(channel.read()))
        self.assertFalse(channel.read() == -1)

        if utils.is_multiprocessing():
            print()
            channel = cls(default=0, unsigned=True, mp=True, **kwargs)
            logger.info("Testing MP limit of " + str(channel))
            self.assertTrue(utils.write_channel(channel, -1))
            logger.info("Wrote -1 -> {}".format(channel.read()))
            self.assertFalse(channel.read() == -1)

    def chan_value(self, cls, default=0, write=0, **kwargs):
        print()
        channel = cls(default=default, **kwargs)
        logger.info("Testing " + str(channel))

        logger.info("Test polling/read/write")
        self.assertTrue(channel.is_readable())
        logger.info("Testing read -> {} ?= {}".format(channel.read(), default))
        self.assertEqual(channel.read(), default)
        channel.task_done()
        self.assertTrue(channel.is_readable() == False)
        self.assertTrue(utils.write_channel(channel, write))
        self.assertTrue(channel.is_readable())
        logger.info("Testing read -> {} ?= {}".format(channel.read(), write))
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

    def test_channel_double(self):
        print()
        default = 0.222
        write = 1231.1123123

        channel = ChannelDouble(default=default)
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

        if utils.is_multiprocessing():
            print()
            channel = ChannelDouble(default=default, mp=True)
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
    
    def test_channel_value(self):
        #Int
        self.chan_value(ChannelInt, default=1, write=101)
        if utils.is_multiprocessing():
            self.chan_value(ChannelInt, default=42, write=1337, mp=True)
        self.chan_limit(ChannelInt)
        #Short
        self.chan_value(ChannelShort, default=-20, write=1)
        if utils.is_multiprocessing():
            self.chan_value(ChannelShort, default=42, write=1337, mp=True)
        self.chan_limit(ChannelShort)
        #Byte
        self.chan_value(ChannelByte, default=127, write=-128)
        if utils.is_multiprocessing():
            self.chan_value(ChannelByte, default=1, write=255, mp=True, unsigned=True)
        self.chan_limit(ChannelByte)
        #Char
        self.chan_value(ChannelChar, default='m', write='d')
        if utils.is_multiprocessing():
            self.chan_value(ChannelChar, default='é', write='à', mp=True, unicode=True)
        #Long
        self.chan_value(ChannelLong, default=123456789, write=9876543211)
        if utils.is_multiprocessing():
            self.chan_value(ChannelLong, default=123456789, write=9876543211, mp=True, unsigned=True)
        self.chan_limit(ChannelLong)

if __name__ == '__main__':
    unittest.main(verbosity=2)
