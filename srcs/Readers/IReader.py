#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .. import Utilities

class IReader(Utilities.ISequenceable, Utilities.IAppContainer,
                Utilities.IObservable, Utilities.IDumpable):

    def __init__(self, app=None, name="IReader"):
        super(IReader, self).__init__(name)
        self._saving_data = False
        if (app):
            self.set_app(app)

    def set_source(self, source):
        return


    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)

    """ IDumpable """

    def save_data(self):
        self._saving_data = not self._saving_data
