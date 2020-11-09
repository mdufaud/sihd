#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import time
import unittest

import utils
import sihd
logger = sihd.log.setup('info')

from sihd.gui.cmd.ACmdGui import ACmdGui
from sihd.app.SihdApp import SihdApp

class SimpleCmd(ACmdGui):

    def __init__(self, name="SimpleCmd", app=None):
        super(SimpleCmd, self).__init__(app=app, name=name)
        self.set_intro("Welcome to SimpleCmd")
        self.__complete1 = ["say", 'yell', 'tell']
        self.__complete2 = ["magics", 'stories', 'shit']
        self.set_completion("complete", self.__complete1)
        self.set_completion("complete_yell", ['stuff'])
        self.set_completion("complete_any", self.__complete2)

    def help_test(self):
        print("Simple test to log stuff")

    def do_test(self, args):
        self.log_info("Test ! -> {}".format(args))

    def help_complete(self):
        print("Simple test to auto complete stuff")

    def do_complete(self, args):
        if not args:
            print("You have to do something ! Use tab for completion")
            return
        split = args.split()
        if split[0] in self.__complete1:
            s = "You {} ".format(split[0])
            if len(split) == 2:
                if split[1] in self.__complete2:
                    s += split[1]
                else:
                    s += "something uninteresting"
            else:
                s += "nothing"
            print(s)
        else:
            print("Well you have to use autocomplete on this one")

class SimpleApp(SihdApp):

    def __init__(self, name):
        super(SimpleApp, self).__init__(name)
        dirname = os.path.dirname
        join = os.path.join
        self.set_app_path(join(dirname(__file__), 'output'))

    def service_state_changed(self, service, stopped, paused):
        if self.is_gui(service) is False:
            return
        if stopped:
            self.stop()

    def build_services(self):
        self.gui = SimpleCmd(app=self)
        self.gui.set_intro("Welcome in cmd gui inApp test")
        return True

    def on_init(self):
        self.add_state_observer(self.gui)

class TestCmdGui(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_simple_cmd_gui(self):
        print()
        gui = SimpleCmd()
        self.assertTrue(gui.setup())
        self.assertTrue(gui.start())
        try:
            while gui.is_running():
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        self.assertTrue(gui.stop())

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_cmd_gui_app(self):
        print()
        app = SimpleApp("SomeApp")
        if app.setup_app() is False:
            return
        app.start()
        app.loop(timeout=20)
        app.stop()

if __name__ == '__main__':
    unittest.main(verbosity=2)
