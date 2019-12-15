#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import os

from .. import Utilities

class IReader(Utilities.IService, Utilities.IAppContainer,
                Utilities.IObservable, Utilities.IConfigurable,
                Utilities.IRunnable, Utilities.IDumpable):

    def __init__(self, app=None, name="IReader"):
        super(IReader, self).__init__(name)
        if (app):
            self.set_app(app)
        self._saving_data = False
        self._reading = False

    def set_source(self, source):
        return

    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)

    """ IDumpable """

    def save_data(self):
        self._saving_data = not self._saving_data
