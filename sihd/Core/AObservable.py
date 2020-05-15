#!/usr/bin/python
#coding: utf-8

from .ANamedObject import ANamedObject

class AObservable(ANamedObject):

    def __init__(self, name="AObservable", **kwargs):
        super(AObservable, self).__init__(name, **kwargs)
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
