#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject

Queue = None

class IProducer(INamedObject):

    def __init__(self, name="IProducer"):
        super(IProducer, self).__init__(name)
        self.__out_queue = None

    def create_producing_queue(self):
        global Queue
        try:
            if Queue is None:
                from multiprocessing import Queue
            self.__out_queue = Queue()
        except ImportError:
            self.__out_queue = None
        return self.__out_queue is not None

    def set_producing_queue(self, queue):
        self.__out_queue = queue

    def get_producing_queue(self):
        if self.__out_queue is None:
            self.create_producing_queue()
        return self.__out_queue

    def produce(self, *datas):
        self.__out_queue.put(*datas)
