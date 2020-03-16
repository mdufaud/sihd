#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject

queue = None
multiprocessing = None

class IProducer(INamedObject):

    def __init__(self, name="IProducer", size=0):
        super(IProducer, self).__init__(name)
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        global queue
        if queue is None:
            import queue
        self.__out_queue = multiprocessing.Queue(maxsize=size)

    def set_producing_queue(self, queue):
        self.__out_queue = queue

    def get_producing_queue(self):
        return self.__out_queue

    def is_queue_empty(self):
        return self.__out_queue.empty()

    def produce(self, data, block=False, timeout=None):
        q = self.__out_queue
        try:
            q.put(data, block=block, timeout=timeout)
        except queue.Full:
            return False
        return True
