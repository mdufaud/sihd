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

    def test_config_path(self):
        api = ConfigApi("test.config3")
        dic = {
            'dic': {
                'subdic': {
                    'value': 1,
                },
                'lst': [2, 3, 4],
                'value': 5,
            },
            'lst': [6, [7, 8, 9], 10],
            'value': 42,
        }
        api.add_defaults(dic)
        # Getting
        self.assertEqual(api.get('dic.subdic.value'),
                         dic['dic']['subdic']['value'])
        with self.assertRaises(KeyError):
            api.get('dic.subdic.value2')
        self.assertEqual(api.get('dic.lst.0'), dic['dic']['lst'][0])
        self.assertEqual(api.get('dic.lst.1'), dic['dic']['lst'][1])
        self.assertEqual(api.get('dic.lst.2'), dic['dic']['lst'][2])
        with self.assertRaises(KeyError):
            api.get('dic.lst.3')
        self.assertEqual(api.get('lst.0'), dic['lst'][0])
        self.assertEqual(api.get('lst.1.0'), dic['lst'][1][0])
        self.assertEqual(api.get('lst.1.1'), dic['lst'][1][1])
        self.assertEqual(api.get('lst.1.2'), dic['lst'][1][2])
        with self.assertRaises(KeyError):
            api.get('lst.1.3')
        with self.assertRaises(KeyError):
            api.get('lst.1.4')
        with self.assertRaises(KeyError):
            api.get('lst.1.4.5.6.7')
        self.assertEqual(api.get('lst.2'), dic['lst'][2])
        self.assertEqual(api.get('value'), 42)
        with self.assertRaises(KeyError):
            api.get('value2')
        # Setting
        api.set('dic.test', {'new': 1337})
        self.assertEqual(api.get('dic.test.new'), 1337)
        with self.assertRaises(KeyError):
            api.set('dic.smth.else', -1)
        api.set('lst.1.2', [15, 51])
        self.assertEqual(api.get('lst.1.2'), [15, 51])
        with self.assertRaises(KeyError):
            api.set('lst.1.3', -1)
        api.set('lst.1.-1', 'last')
        self.assertEqual(api.get('lst.1.-1'), 'last')
        # Load
        api.load('{"dic.lst.1": 22, "lst.1.-1": "double-last"}')
        self.assertEqual(api.get('dic.lst.1'), 22)
        self.assertEqual(api.get('lst.1.-1'), 'double-last')

    def test_config_api_load(self):
        api = ConfigApi('test.config2')
        api.add_defaults({
            'key': 42,
            'lst': ["toto", "titi"],
            'dic': {"hello": "world"},
        })
        api.add_defaults({
            'akey': 1337,
        }, expose=False)
        self.assertEqual(api.is_set('key'), False)
        self.assertEqual(api.is_set('akey'), False)
        # Load multiple
        api.load('{"key": 24,"akey": 1}')
        self.assertEqual(api.is_set('key'), True)
        self.assertEqual(api.is_set('akey'), True)
        self.assertEqual(api.get_default('key'), 42)
        self.assertEqual(api.get('key'), 24)
        self.assertEqual(api.get_default('akey'), 1337)
        self.assertEqual(api.get('akey'), 1)
        # Load single
        api.load({"key": 1001})
        self.assertEqual(api.get('key'), 1001)
        self.assertEqual(api.get('akey'), 1)
        # Load single same
        api.load({"key": 1002})
        self.assertEqual(api.get('key'), 1002)
        self.assertEqual(api.get('akey'), 1)

    def test_config_api(self):
        api = ConfigApi('test.config')
        api.add_defaults({
            'key': 42,
            'lst': ["toto", "titi"],
            'dic': {"hello": "world"},
        })
        api.add_defaults({
            'akey': 1337,
        }, expose=False)
        api.dump()
        dic = api.to_dict()
        # Check default
        self.assertEqual(dic.get('key'), 42)
        self.assertEqual(dic.get('akey', False), False)
        self.assertEqual(api.get_default('key'), 42)
        self.assertEqual(api.get('key'), 42)
        # Set
        api.set('key', 24)
        self.assertEqual(api.get_default('key'), 42)
        self.assertEqual(api.get('key'), 24)
        # Set new type
        api.set('key', "test")
        self.assertEqual(api.get_default('key'), 42)
        self.assertEqual(api.get('key'), "test")
        # Check not exposed conf
        self.assertEqual(api.get_default('akey'), 1337)
        self.assertEqual(api.get('akey'), 1337)
        # Force
        api.set('akey', 7331, force=True)
        self.assertEqual(api.get_default('akey'), 1337)
        self.assertEqual(api.get('akey'), 7331)
        # Can not be changed
        api.set('akey', 1001)
        self.assertEqual(api.get_default('akey'), 1337)
        self.assertEqual(api.get('akey'), 7331)
        # Check types
        self.assertTrue(dic.get('lst'))
        self.assertEqual(dic.get('lst')[0], "toto")
        self.assertEqual(dic.get('lst')[1], "titi")
        self.assertTrue(dic.get('dic'))
        self.assertEqual(dic.get('dic')["hello"], "world")
        api.dump()

if __name__ == '__main__':
    unittest.main(verbosity=2)
