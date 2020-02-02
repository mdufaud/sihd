#!/usr/bin/python
#coding: utf-8

""" System """

import os
import sys
import time

import test_utils
import sihd

""" Setting up basic logging """

import logging
logger = logging.getLogger()
logging.basicConfig(level=logging.DEBUG)

import unittest

from sihd.srcs.GUI.Cmd.ICmdGui import ICmdGui

class SimpleCmd(ICmdGui):

    def __init__(self, app=None, name="SimpleCmd"):
        super(SimpleCmd, self).__init__(app=app, name=name)
        self.set_intro("Welcome to SimpleCmd")

class TestGui(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_cmd_gui_app(self):
        print()
        app = test_utils.TestApp("WithCmd")
        gui = SimpleCmd(app=app)
        gui.add_state_observer(app)
        gui.set_intro("Welcome in cmd gui inApp test")
        dir_path = os.path.join(os.path.dirname(__file__), "resources", "Txt")
        path = os.path.join(dir_path, "5_lines.txt")
        if not app.is_args():
            app.set_args([
                "-f", path,
                "-s",
            ])
        if app.setup_app() is False:
            sys.exit(1)
        app.start()
        app.loop(timeout=60)
        app.stop()

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_simple_cmd_gui(self):
        print()
        gui = SimpleCmd()
        self.assertTrue(gui.setup())
        self.assertTrue(gui.start())
        try:
            time.sleep(200)
        except KeyboardInterrupt:
            pass
        self.assertTrue(gui.stop())

if __name__ == '__main__':
    unittest.main(verbosity=2)
