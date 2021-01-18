#!/usr/bin/python
# coding: utf-8

#
# System
#
import os
import os.path
import sys
import logging

import sihd
from sihd.gui.AGui import AGui
from sihd import core

from functools import update_wrapper

flask = None
make_server = None

class AFlaskGui(AGui):

    def __init__(self, name="AFlaskGui", *args, **kwargs):
        global flask
        if flask is None:
            import flask
        global make_server
        if make_server is None:
            from werkzeug.serving import make_server
        super().__init__(name=name, *args, **kwargs)
        self.configuration.add_defaults({
            'host': "127.0.0.1",
            'port': 5000,
            'flask': {
                'TESTING': False,
            },
            'secret_key': 'random',
            'web_path': "",
            'resources_path': [],
        })
        self.configuration.set('runnable_type', 'thread')
        self.server = None
        self.render_ctx = {}
        self.__base_methods = {}

    def on_reset(self):
        self._load_base_methods()
        return super().on_reset()

    def _add_base_method(self, name, func):
        self.__base_methods[name] = func

    def _load_base_methods(self):
        for name, func in self.__base_methods.items():
            setattr(self, name, func)
        self.__base_methods.clear()

    @staticmethod
    def generate_random_key():
        return os.urandom(16)

    def configure_flask_app(self, app, config):
        flask_conf = config.get("flask")
        sk = config.get("secret_key")
        if sk == "random":
            sk = self.generate_random_key()
        elif isinstance(sk, str):
            sk = sk.encode()
        app.secret_key = sk
        app.config.update(flask_conf)

    def create_flask_app(self, path):
        self.log_info("Flask app directory: {}".format(path))
        app = flask.Flask(self.get_name(), root_path=path)
        self.configure_flask_app(app, self.configuration)
        self.add_context_processor(app)
        return app

    def create_server(self, app):
        return make_server('127.0.0.1', self.configuration.get('port'), app)

    def stop_server(self):
        self.server.shutdown()

    def run_server(self):
        self.server.serve_forever()

    def build_routes(self, app):
        pass

    #override
    def on_setup(self, config):
        ret = super().on_setup(config)
        res_paths = config.get('resources_path')
        for path in res_paths:
            sihd.path.add(path)
        path = self.configuration.get("web_path")
        self.flask_app = self.create_flask_app(path)
        self.server = self.create_server(self.flask_app)
        self.build_routes(self.flask_app)
        return ret

    #override
    def loop(self, **kwargs):
        self.log_info("Starting server")
        self.run_server()

    def on_stop(self):
        self.stop_server()
        self.server = None
        self.flask_app = None
        return super().on_stop()

    #
    # Flask wrapping
    #

    def route(self, methodname, route, base=None, **kwargs):
        app = base or self.flask_app
        return self.flask_wrap(methodname, app.route, route, **kwargs)

    def errorhandler(self, methodname, number, base=None):
        app = base or self.flask_app
        self.flask_wrap(methodname, app.errorhandler, number)

    def flask_wrap(self, methodname, wrapper, *args, **kwargs):
        try:
            method = getattr(self, methodname)
        except AttributeError:
            self.log_error("Method to wrap not found: {}".format(methodname))
            return False
        if not callable(method):
            self.log_error("Tried to wrap a non callable: {}".format(methodname))
            return False
        self._add_base_method(methodname, method)
        setattr(self, methodname, wrapper(*args, **kwargs)(method))
        return True

    def flask_add_ctx(self):
        return self.render_ctx

    def add_context_processor(self, app_or_bp, method=None):
        method = method or self.flask_add_ctx
        app_or_bp.context_processor(method)
