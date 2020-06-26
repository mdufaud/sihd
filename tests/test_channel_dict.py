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

from sihd.Handlers.AHandler import AHandler
from sihd.Core.Channel import *

class LittleObject(object):

    def __init__(self):
        self.a = 1
        self.b = 2

    def __str__(self):
        return "self.a=" + str(self.a)

class TestChannelDict(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    """ Channel Dict """

    def do_dict(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertEqual(channel.read("key"), default['key'])
        self.assertEqual(channel.get_data()['key'], default['key'])
        channel.clear('key')
        self.assertEqual(channel.read("key"), None)
        channel.consumed_data()
        self.assertFalse(channel.is_readable())

        self.assertTrue(utils.write_channel(channel, "hello", 'world'))
        self.assertTrue(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, 1, {"some": "dict"}))
        self.assertTrue(utils.write_channel(channel, (2), (1, 2)))

        self.assertEqual(channel.read('hello'), 'world')
        self.assertEqual(channel.read(1)["some"], "dict")
        self.assertEqual(channel.read((2)), (1, 2))

        channel.clear()
        self.assertEqual(channel.read('hello'), None)
        self.assertEqual(channel.read(1), None)
        self.assertEqual(channel.read((2)), None)

        channel.clear()
        self.assertTrue(channel.write({"key": "value", "key2": "value2"}))
        self.assertEqual(channel.read("key"), "value")
        self.assertEqual(channel.read("key2"), "value2")

        channel.clear()
        self.assertTrue(channel.write("obj", LittleObject()))
        print(channel.read("obj"))
        self.assertEqual(channel.read("obj").a, 1)
        self.assertEqual(channel.read("obj").b, 2)

    def test_channel_dict(self):
        print()
        default = {"key": "value"}
        channel = ChannelDict(default=default)
        self.do_dict(channel, default)

        if utils.is_multiprocessing():
            print()
            default = {"key": ("l2p", "noob")}
            channel = ChannelDict(default=default, mp=True)
            self.do_dict(channel, default)

if __name__ == '__main__':
    unittest.main(verbosity=2)
