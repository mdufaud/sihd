#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .. import Utilities

class IGui(Utilities.IService, Utilities.IAppContainer,
            Utilities.IObserver, Utilities.ILoggable,
            Utilities.IConfigurable, Utilities.IRunnable):

    def __init__(self, app=None, name="IGui"):
        super(IGui, self).__init__(name)
        if (app):
            self.set_app(app)

    def set_app(self, app):
        super(IGui, self).set_app(app)
        app.add_gui(self)

    def update(self): 
        raise NotImplementedError("Update not implemented")
