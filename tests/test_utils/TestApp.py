#!/usr/bin/python
#coding: utf-8

""" System """

import sys
import os
import time

""" Our Stuff """
import sihd

class TestApp(sihd.App.IApp):

    def __init__(self, test_number):
        self._test = str(test_number)
        super(TestApp, self).__init__("TestApp" + self._test)
        self.set_path(os.path.dirname(os.path.dirname((__file__))))
        self.load_app_conf()
        sihd.Core.ILoggable.set_color(True)

    def _setup_app_impl(self):
        #Get args for app
        args = self.parse_args()
        #If time args has been set, will set a limited app loop
        if args.time:
            self.set_timed_loop(args.time)
        #Setting up LineReader
        reader = sihd.Readers.sys.LineReader(args.file, self, "LineReader" + self._test)
        #Set configuration for this reader
        self._configure_reader(reader)
        #Will be notified of state change
        reader.add_state_observer(self)
        #Setting up WordHandler
        handler = sihd.Handlers.sys.WordHandler(self, "WordHandler" + self._test)
        #Set configuration for this handler
        self._configure_handler(handler)
        reader.add_observer(handler)
        #Remember for further access those services
        self._word_handler = handler
        self._line_reader = reader
        return True

    def define_args(self, parser):
        """ Add arguments """
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
            handler.handle = sihd.Core.Stats.stat_it(handler.handle)

    def _configure_reader(self, reader):
        #Decorate with stats
        if self.args.stats:
            method = sihd.Core.Stats.stat_it(reader.get_run_method())
            reader.set_run_method(method)
