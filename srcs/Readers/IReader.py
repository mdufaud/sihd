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
        self.__notify = self.notify_observers

    def set_source(self, source):
        raise NotImplementedError("set_source not implemented")

    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)

    """ IDumpable """

    def _dump(self):
        state = self.__dict__.copy()
        del state['_log']
        del state['_run_method']
        del state['notify_observers']
        return state

    def is_saving_data(self):
        return self.__saving_data

    def save_data(self, active):
        self.__saving_data = active
        if active:
            self.notify_observers = self.__save_notify
        else:
            self.notify_observers = self.__notify

    def get_data_saved(self):
        return self.__data_saved

    def clear_data_saved(self):
        self.__data_saved = []

    def __save_notify(self, *args):
        self.__data_saved.append(args)
        return self.__notify(*args)
