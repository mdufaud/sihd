#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .INamedObject import INamedObject

class IObservable(INamedObject):

    def __init__(self, name="IObservable"):
        super(IObservable, self).__init__(name)
        self.handlers = set()

    def add_observer(self, handler):
        assert(handler.handle)
        self.handlers.add(handler)

    def notify_observers(self, data):
        for handler in self.handlers:
            handler.handle(self, data)

    def notify_error(self, error):
        for handler in self.handlers:
            handler.on_error(self, error)

    def notify_info(self, info):
        for handler in self.handlers:
            handler.on_info(self, info)
