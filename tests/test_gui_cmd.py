#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import time
import unittest

import utils
import sihd
logger = sihd.set_log()

from sihd.GUI.Cmd.ICmdGui import ICmdGui
from sihd.App.IApp import IApp

class SimpleCmd(ICmdGui):

    def __init__(self, app=None, name="SimpleCmd"):
        super(SimpleCmd, self).__init__(app=app, name=name)
        self.set_intro("Welcome to SimpleCmd")
        self.__complete1 = ["say", 'yell', 'tell']
        self.__complete2 = ["magics", 'stories', 'shit']
        self.set_completion("complete", self.__complete1)
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
                    s += "clearly not something interesting"
            else:
                s += "nothing"
            print(s)
        else:
            print("Well you have to use autocomplete on this one")

class SimpleApp(IApp):

    def __init__(self, name):
        super(SimpleApp, self).__init__(name)

    def service_state_changed(self, service, stopped, paused):
        if self.is_gui(service) is False:
            return
        self.log_info("{}: {}".format(service.get_name(),
                            service.get_service_state()))
        if stopped:
            self.log_info("Stopping too")
            self.stop()

class TestGui(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_cmd_gui_app(self):
        print()
        app = SimpleApp("SomeApp")
        gui = SimpleCmd(app=app)
        gui.add_state_observer(app)
        gui.set_intro("Welcome in cmd gui inApp test")
        if app.setup_app() is False:
            return
        app.start()
        app.loop(timeout=20)
        app.stop()

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

if __name__ == '__main__':
    unittest.main(verbosity=2)
