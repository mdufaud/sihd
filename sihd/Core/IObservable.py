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

    def notify_observers(self, *datas):
        for observer in self.__observers:
            observer.on_notify(self, *datas)

    def notify_error(self, err):
        for observer in self.__observers:
            observer.on_error(self, err)

    def notify_info(self, info):
        for observer in self.__observers:
            observer.on_info(self, info)
