#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .INamedObject import INamedObject

class IObservable(INamedObject):

    def __init__(self, name="IObservable"):
        super(IObservable, self).__init__(name)
        self.observers = set()

    def add_observer(self, observer):
        assert(observer.on_notify)
        assert(observer.on_info)
        assert(observer.on_error)
        self.observers.add(observer)

    def notify_observers(self, *args):
        for observer in self.observers:
            observer.on_notify(self, *args)

    def notify_error(self, *args):
        for observer in self.observers:
            observer.on_error(self, *args)

    def notify_info(self, *args):
        for observer in self.observers:
            observer.on_info(self, *args)
