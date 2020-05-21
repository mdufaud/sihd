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

class TestNestedObject(object):

    def __init__(self):
        self.val = 10

    def get(self):
        return self.val

    def set(self, val):
        self.val = val

class TestClassPoint(object):

    def __init__(self, x, y, threeD=False):
        logger.info("Creation of {}: x: {} y: {}"\
                        .format(self.__class__.__name__, x, y))
        self.x = x
        self.y = y
        self.ddd = threeD

    def is_ddd(self):
        return self.ddd

    def set(self, value):
        x, y = value
        self.x = x
        self.y = y

    def get_x(self):
        return self.x

    def get_y(self):
        return self.y

class TestChannelObject(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    """ Channel Obj """

    def do_object(self, channel, default):
        logger.info("Testing " + str(channel))
        #Should not have a default write
        self.assertFalse(channel.is_readable())

        self.assertEqual(channel.read('get_x'), 1)
        self.assertEqual(channel.read('get_y'), 2)

        channel.consumed_data()
        self.assertFalse(channel.is_readable())

        self.assertTrue(utils.write_channel(channel, 'set', (13, 42)))
        self.assertEqual(channel.read('get_x'), 13)
        self.assertEqual(channel.read('get_y'), 42)

        self.assertEqual(channel.read('get_smth'), None)
        self.assertEqual(channel.read('x'), None)
        self.assertEqual(channel.read('y'), None)

        self.assertEqual(channel.read('is_ddd'), True)

        obj = channel.get_data()
        self.assertEqual(obj.is_ddd(), True)


    def test_channel_object(self):
        from sihd.Core.Channel import register_channel_object
        register_channel_object("test_id", TestClassPoint)
        print()
        values = (3, 4)
        default = ('set', values)
        channel = ChannelObject("test_id", [1, 2], {"threeD": True}, default=default)
        self.do_object(channel, values)
        
        if utils.is_multiprocessing():
            print()
            values = (3, 4)
            default = ('set', values)
            channel = ChannelObject("test_id", [1, 2], {"threeD": True},
                                    default=default, mp=True)
            self.do_object(channel, values)

if __name__ == '__main__':
    unittest.main(verbosity=2)
