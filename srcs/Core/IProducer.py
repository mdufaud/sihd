#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject

multiprocessing = None
queue = None

class IProducer(INamedObject):

    def __init__(self, name="IProducer"):
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        global queue
        if queue is None:
            import queue
        super(IProducer, self).__init__(name)
        self.__out_queue = multiprocessing.Queue()

    def get_producing_queue(self):
        return self.__out_queue

    def produce(self, *datas):
        if len(datas) == 1:
            datas = datas[0]
        try:
            self.__out_queue.put(datas)
        except queue.Full as e:
            self.log_warning("Queue full: {}".format(e))
