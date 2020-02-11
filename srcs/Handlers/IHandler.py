#!/usr/bin/python
#coding: utf-8

""" System """

from sihd.srcs import Core

class IHandler(Core.IPolyService, Core.IAppContainer, Core.IObserver):

    def __init__(self, app=None, name="IHandler"):
        super(IHandler, self).__init__(name)
        if (app):
            self.set_app(app)

    """ IProcessedService """

    def do_work(self, i, queue, data, producer):
        return self.handle(producer, data)

    """ IObserver """

    def on_notify(self, observable, *args):
        if self.is_active():
            self.handle(observable, *args)

    """ IHandler """

    def handle(self, observable, *args):
        raise NotImplementedError("handle not implemented")

    """ IAppContainer """

    def set_app(self, app):
        super(IHandler, self).set_app(app)
        app.add_handler(self)
