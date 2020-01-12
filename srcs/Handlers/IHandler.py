#!/usr/bin/python
#coding: utf-8

""" System """

from sihd.srcs import Core

class IHandler(Core.IService, Core.IAppContainer,
                Core.IObservable, Core.IObserver,
                Core.IConfigurable, Core.IDumpable):

    def __init__(self, app=None, name="IHandler"):
        super(IHandler, self).__init__(name)
        self.listeners = set()
        if (app):
            self.set_app(app)

    def on_notify(self, observable, *args, **kwargs):
        if self.is_active():
            self.handle(observable, *args, **kwargs)

    def handle(self, observable, *args, **kwargs):
        raise NotImplementedError("handle not implemented")

    def set_app(self, app):
        super(IHandler, self).set_app(app)
        app.add_handler(self)

    def _start_impl(self):
        return True

    def _stop_impl(self):
        return True
