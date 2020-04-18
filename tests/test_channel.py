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

try:
    from multiprocessing import Process, Manager
except ImportError:
    pass

try:
    if multiprocessing is not None:
        #checks for /dev/shm
        val = multiprocessing.Value('i', 0)
except FileNotFoundError:
    multiprocessing = None

def write_into_channel(channel, value, value2):
    if value2 is not None:
        channel.write(value, value2)
    else:
        channel.write(value)

class TestClassPoint(object):

    def __init__(self, x, y, threeD=False):
        self.x = 0
        self.y = 0
        if threeD:
            self.z = 0

class TestChannel(unittest.TestCase):

    def setUp(self):
        pass

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

    """ Channel Obj """

    def do_object(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(default[0]), default[1])
        channel.task_done()
        self.assertFalse(channel.is_readable())
        self.do_write(channel, 'x', 42)
        self.assertEqual(channel.read('x'), 42)
        self.assertEqual(channel.read('fake'), None)

    def test_channel_object(self):
        from sihd.Core.Channel import register_channel_object
        register_channel_object("test_id", TestClassPoint)
        print()
        default = ('x', 4)
        channel = ChannelObject({
            "ident": "test_id",
            "args": [1, 2],
            "kwargs": {"threeD": True}
        }, default=default)
        self.do_object(channel, default)
        
        if multiprocessing:
            print()
            default = ('x', 4)
            channel = ChannelObject({
                "ident": "test_id",
                "args": [1, 2],
                "kwargs": {"threeD": True}
            }, default=default, mp=True)
            self.do_object(channel, default)

    """ Channel Bool """

    def do_bool(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(), default)
        channel.task_done()
        self.assertFalse(channel.is_readable())
        self.do_write(channel, not default)
        self.assertEqual(channel.read(), not default)
        self.assertEqual(channel.read(), not default)
        self.do_write(channel, default)
        self.assertEqual(channel.read(), default)

    def test_channel_bool(self):
        print()
        default = True
        channel = ChannelBool(default=default)
        self.do_bool(channel, default)
        
        if multiprocessing:
            print()
            default = True
            channel = ChannelBool(default=default, mp=True)
            self.do_bool(channel, default)

    """ Channel Array """

    def do_array(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(0), default[0])
        self.assertEqual(channel.read(-1), default[-1])
        self.assertEqual(channel.read(len(default)), None)
        self.assertEqual(channel.read(len(default) + 1), None)
        channel.task_done()
        self.assertFalse(channel.is_readable())

        self.do_write(channel, default[0], len(default), expect=False)
        self.do_write(channel, default[0], len(default) + 1, expect=False)

        self.assertEqual(channel.get_data()[0], default[0])

        self.do_write(channel, default[0], -1)
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(-1), default[0])

        self.do_write(channel, default[-1], 0)
        self.assertEqual(channel.read(0), default[-1])

        self.do_write(channel, default)
        self.assertEqual(channel.read(-1), default[-1])
        self.assertEqual(channel.read(0), default[0])

        channel.clear()
        self.assertEqual(channel.read(0), 0)
        self.assertEqual(channel.read(-1), 0)

    def test_channel_array(self):
        print()
        default = [1, 2, 3]
        channel = ChannelArray(default=default, ctype='i', size=len(default))
        self.do_array(channel, default)

        print()
        default = [1.2, 2.3, 3.4]
        channel = ChannelArray(default=default, ctype='d', size=len(default))
        self.do_array(channel, default)

        if multiprocessing:
            print()
            default = [1, 2, 3]
            channel = ChannelArray(default=default, ctype='i', size=len(default), mp=True)
            self.do_array(channel, default)

            print()
            default = [1.2, 2.3, 3.4]
            channel = ChannelArray(default=default, ctype='d', size=len(default), mp=True)
            self.do_array(channel, default)
    
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
        self.do_write(channel, 10)
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(0), 10)
        self.assertEqual(channel.read(-1), 10)
        self.do_write(channel, 13)
        self.do_write(channel, 37)
        self.assertEqual(channel.read(1), 13)
        self.assertEqual(channel.read(2), 37)
        self.assertEqual(channel.read(-1), 37)
        channel.task_done()
        self.assertFalse(channel.is_readable())
        self.do_write(channel, 42, -1)
        self.assertEqual(channel.read(-1), 42)
        channel.clear()
        self.do_write(channel, [1, 1, 2, 3, 5, 7])
        self.assertEqual(channel.read(0), 1)
        self.assertEqual(channel.read(-1), 7)
        channel.clear()
        self.assertEqual(channel.read(-1), None)

    def test_channel_list(self):
        print()
        default = [1, 2, 3]
        channel = ChannelList(default=default)
        self.do_list(channel, default)

        if multiprocessing:
            print()
            default = [4, 5, 6]
            channel = ChannelList(default=default, mp=True)
            self.do_list(channel, default)

    """ Channel Dict """

    def do_dict(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read("key"), default['key'])
        self.assertEqual(channel.get_data()['key'], default['key'])
        channel.clear('key')
        self.assertEqual(channel.read("key"), None)
        channel.task_done()
        self.assertFalse(channel.is_readable())

        self.do_write(channel, "hello", 'world')
        self.assertTrue(channel.is_readable())
        self.do_write(channel, 1, {"some": "dict"})
        self.do_write(channel, (2), (1, 2))

        self.assertEqual(channel.read('hello'), 'world')
        self.assertEqual(channel.read(1)["some"], "dict")
        self.assertEqual(channel.read((2)), (1, 2))

        channel.clear()
        self.assertEqual(channel.read('hello'), None)
        self.assertEqual(channel.read(1), None)
        self.assertEqual(channel.read((2)), None)

    def test_channel_dict(self):
        print()
        default = {"key": "value"}
        channel = ChannelDict(default=default)
        self.do_dict(channel, default)

        if multiprocessing:
            print()
            default = {"key": ("l2p", "noob")}
            channel = ChannelDict(default=default, mp=True)
            self.do_dict(channel, default)

    """ Channel Queue """

    def queue_do_list(self, channel, lst):
        for el in lst:
            self.do_write(channel, el)
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
        self.do_write(channel, 'smth')
        self.assertTrue(channel.is_readable())
        channel.clear()
        self.assertFalse(channel.is_readable())
   
    def test_channel_queue(self):
        print()
        default = {"some": "datastruct"}
        channel = ChannelQueue(default=default)
        self.do_queue(channel, default)

        if multiprocessing:
            print()
            default = ['my', 'bad']
            channel = ChannelQueue(default=default, mp=True, simple=True)
            self.do_queue(channel, default)

            print()
            default = ('hello', 'darkness', 'my', 'man', {"hey": "sup"})
            channel = ChannelQueue(default=default, mp=True)
            time.sleep(0.1)
            self.do_queue(channel, default)

    """ Channel String """

    def do_string(self, channel, default):
        logger.info("Testing " + str(channel))
        self.assertTrue(channel.is_readable())
        self.assertEqual(channel.read(), default)
        channel.task_done()
        self.assertFalse(channel.is_readable())
        self.do_write(channel, "world")
        self.assertEqual(channel.read(), "world")
        #too long
        self.assertFalse(channel.write("hello world"))
        self.assertEqual(channel.read(), "world")

    def test_channel_string(self):
        print()
        channel = ChannelString(default='hello', size=10)
        self.do_string(channel, "hello")

        if multiprocessing:
            print()
            channel = ChannelString(default='hello', size=10, mp=True)
            self.do_string(channel, "hello")

    """ Channel Value """

    def chan_limit(self, cls, **kwargs):
        print()
        channel = cls(default=0, unsigned=True, **kwargs)
        logger.info("Testing limit of " + str(channel))
        self.do_write(channel, -1)
        logger.info("Wrote -1 -> {}".format(channel.read()))
        self.assertFalse(channel.read() == -1)

        if multiprocessing:
            print()
            channel = cls(default=0, unsigned=True, mp=True, **kwargs)
            logger.info("Testing MP limit of " + str(channel))
            self.do_write(channel, -1)
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
        self.assertTrue(channel.write(write))
        self.do_write(channel, write)
        self.assertTrue(channel.is_readable())
        logger.info("Testing read -> {} ?= {}".format(channel.read(), write))
        self.do_write(channel, write)
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

        if multiprocessing:
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
        if multiprocessing:
            self.chan_value(ChannelInt, default=42, write=1337, mp=True)
        self.chan_limit(ChannelInt)
        #Short
        self.chan_value(ChannelShort, default=-20, write=1)
        if multiprocessing:
            self.chan_value(ChannelShort, default=42, write=1337, mp=True)
        self.chan_limit(ChannelShort)
        #Byte
        self.chan_value(ChannelByte, default=127, write=-128)
        if multiprocessing:
            self.chan_value(ChannelByte, default=1, write=255, mp=True, unsigned=True)
        self.chan_limit(ChannelByte)
        #Char
        self.chan_value(ChannelChar, default='m', write='d')
        if multiprocessing:
            self.chan_value(ChannelChar, default='é', write='à', mp=True, unicode=True)
        #Long
        self.chan_value(ChannelLong, default=123456789, write=9876543211)
        if multiprocessing:
            self.chan_value(ChannelLong, default=123456789, write=9876543211, mp=True, unsigned=True)
        self.chan_limit(ChannelLong)

if __name__ == '__main__':
    unittest.main(verbosity=2)
