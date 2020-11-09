#!/usr/bin/python
# coding: utf-8

import os
import sys
import time

import utils
import sihd

import json

logger = sihd.log.setup('info')

import unittest

class TestClassConfig(unittest.TestCase):

    def setUp(self):
        print()
        sihd.tree.clear()

    def tearDown(self):
        time.sleep(0.01)

    def get_conf_path(self):
        dir_path = os.path.join(os.path.dirname(__file__), "output")
        return dir_path

    def test_simple_config(self):
        reader = sihd.readers.sys.LineReader(name="LineReader")
        default = reader.configuration.get_default("path")
        value = reader.configuration.get("path")
        self.assertTrue(default == value)
        not_default = reader.configuration.get("path", default=False)
        self.assertTrue(not_default is None)
        reader.configuration.load({"path": "some_path"})
        value = reader.configuration.get("path")
        self.assertTrue(value == "some_path")
        reader.configuration.set("path", __file__)
        value = reader.configuration.get("path")
        self.assertTrue(value == __file__)
        self.assertTrue(default == reader.configuration.get_default("path"))
        self.assertTrue(reader.setup())

    def test_app_config_changed(self):
        app = utils.TestApp()
        #Check if config is here and good
        path_config = os.path.join(app.get_default_conf_path())
        if not os.path.exists(os.path.dirname(path_config)):
            os.makedirs(os.path.dirname(path_config))
        with open(path_config, 'w+') as fd:
            json.dump({
                "log": {
                    "level": 'debug',
                },
                "children": {
                    "LineReader": {
                        "path": __file__,
                    },
                }
            }, fd)
            logger.info("Config file written")
        app.set_args([
            "-f", __file__,
            "-s",
        ])
        self.assertTrue(app.setup_app())
        self.assertTrue(app.start())
        app.stop()

        appconf = {}
        with open(path_config, 'r') as fd:
            appconf = json.load(fd)
        print(appconf)
        self.assertTrue(appconf.get('children', False))
        self.assertTrue(appconf['children'].get('LineReader', False))
        self.assertTrue(appconf['children']['LineReader'].get('path', False))
        file_to_read = appconf['children']['LineReader']['path']
        logger.info("File setted: {} (== {})".format(file_to_read, __file__))
        self.assertEqual(file_to_read, __file__)
        os.remove(path_config)

if __name__ == '__main__':
    unittest.main(verbosity=2)
