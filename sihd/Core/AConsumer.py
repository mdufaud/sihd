#!/usr/bin/python
#coding: utf-8

from .ANamedObject import ANamedObject

queue = None

class AConsumer(ANamedObject):

    def __init__(self, name="AConsumer"):
        super(AConsumer, self).__init__(name)
        global queue
        if queue is None:
            import queue
        self.__consuming_queue = None

    def consume_queue(self, queue):
        self.__consuming_queue = queue

    def consume(self, block=False, timeout=None):
        data = None
        q = self.__consuming_queue
        if q is not None:
            try:
                data = q.get(block=block, timeout=timeout)
            except queue.Empty:
                pass
        return data
