#!/usr/bin/python
#coding: utf-8

""" System """

import sys
import os
import time

""" Our Stuff """
import sihd

class TestApp(sihd.App.SihdApp):

    def __init__(self):
        super(TestApp, self).__init__("TestApp")
        self._default_log_level = "info"
        self.set_path(os.path.dirname(os.path.dirname((__file__))))
        sihd.Core.ALoggable.set_color(True)

    def on_setup(self):
        ret = super().on_setup()
        return ret

    def build_services(self):
        # Get args for app
        args = self.parse_args()
        # If time args has been set, will set a limited app loop
        if args.time:
            self.set_timed_loop(args.time)
        # Setting up LineReader
        reader = sihd.Readers.sys.LineReader("LineReader", self)
        reader.set_channel_conf("output", type='queue')
        # Set configuration for this reader
        self._configure_reader(reader, args)
        # Setting up WordHandler
        handler = sihd.Handlers.sys.WordHandler("WordHandler", self)
        handler.link('input', '..LineReader.output')
        # Set configuration for this handler
        self._configure_handler(handler)
        # Remember for further access those services
        self._word_handler = handler
        self._line_reader = reader
        return True

    def on_init(self):
        reader = self._line_reader
        handler = self._word_handler
        # Counter
        handler.processed.add_observer(self)
        reader.eof.add_observer(self)
        # Check reader state
        self.add_state_observer(reader)
        self.print_tree()
        return True

    def build_args(self, parser):
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

    def on_notify(self, channel):
        self.handle(channel)

    def handle(self, channel):
        eof = self._line_reader.eof
        lines = self._line_reader.lines
        processed = self._word_handler.processed
        if channel == processed and channel.read() == lines.read()\
                and eof.read():
            self.log_info("Reader stop")
            self._line_reader.stop()
        elif channel == eof and channel.read():
            self.log_info("EOF")
            if lines.read() == processed.read():
                self.log_info("Reader stop")
                self._line_reader.stop()

    def service_state_changed(self, service, stopped, paused):
        #Exit only if no gui attached
        self.is_handler(service)
        self.is_interactor(service)
        if self.guis:
            if self.is_gui(service) is False:
                return
        elif self.is_reader(service) is False:
            return
        self.log_info("Service state changed {} --to-- > {}".\
                format(service.get_name(), service.get_service_state_str()))
        if stopped:
            self.log_info("=== {} has stopped ===".format(service.get_name()))
            self.stop()

    def _configure_handler(self, handler):
        #Decorate with stats
        if self.args.stats:
            handler.step = sihd.Core.Stats.stat_it(handler.step)

    def _configure_reader(self, reader, args):
        reader.set_conf("path", args.file, force=True)
        #Decorate with stats
        if self.args.stats:
            reader.step = sihd.Core.Stats.stat_it(reader.step)
