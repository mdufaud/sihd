#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .. import Utilities

class IInteractor(Utilities.IService, Utilities.ILoggable,
                Utilities.IConfigurable, Utilities.IAppContainer):

    def __init__(self, app=None, name="IInteractor"):
        super(IInteractor, self).__init__(name)
        if (app):
            self.set_app(app)

    def set_app(self, app):
        super(IInteractor, self).set_app(app)
        app.add_interactor(self)

    def interact(self, data):
        raise NotImplementedError("Interact not implemented");
