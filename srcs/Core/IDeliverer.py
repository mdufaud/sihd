#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject
from .IProducer import IProducer
from .IObservable import IObservable

class IDeliverer(IProducer, IObservable):

    def __init__(self, name="IDeliverer"):
        super(IDeliverer, self).__init__(name)
        self.deliver = self.__deliver_obs

    def set_service_multiprocess(self):
        self.deliver = self.__deliver_mp

    def set_observable(self):
        self.deliver = self.__deliver_obs

    def __deliver_mp(self, *data):
        return self.produce(*data)

    def __deliver_obs(self, *data):
        return self.notify_observers(*data)

    def deliver(self, *data):
        return
