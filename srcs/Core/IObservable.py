#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject

class IObservable(INamedObject):

    def __init__(self, name="IObservable"):
        super(IObservable, self).__init__(name)
        self.__observers = set()

    def add_observer(self, observer):
        self.__observers.add(observer)

    def notify_observers(self, *args):
        for observer in self.__observers:
            observer.on_notify(self, *args)

    def notify_error(self, *args):
        for observer in self.__observers:
            observer.on_error(self, *args)

    def notify_info(self, *args):
        for observer in self.__observers:
            observer.on_info(self, *args)
