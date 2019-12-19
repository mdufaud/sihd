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

    def notify_observers(self, *args):
        for handler in self.handlers:
            handler.handle(self, *args)

    def notify_error(self, *args):
        for handler in self.handlers:
            handler.on_error(self, *args)

    def notify_info(self, *args):
        for handler in self.handlers:
            handler.on_info(self, *args)
