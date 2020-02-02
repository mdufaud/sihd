#!/usr/bin/python
#coding: utf-8

""" System """

from sihd.srcs import Core

class IGui(Core.IService, Core.IAppContainer,
            Core.IObserver, Core.ILoggable,
            Core.IConfigurable, Core.IRunnable):

    def __init__(self, app=None, name="IGui"):
        super(IGui, self).__init__(name)
        self.set_run_method(self.gui_loop)
        if (app):
            self.set_app(app)

    def gui_loop(self, *args, **kwargs):
        raise NotImplementedError("gui_loop not implemented")

    def on_notify(self, observable, *args, **kwargs):
        if self.is_active():
            self.update(observable, *args, **kwargs)

    def set_app(self, app):
        super(IGui, self).set_app(app)
        app.add_gui(self)

    def update(self, observable, *args, **kwargs):
        raise NotImplementedError("update not implemented")
