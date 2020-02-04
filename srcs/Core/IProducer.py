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

    def get_producing_queue(self):
        return self.__out_queue

    def produce(self, *datas):
        if len(datas) == 1:
            datas = datas[0]
        try:
            self.__out_queue.put(datas)
        except queue.Full as e:
            self.log_warning("Queue full: {}".format(e))
