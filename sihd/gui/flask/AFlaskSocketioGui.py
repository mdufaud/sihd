#!/usr/bin/python
# coding: utf-8

#
# System
#

from .AFlaskGui import AFlaskGui

SocketIO = None

class AFlaskSocketioGui(AFlaskGui):

    def __init__(self, name="AFlaskSocketioGui", *args, **kwargs):
        global SocketIO
        if SocketIO is None:
            from flask_socketio import SocketIO
        super().__init__(name=name, *args, **kwargs)

    def build_events(self, socketio):
        pass

    #
    # Create socketio server
    #

    def on_setup(self, config):
        ret = super().on_setup(config)
        self.build_events(self.socketio)
        return ret

    def create_server(self, app):
        socketio = SocketIO(app)
        self.socketio = socketio
        return socketio

    def run_server(self):
        self.socketio.run(self.flask_app)

    def stop_server(self):
        self.socketio.stop()

    #
    # Socketio wrapping
    #

    def on_event(self, methodname, event, base=None, **kwargs):
        socketio = base or self.socketio
        return self.socketio_wrap(methodname, socketio.on, event, **kwargs)

    def on_error(self, methodname, namespace=None, base=None):
        socketio = base or self.socketio
        return self.socketio_wrap(methodname, socketio.on_error, namespace=namespace)

    def socketio_wrap(self, methodname, wrapper, *args, **kwargs):
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
