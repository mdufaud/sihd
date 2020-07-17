#!/usr/bin/python
# coding: utf-8

import os
import sys
import time

import utils
import sihd
from sihd.API.config import ConfigApi

try:
    import ConfigParser
except ImportError:
    import configparser
    ConfigParser = configparser

logger = sihd.log.setup('info')

import unittest

class TestApiConfig(unittest.TestCase):

    def setUp(self):
        self.config = ConfigParser.ConfigParser()
        print()
        sihd.tree.clear()

    def tearDown(self):
        time.sleep(0.01)

    def get_conf_path(self):
        dir_path = os.path.join(os.path.dirname(__file__), "config")
        return dir_path

    def read_configuration(self, path):
        self.config.read(path)

    def write_conf(self, path, obj=None):
        if obj is None:
            obj = self.config
        with open(path, 'w+') as configfile:
            obj.write(configfile)

    def test_config_api_file(self):
        api = ConfigApi('test.config2')
        api.add_defaults({
            'key': 42,
            'akey': (1337, {'infile': False}),
        })
        api.obj = self.config
        self.assertTrue(api.save())
        self.assertTrue(self.config.has_option(api.name, "key"))
        self.assertEqual(self.config.get(api.name, "key"), "42")
        self.assertFalse(self.config.has_option(api.name, "akey"))
        path_config = os.path.join(self.get_conf_path(),
                        "{}.ini".format('api_test'))
        self.write_conf(path_config)
        self.assertFalse(self.config.has_option(api.name, "akey"))
        self.assertEqual(api.get_default('key'), "42")
        self.assertEqual(api.get('key'), "42")
        self.assertEqual(api.get_default('akey'), "1337")
        self.assertEqual(api.get('akey'), "1337")
        api.set('key', 24)
        api.set('akey', 'impossibru')
        self.assertTrue(api.save())
        self.write_conf(path_config)
        self.assertEqual(api.get_default('key'), "42")
        self.assertEqual(api.get('key'), "24")
        self.assertEqual(api.get_default('akey'), "1337")
        self.assertEqual(api.get('akey'), "impossibru")
        self.assertFalse(self.config.has_option(api.name, "akey"))

    def test_config_api(self):
        api = ConfigApi('test.config')
        api.add_defaults({
            'key': 42,
            'akey': (1337, {'infile': False}),
        })
        api.dump()
        self.assertEqual(api.get_default('key'), "42")
        self.assertEqual(api.get('key'), "42")
        api.set('key', 24)
        self.assertEqual(api.get_default('key'), "42")
        self.assertEqual(api.get('key'), "24")
        api.set('key', "test")
        self.assertEqual(api.get_default('key'), "42")
        self.assertEqual(api.get('key'), "test")

        self.assertEqual(api.get_default('akey'), "1337")
        self.assertEqual(api.get('akey'), "1337")
        api.set('akey', 7331, force=True)
        self.assertEqual(api.get_default('akey'), "1337")
        self.assertEqual(api.get('akey'), "7331")
        api.set('akey', 1001)
        self.assertEqual(api.get_default('akey'), "1337")
        self.assertEqual(api.get('akey'), "7331")
        api.dump()
        self.assertFalse(api.save())

if __name__ == '__main__':
    unittest.main(verbosity=2)
