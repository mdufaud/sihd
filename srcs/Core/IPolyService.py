#!/usr/bin/python
#coding: utf-8

""" System """

from .IService import IService
from .IThreadedService import IThreadedService
from .IProcessedService import IProcessedService
from .IDeliverer import IDeliverer
from .IDumpable import IDumpable

class ServiceType:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class IPolyService(IThreadedService, IProcessedService, IDeliverer, IDumpable):

    __service_types = ServiceType(default=1, thread=2, process=3)

    def __init__(self, name="IPolyService"):
        super(IPolyService, self).__init__(name)
        self.__default_deliver = self.deliver
        self.set_service_default()

    """ IProcessedService """

    def on_worker_start(self, number, queue):
        self.deliver = self.__worker_delivery
        super().on_worker_start(number, queue)

    def __worker_delivery(self, *data):
        queue = self.get_producing_queue()
        queue.put(*data)

    """ Thread """

    def set_service_threading(self):
        self.__type = self.__service_types.thread

    def is_service_threading(self):
        return self.__type == self.__service_types.thread

    """ Process """

    def set_service_multiprocess(self):
        self.__type = self.__service_types.process

    def is_service_multiprocess(self):
        return self.__type == self.__service_types.process

    """ Service """

    def set_service_default(self):
        self.__type = self.__service_types.default

    """ IDumpable """

    def _dump(self):
        dic = super()._dump()
        dic.update({"service_type": self.__type})
        return dic

    def _load(self, dic):
        super()._load(dic)
        service_type = dic.get("service_type", None)
        if service_type:
            self.__type = service_type

    """ IService """

    def _start_impl(self):
        if self.__type == self.__service_types.default:
            return IService._start_impl(self)
        elif self.__type == self.__service_types.thread:
            return IThreadedService._start_impl(self)
        elif self.__type == self.__service_types.process:
            return IProcessedService._start_impl(self)
        return False

    def _stop_impl(self):
        if self.__type == self.__service_types.default:
            return IService._stop_impl(self)
        elif self.__type == self.__service_types.thread:
            return IThreadedService._stop_impl(self)
        elif self.__type == self.__service_types.process:
            return IProcessedService._stop_impl(self)
        return False

    def _pause_impl(self):
        if self.__type == self.__service_types.default:
            return IService._pause_impl(self)
        elif self.__type == self.__service_types.thread:
            return IThreadedService._pause_impl(self)
        elif self.__type == self.__service_types.process:
            return IProcessedService._pause_impl(self)
        return False

    def _resume_impl(self):
        if self.__type == self.__service_types.default:
            return IService._resume_impl(self)
        elif self.__type == self.__service_types.thread:
            return IThreadedService._resume_impl(self)
        elif self.__type == self.__service_types.process:
            return IProcessedService._resume_impl(self)
        return False
