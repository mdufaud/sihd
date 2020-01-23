#!/usr/bin/python
#coding: utf-8

""" System """
import time

from .IService import IService
from .IConfigurable import IConfigurable
from .IWorker import IWorker

class IProcessedService(IService, IConfigurable, IWorker):

    def __init__(self, name="IProcessedService"):
        super(IProcessedService, self).__init__(name)
        self._set_default_conf({
            "process_workers": 1,
        })
        self.__process_start_time = None

    def get_process_start_time():
        return self.__process_start_time

    """ IConfigurable """

    def _setup_impl(self):
        workers = int(self.get_conf("process_workers"))
        self.set_worker_number(workers)
        self.make_workers(workers)
        return True

    """ IService """

    def _start_impl(self):
        self.__process_start_time = time.time()
        self.start_workers()
        return True

    def _stop_impl(self):
        self.stop_workers()
        return True

    def _pause_impl(self):
        self.pause_workers()
        return True

    def _resume_impl(self):
        self.resume_workers()
        return True
