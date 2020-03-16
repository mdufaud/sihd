#!/usr/bin/python
#coding: utf-8

""" System """

from .IService import IService
from .IThreadedService import IThreadedService
from .IProcessedService import IProcessedService
from .IDumpable import IDumpable

class ServiceType:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class IPolyService(IThreadedService, IProcessedService, IDumpable):

    __service_types = ServiceType(default=1, thread=2, process=3)
    __service_types_str = {
        "default": 1,
        "thread": 2,
        "process": 3,
    }

    def __init__(self, name="IPolyService"):
        super(IPolyService, self).__init__(name)
        self._set_default_conf({
            "service_type": "default",
        })
        self.set_service_default()
        self.__service_types_fun = {
            1: self.set_service_default,
            2: self.set_service_threading,
            3: self.set_service_multiprocess
        }

    def do_setup(self):
        ret = super().do_setup()
        if ret:
            service_type = self.get_conf("service_type")
            val = self.__service_types_str.get(service_type, None)
            if val is None:
                self.log_error("No such type {}".format(val))
                ret = False
            else:
                self.__service_types_fun[val]()
        return ret

    """ IProcessedService """

    def on_worker_start(self, number):
        super().on_worker_start(number)

    """ Thread """

    def set_service_threading(self):
        self.__service_type = self.__service_types.thread

    def is_service_threading(self):
        return self.__service_type == self.__service_types.thread

    """ Process """

    def set_service_multiprocess(self):
        self.__service_type = self.__service_types.process

    def is_service_multiprocess(self):
        return self.__service_type == self.__service_types.process

    """ Service """

    def set_service_default(self):
        self.__service_type = self.__service_types.default

    """ IDumpable """

    def _dump(self):
        dic = super()._dump()
        dic.update({"service_type": self.__service_type})
        return dic

    def _load(self, dic):
        super()._load(dic)
        service_type = dic.get("service_type", None)
        if service_type:
            fun = self.__service_types_fun.get(service_type, None)
            if fun:
                fun()

    """ IService """

    def _start_impl(self):
        if self.__service_type == self.__service_types.default:
            return IService._start_impl(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService._start_impl(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService._start_impl(self)
        return False

    def _stop_impl(self):
        if self.__service_type == self.__service_types.default:
            return IService._stop_impl(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService._stop_impl(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService._stop_impl(self)
        return False

    def _pause_impl(self):
        if self.__service_type == self.__service_types.default:
            return IService._pause_impl(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService._pause_impl(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService._pause_impl(self)
        return False

    def _resume_impl(self):
        if self.__service_type == self.__service_types.default:
            return IService._resume_impl(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService._resume_impl(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService._resume_impl(self)
        return False
