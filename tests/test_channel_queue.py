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

class TestChannelQueue(unittest.TestCase):

    def setUp(self):
        print()
        sihd.clear_tree()

    def tearDown(self):
        pass

    def write_mp(self, channel, value, value2=None):
        p = Process(target=write_into_channel, args=[channel, value, value2])
        p.start()
        p.join()

    def do_write(self, channel, value, value2=None, expect=True):
        logger.info("Writing -> {}{}".format(value,
            " - " + str(value2) if value2 is not None else ""))
        if channel.is_multiprocess():
            self.write_mp(channel, value, value2)
        else:
            if value2 is not None:
                self.assertTrue(channel.write(value, value2) == expect)
            else:
                self.assertTrue(channel.write(value) == expect)

    """ Channel Queue """

    def queue_do_list(self, channel, lst):
        for el in lst:
            self.assertTrue(utils.write_channel(channel, el))
        for el in lst:
            self.assertEqual(el, channel.read())

    def do_queue(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(), default)
        self.assertFalse(channel.is_readable())
        self.assertTrue(channel.read() == None)
        self.queue_do_list(channel, ['one', 'two', 'yeeeeeee'])
        self.assertFalse(channel.is_readable())
        self.assertTrue(utils.write_channel(channel, 'smth'))
        self.assertTrue(channel.is_readable())
        channel.clear()
        self.assertFalse(channel.is_readable())
   
    def test_channel_queue(self):
        print()
        default = {"some": "datastruct"}
        channel = ChannelQueue(default=default)
        self.do_queue(channel, default)

        if utils.is_multiprocessing():
            print()
            default = ['my', 'bad']
            channel = ChannelQueue(default=default, mp=True, simple=True)
            self.do_queue(channel, default)

            print()
            default = ('hello', 'darkness', 'my', 'man', {"hey": "sup"})
            channel = ChannelQueue(default=default, mp=True)
            time.sleep(0.1)
            self.do_queue(channel, default)


if __name__ == '__main__':
    unittest.main(verbosity=2)
