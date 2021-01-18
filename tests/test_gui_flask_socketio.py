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

from sihd.gui.flask.AFlaskSocketioGui import AFlaskSocketioGui
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

class SimpleFlask(AFlaskSocketioGui):

    def __init__(self, name="SimpleFlask", app=None):
        super(SimpleFlask, self).__init__(app=app, name=name)
        self.render_ctx['sihd_gui'] = self
        self.render_ctx['channels'] = {}

    # Sihd

    def handle(self, channel):
        self.render_ctx['channels'][channel.get_path()] = channel.read()

    # Flask

    def sessions(self):
        return render_template('socketio.html')

    # Socketio

    def message_received(self, methods=['GET', 'POST']):
        print('Message was received !')

    def custom_event(self, json, methods=['GET', 'POST']):
        print('Received my event: ' + str(json))
        if 'message' in json and json['message'] == 'stop':
            self.stop()
        else:
            self.socketio.emit('response', json, callback=self.message_received)

    def custom_error(self, err):
        print("ERROR " + err)

    def default_error(self, err):
        print('DEFAULT ERROR ' + err)

    # Build

    def build_routes(self, app):
        self.route("sessions", "/")

    def build_events(self, socketio):
        self.on_event('custom_event', 'data')
        self.on_error('custom_error')
        socketio.on_error_default(self.default_error)

        @self.socketio.on('event')
        def handle_my_custom_event(json, methods=['GET', 'POST']):
            print('Custom event: ' + str(json))


class InfiniteReader(AReader):

    def __init__(self, *args, **kwargs):
        super(InfiniteReader, self).__init__(*args, **kwargs)
        self.configuration.set("runnable_frequency", 10)
        self.add_channel('output', type='counter')

    def on_step(self):
        self.output.write()
        return True

class TestFlaskSocketioGui(unittest.TestCase):

    def setUp(self):
        sihd.path.add("tests", "resources", "gui", "flask")
        print()

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_gui(self):
        print()
        provider = InfiniteReader(name='provider')
        gui = SimpleFlask()
        gui.configuration.load({
            "web_path": sihd.path.get("www"),
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
