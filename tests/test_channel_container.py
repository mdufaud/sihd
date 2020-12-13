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

from array import array

class TestChannelContainer(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    def test_container(self):
        container = sihd.core.ChannelContainer("test")
        container.configuration.load({
            'channels': {
                'ch1': "type=int;default=5",
                'ch2': {"type": "bool", "default": False},
                'ch3': "type=counter"
            }
        })
        sihd.tree.dump()
        container.make_channels()
        sihd.tree.dump()
        self.assertEqual(container.ch1.read(), 5)
        self.assertTrue(container.ch1.write(10))
        self.assertEqual(container.ch1.read(), 10)

        self.assertEqual(container.ch2.read(), False)
        self.assertTrue(container.ch2.write(True))
        self.assertEqual(container.ch2.read(), True)

        self.assertEqual(container.ch3.read(), 0)
        self.assertTrue(container.ch3.write())
        self.assertEqual(container.ch3.read(), 1)
        container.remove_channels()
        with self.assertRaises(AttributeError):
            container.ch3.write()

if __name__ == '__main__':
    unittest.main(verbosity=2)
