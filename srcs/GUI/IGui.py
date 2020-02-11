#!/usr/bin/python
#coding: utf-8

""" System """

from sihd.srcs import Core

class IGui(Core.IService, Core.IAppContainer,
            Core.IObserver, Core.ILoggable,
            Core.IRunnable):

    def __init__(self, app=None, name="IGui"):
        super(IGui, self).__init__(name)
        self.set_step_method(self.gui_loop)
        if (app):
            self.set_app(app)

    """ IGui """

    def gui_loop(self, *args, **kwargs):
        raise NotImplementedError("gui_loop not implemented")

    def update(self, observable, *args):
        raise NotImplementedError("update not implemented")

    """ IObserver """

    def on_notify(self, observable, *args):
        if self.is_active():
            self.update(observable, *args)

    """ IAppContainer """

    def set_app(self, app):
        super(IGui, self).set_app(app)
        app.add_gui(self)
