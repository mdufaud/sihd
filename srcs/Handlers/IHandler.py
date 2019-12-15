#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .. import Utilities

class IHandler(Utilities.IService, Utilities.IAppContainer,
                Utilities.IObservable, Utilities.IObserver,
                Utilities.IConfigurable, Utilities.IDumpable):

    def __init__(self, app=None, name="IHandler"):
        super(IHandler, self).__init__(name)
        self.listeners = set()
        if (app):
            self.set_app(app)

    def set_app(self, app):
        super(IHandler, self).set_app(app)
        app.add_handler(self)
