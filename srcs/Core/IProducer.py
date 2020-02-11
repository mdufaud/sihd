#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject

Queue = None

class IProducer(INamedObject):

    def __init__(self, name="IProducer"):
        global Queue
        try:
            if Queue is None:
                from multiprocessing import Queue
            self.__out_queue = Queue()
        except ImportError:
            self.__out_queue = None
        super(IProducer, self).__init__(name)

    def set_out_queue(self, queue):
        self.__out_queue = queue

    def get_producing_queue(self):
        return self.__out_queue

    def produce(self, *datas):
        self.__out_queue.put(*datas)
