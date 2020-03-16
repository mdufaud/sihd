#!/usr/bin/python
#coding: utf-8

""" System """

from sihd import Core

class IHandler(Core.IPolyService, Core.IAppContainer):

    def __init__(self, app=None, name="IHandler"):
        super(IHandler, self).__init__(name)
        if (app):
            self.set_app(app)

    """ IProcessedService """

    def do_work(self, i):
        return self.step_method()

    """ IObserver """

    def on_notify(self, channel):
        if self.is_active():
            self.handle(channel)

    """ IHandler """

    def handle(self, channel):
        raise NotImplementedError("handle not implemented")

    """ IAppContainer """

    def set_app(self, app):
        super(IHandler, self).set_app(app)
        app.add_handler(self)
