#!/usr/bin/python
#coding: utf-8

""" System """

from .SihdService import SihdService
from .IThreadedService import IThreadedService
from .IProcessedService import IProcessedService

class ServiceType:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class IPolyService(IThreadedService, IProcessedService):

    __service_types = ServiceType(default=1, thread=2, process=3)
    __service_types_str = {
        "default": 1,
        "thread": 2,
        "process": 3,
    }

    def __init__(self, name="IPolyService", **kwargs):
        super(IPolyService, self).__init__(name, **kwargs)
        self._set_default_conf({
            "service_type": "default",
        })
        self.set_service_default()
        self.__service_types_fun = {
            1: self.set_service_default,
            2: self.set_service_threading,
            3: self.set_service_multiprocess
        }

    """ INamedObject """

    def _get_attributes(self):
        lst = super()._get_attributes()
        lst.append("type=" + self.__service_type_str)
        return lst

    def on_setup(self):
        ret = super().on_setup()
        if ret:
            service_type = self.get_conf("service_type")
            val = self.__service_types_str.get(service_type, None)
            if val is None:
                self.log_error("No such type {}".format(val))
                ret = False
            else:
                self.__service_types_fun[val]()
        return ret

    """ Channels """

    def create_channel(self, name, **kwargs):
        if self.__service_type == self.__service_types.default:
            return SihdService.create_channel(self, name, **kwargs)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService.create_channel(self, name, **kwargs)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService.create_channel(self, name, **kwargs)
        return None

    """ Thread """

    def set_service_threading(self):
        if self.is_configured():
            raise RuntimeError("Cannot set process type after setup")
        self.__service_type_str = "threading"
        self.__service_type = self.__service_types.thread

    def is_service_threading(self):
        return self.__service_type == self.__service_types.thread

    """ Process """

    def set_service_multiprocess(self):
        if self.is_configured():
            raise RuntimeError("Cannot set process type after setup")
        self.__service_type_str = "process"
        self.__service_type = self.__service_types.process

    def is_service_multiprocess(self):
        return self.__service_type == self.__service_types.process

    """ Service """

    def set_service_default(self):
        if self.is_configured():
            raise RuntimeError("Cannot set process type after setup")
        self.__service_type_str = "default"
        self.__service_type = self.__service_types.default

    """ IDumpable """

    def on_dump(self):
        dic = super().on_dump()
        dic.update({"service_type": self.__service_type})
        return dic

    def on_load(self, dic):
        super().on_load(dic)
        service_type = dic.get("service_type", None)
        if service_type:
            fun = self.__service_types_fun.get(service_type, None)
            if fun:
                fun()

    """ SihdService """

    def link_channel(self, name, new_channel):
        if self.__service_type == self.__service_types.process:
            return IProcessedService.link_channel(self, name, new_channel)
        return SihdService.link_channel(self, name, new_channel)

    def on_start(self):
        if self.__service_type == self.__service_types.default:
            return SihdService.on_start(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService.on_start(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService.on_start(self)
        return False

    def _start_impl(self):
        if self.__service_type == self.__service_types.default:
            return SihdService._start_impl(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService._start_impl(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService._start_impl(self)
        return False

    def _stop_impl(self):
        if self.__service_type == self.__service_types.default:
            return SihdService._stop_impl(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService._stop_impl(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService._stop_impl(self)
        return False

    def _pause_impl(self):
        if self.__service_type == self.__service_types.default:
            return SihdService._pause_impl(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService._pause_impl(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService._pause_impl(self)
        return False

    def _resume_impl(self):
        if self.__service_type == self.__service_types.default:
            return SihdService._resume_impl(self)
        elif self.__service_type == self.__service_types.thread:
            return IThreadedService._resume_impl(self)
        elif self.__service_type == self.__service_types.process:
            return IProcessedService._resume_impl(self)
        return False
