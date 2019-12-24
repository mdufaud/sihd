#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import sys
import os
import time
import argparse

""" Our Stuff """
import sihd

class TestApp(sihd.App.IApp):

    def __init__(self, test_number):
        self._test = str(test_number)
        super(TestApp, self).__init__("TestApp" + self._test)
        self.load_app_conf()
        sihd.srcs.Utilities.ILoggable.set_color(True)

    def _setup_impl(self):
        args = self.parse_args()
        if args.time:
            self.set_timed_loop(args.time)
        reader = sihd.Readers.sys.LineReader(args.file, self, "LineReader" + self._test)
        self._configure_reader(reader)
        reader.add_state_observer(self)
        handler = sihd.Handlers.sys.WordHandler(self, "WordHandler" + self._test)
        self._configure_handler(handler)
        #gui = Wifimapper.GUI.WxPython.dot11.WxPythonDot11Gui(self)
        reader.add_observer(handler)
        #handler.add_observer(gui)
        self._word_handler = handler
        self._line_reader = reader
        return True

    def define_args(self):
        """ Create arguments """
        parser = argparse.ArgumentParser(prog='TestApp' + self._test)
        parser.add_argument("-f", "--file",
                type=str,
                default=None,
                help="Read specified file")
        parser.add_argument("-s", "--stats",
                action='store_true',
                default=False,
                help="Print stats when printing results")
        parser.add_argument("-t", "--time",
                type=int,
                default=None,
                help="Timer until stop")
        return parser

    def service_state_changed(self, service, stopped, paused):
        #Exit only if no gui attached
        if self.guis:
            return
        if self.is_reader(service) is False:
            return
        self.log_info(service.get_state())
        if stopped:
            self.log_info("Reader {} ended and stopped".format(service.get_name()))

    def _configure_handler(self, handler):
        #Decorate with stats
        if self.args.stats:
            handler.handle = sihd.Utilities.Stats.stat_it(handler.handle)

    def _configure_reader(self, reader):
        #Decorate with stats
        if self.args.stats:
            method = sihd.Utilities.Stats.stat_it(reader.get_run_method())
            reader.set_run_method(method)
