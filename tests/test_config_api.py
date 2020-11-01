#!/usr/bin/python
# coding: utf-8

import os
import sys
import time

import utils
import sihd
import json
from sihd.utils.config import ConfigApi

logger = sihd.log.setup('info')

import unittest

class TestApiConfig(unittest.TestCase):

    def setUp(self):
        print()
        sihd.tree.clear()

    def tearDown(self):
        time.sleep(0.01)

    def get_conf_path(self):
        dir_path = os.path.join(os.path.dirname(__file__), "config")
        return dir_path

    def test_config_api_load(self):
        api = ConfigApi('test.config2')
        api.add_defaults({
            'key': 42,
            'akey': (1337, {'expose': False}),
        })
        self.assertEqual(api.is_set('key'), False)
        self.assertEqual(api.is_set('akey'), False)
        api.load('{"key": 24,"akey": 1}')
        self.assertEqual(api.is_set('key'), True)
        self.assertEqual(api.is_set('akey'), True)
        self.assertEqual(api.get_default('key'), 42)
        self.assertEqual(api.get('key'), 24)
        self.assertEqual(api.get_default('akey'), 1337)
        self.assertEqual(api.get('akey'), 1)
        api.load({"key": 1001})
        self.assertEqual(api.get('key'), 1001)
        self.assertEqual(api.get('akey'), 1)
        api.load({"key": 1002})
        self.assertEqual(api.get('key'), 1002)
        self.assertEqual(api.get('akey'), 1)

    def test_config_api(self):
        api = ConfigApi('test.config')
        api.add_defaults({
            'key': 42,
            'akey': (1337, {'expose': False}),
        })
        api.dump()
        dic = api.to_dict()
        self.assertEqual(dic.get('key'), 42)
        self.assertEqual(dic.get('akey', False), False)
        self.assertEqual(api.get_default('key'), 42)
        self.assertEqual(api.get('key'), 42)
        api.set('key', 24)
        self.assertEqual(api.get_default('key'), 42)
        self.assertEqual(api.get('key'), 24)
        api.set('key', "test")
        self.assertEqual(api.get_default('key'), 42)
        self.assertEqual(api.get('key'), "test")

        self.assertEqual(api.get_default('akey'), 1337)
        self.assertEqual(api.get('akey'), 1337)
        api.set('akey', 7331, force=True)
        self.assertEqual(api.get_default('akey'), 1337)
        self.assertEqual(api.get('akey'), 7331)
        api.set('akey', 1001)
        self.assertEqual(api.get_default('akey'), 1337)
        self.assertEqual(api.get('akey'), 7331)
        api.dump()

if __name__ == '__main__':
    unittest.main(verbosity=2)
