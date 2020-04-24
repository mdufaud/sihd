#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys
import time

import utils
import sihd

try:
    import ConfigParser
except ImportError:
    import configparser
    ConfigParser = configparser

logger = sihd.set_log('debug')

import unittest

class TestConfig(unittest.TestCase):

    def setUp(self):
        self.config = ConfigParser.ConfigParser()
        print()

    def tearDown(self):
        time.sleep(0.01)

    def get_conf_path(self):
        dir_path = os.path.join(os.path.dirname(__file__), "config")
        return dir_path

    def get_conf(self, path):
        self.config.read(path)

    def write_conf(self, path, obj=None):
        if obj is None:
            obj = self.config
        with open(path, 'w+') as configfile:
            obj.write(configfile)

    def test_simple_config(self):
        reader = sihd.Readers.sys.LineReader(name="LineReader")
        default = reader.get_conf("path")
        self.assertTrue(default == "/path/to/file")
        not_default = reader.get_conf("path", default=False)
        self.assertTrue(not_default is None)

        reader.set_conf("path", __file__)

        my_conf = reader.get_conf("path")
        self.assertTrue(my_conf == __file__)
        self.assertTrue(default == reader.get_default_conf("path"))
        self.assertTrue(reader.setup())

    def test_config_file(self):
        path = os.path.join(self.get_conf_path(), "test_config.ini")
        reader = sihd.Readers.sys.LineReader(name="LineReader")
        reader.set_conf_obj(self.config)
        reader.set_conf("path", __file__)
        reader.save_conf()
        self.write_conf(path)

        obj = ConfigParser.ConfigParser()
        obj.read(path)
        self.assertTrue(obj.has_section(reader.get_name()))
        self.assertTrue(obj.has_option(reader.get_name(), "path"))
        self.assertTrue(obj.get(reader.get_name(), "path") == __file__)
        os.remove(path)

    def test_app_config_changed(self):
        app = utils.TestApp("ConfigTest")

        #Check if config is here and good
        obj = ConfigParser.ConfigParser()
        path_config = os.path.join(self.get_conf_path(),
                "{}.ini".format(app.get_name()))
        obj.read(path_config)
        self.assertTrue(obj.has_section(app.get_name()))
        reader_name = "LineReaderConfigTest"
        self.assertTrue(obj.has_section(reader_name))
        self.assertTrue(obj.has_option(reader_name, "path"))
        obj.set(reader_name, "path", __file__)
        self.write_conf(path_config, obj)

        path = __file__
        app.set_args([
            "-f", path,
            "-s",
        ])
        if app.setup_app() is False:
            sys.exit(1)
        self.assertTrue(app.start())
        app.stop()

        #Check if reader is set to path
        obj = ConfigParser.ConfigParser()
        obj.read(path_config)
        self.assertTrue(obj.has_section(app.get_name()))
        reader = app._line_reader
        self.assertTrue(obj.has_section(reader.get_name()))
        self.assertTrue(obj.has_option(reader.get_name(), "path"))
        file_to_read = obj.get(reader.get_name(), "path")
        logger.info("File setted: {} (== {})"\
                .format(file_to_read, __file__))
        self.assertEqual(file_to_read, __file__)
        os.remove(path_config)

    def test_app_config(self):
        path = __file__
        app = utils.TestApp("ConfigTest")
        app.set_args([
            "-f", path,
            "-s",
        ])
        if app.setup_app() is False:
            sys.exit(1)
        self.assertTrue(app.start())
        self.assertTrue(app.stop())

        obj = ConfigParser.ConfigParser()
        path_config = os.path.join(self.get_conf_path(),
                    "{}.ini".format(app.get_name()))
        logger.info("Reading {}".format(path_config))
        obj.read(path_config)
        self.assertTrue(obj.has_section(app.get_name()))
        reader = app._line_reader
        self.assertTrue(obj.has_section(reader.get_name()))
        self.assertTrue(obj.has_option(reader.get_name(), "path"))
        self.assertEqual(obj.get(reader.get_name(), "path"), path)


if __name__ == '__main__':
    unittest.main(verbosity=2)
