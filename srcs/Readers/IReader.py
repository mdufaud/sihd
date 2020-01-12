#!/usr/bin/python
#coding: utf-8

""" System """

from sihd.srcs import Core
import time

class IReader(Core.ISequenceable, Core.IAppContainer,
                Core.IObservable, Core.IDumpable):

    def __init__(self, app=None, name="IReader"):
        super(IReader, self).__init__(name)
        self.__saving_data = False
        self.__data_saved = []
        if (app):
            self.set_app(app)

    def set_source(self, source):
        raise NotImplementedError("set_source not implemented")

    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)

    """ IDumpable """

    def is_saving_data(self):
        return self.__saving_data

    def save_data(self, active):
        self.__saving_data = active

    def get_data_saved(self):
        return self.__data_saved

    def clear_data_saved(self):
        self.__data_saved = []

    def notify_observers(self, *args):
        if self.__saving_data:
            now = time.time()
            self.__data_saved.append((now, *args))
        return super().notify_observers(*args)
