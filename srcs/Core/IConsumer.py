#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject

queue = None

class IConsumer(INamedObject):

    def __init__(self, name="IConsumer"):
        super(IConsumer, self).__init__(name)
        global queue
        if queue is None:
            import queue
        self.__conso = {}

    def get_consumables(self):
        return self.__conso.items()

    def add_to_consume(self, producer):
        queue = producer.get_producing_queue()
        self.__conso[producer] = queue

    def remove_all_consumation(self):
        self.__conso = {}

    def remove_consumation(self, name):
        return self.__conso.pop(name, None)

    def consume(self):
        dic = self.__conso
        i = 0
        for producer, queue in dic.items():
            try:
                data = queue.get()
            except queue.Empty:
                continue
            i += 1
            self.consumed(producer, data)
        return i

    def consumed(self, producer, data):
        raise NotImplementedError("consumed not implemented")
