#!/usr/bin/python
#coding: utf-8

""" System """

from sihd.srcs import Core

class IGui(Core.IService, Core.IAppContainer,
            Core.IObserver, Core.ILoggable,
            Core.IConfigurable, Core.IRunnable):

    def __init__(self, app=None, name="IGui"):
        super(IGui, self).__init__(name)
        if (app):
            self.set_app(app)

    def on_notify(self, observable, *args, **kwargs):
        if self.is_active():
            self.handle(observable, *args, **kwargs)

    def handle(self, observable, *args, **kwargs):
        raise NotImplementedError("handle not implemented")

    def set_app(self, app):
        super(IGui, self).set_app(app)
        app.add_gui(self)

    def update(self): 
        raise NotImplementedError("update not implemented")
