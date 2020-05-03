#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject

class IObservable(INamedObject):

    def __init__(self, name="IObservable", **kwargs):
        super(IObservable, self).__init__(name, **kwargs)
        self.__observers = set()

    def add_observer(self, observer):
        if not getattr(observer, "get_name"):
            raise NotImplementedError("Observer does not implement 'get_name'")
        if not getattr(observer, "on_notify"):
            raise NotImplementedError("Observer {} does not implement 'on_notify'"\
                                        .format(observer.get_name()))
        self.__observers.add(observer)

    def notify_observers(self):
        for observer in self.__observers:
            observer.on_notify(self)

    def remove_observer(self, observer):
        try:
            self.__observers.remove(observer)
        except KeyError:
            return False
        return True

    def clear_observers(self):
        self.__observers = set()
