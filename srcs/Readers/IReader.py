#!/usr/bin/python
#coding: utf-8

""" System """

from sihd.srcs import Core
import time

class IReader(Core.IThreadedService, Core.IAppContainer,
                Core.IObservable, Core.IDumpable, Core.IProducer):

    def __init__(self, app=None, name="IReader"):
        super(IReader, self).__init__(name)
        self.__saving_data = False
        self.__is_multiprocess = False
        self.__data_saved = []
        if (app):
            self.set_app(app)
        self.__notify = self.notify_observers

    """ IObservable """

    def notify_observers(self, *args):
        if self.__is_multiprocess:
            self.produce(*args)
        else:
            super().notify_observers(*args)

    """ IAppContainer """

    def set_app(self, app):
        super(IReader, self).set_app(app)
        app.add_reader(self)

    """ IDumpable """

    def _dump(self):
        state = super()._dump()
        del state['_log']
        del state['_run_method']
        del state['notify_observers']
        del state['_IProducer__out_queue']
        return state

    """ Reader """

    def set_source(self, source):
        raise NotImplementedError("set_source not implemented")

    def set_multiprocess(self, active):
        self.__is_multiprocess = active

    def is_saving_data(self):
        return self.__saving_data

    def save_data(self, active):
        self.__saving_data = active
        if active:
            self.notify_observers = self.__save_notify(self.notify_observers)
        else:
            self.notify_observers = self.__notify

    def get_data_saved(self):
        return self.__data_saved

    def clear_data_saved(self):
        self.__data_saved = []

    def __save_notify(self, fun):
        def wrapper(*args):
            self.__data_saved.append(args)
            return fun(*args)
        return wrapper
