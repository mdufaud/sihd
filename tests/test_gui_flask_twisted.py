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

from sihd.gui.flask.AFlaskTwistedGui import AFlaskTwistedGui
from sihd.readers.AReader import AReader

try:
    from markupsafe import escape
    from flask import (
        render_template,
        session, redirect, url_for, request,
        make_response, abort, Blueprint, g, flash
    )
    imprt = None
except ImportError as imprt:
    pass

class SimpleFlask(AFlaskTwistedGui):

    def __init__(self, name="SimpleFlask", app=None):
        super(SimpleFlask, self).__init__(app=app, name=name)
        self.render_ctx['sihd_gui'] = self
        self.render_ctx['channels'] = {}
        self.clients = {}
        self.idx = 1

    def on_setup(self, config):
        self.render_ctx['ws_port'] = self.configuration.get('port')
        return super().on_setup(config)

    # Websocket handling

    def ws_connect(self, handler, request):
        self.log_info("Client connecting: {0}".format(request.peer))

    def ws_open(self, handler):
        self.clients[handler] = self.idx
        self.log_info("Client[{0}]: Websocket opened".format(self.idx))
        self.idx += 1

    def ws_message(self, handler, msg, is_bin):
        client = self.clients[handler]
        if is_bin:
            self.log_info("Client[{0}]: Binary message received: {1} bytes"\
                          .format(client, len(msg)))
        else:
            self.log_info("Client[{0}]: Text message received: {1}"\
                          .format(client, msg.decode('utf-8')))
        # Echo
        handler.sendMessage(msg, is_bin)

    def ws_close(self, handler, was_clean, code, reason):
        client = self.clients[handler]
        del self.clients[handler]
        self.log_info(("Client[{0}]: Websocket closed (clean: {1} - "
                        "code: {2} - reason: {3})").format(client, was_clean,
                                                            code, reason))

    # Sihd

    def handle(self, channel):
        self.render_ctx['channels'][channel.get_path()] = channel.read()

    # Flask

    def sessions(self):
        return render_template('twisted.html')

    # Build

    def build_routes(self, app):
        self.route("sessions", "/")

class InfiniteReader(AReader):

    def __init__(self, *args, **kwargs):
        super(InfiniteReader, self).__init__(*args, **kwargs)
        self.configuration.set("runnable_frequency", 10)
        self.add_channel('output', type='counter')

    def on_step(self):
        self.output.write()
        return True

class TestFlaskTwistedGui(unittest.TestCase):

    def setUp(self):
        sihd.resources.add("tests", "resources", "gui", "flask")
        print()

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_gui(self):
        print()
        provider = InfiniteReader(name='provider')
        gui = SimpleFlask()
        gui.configuration.load({
            "web_path": sihd.resources.get("www"),
            "flask": {
                "TESTING": True,
            },
        })
        self.assertTrue(gui.setup())
        self.assertTrue(provider.start())
        channel = gui.find('provider.output')
        channel.add_observer(gui)
        print(gui.flask_app.url_map)
        self.assertTrue(gui.start())
        try:
            while gui.is_running():
                time.sleep(0.2)
        except KeyboardInterrupt:
            pass
        self.assertTrue(gui.stop())
        self.assertTrue(provider.stop())

if __name__ == '__main__':
    if imprt is None:
        unittest.main(verbosity=2)
    else:
        logger.error("Requirements not installed: {}".format(imprt))
