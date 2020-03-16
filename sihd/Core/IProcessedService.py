#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

from .IService import IService
from .IConfigurable import IConfigurable

from .SihdWorker import SihdWorker

class IProcessedService(IService):

    def __init__(self, name="IProcessedService"):
        super(IProcessedService, self).__init__(name)
        self._set_default_conf({
            "process_workers": 1,
        })
        self.__worker = None
        self.__workers_nbr = 1
        self.__process_start_time = None

    def get_process_start_time(self):
        return self.__process_start_time

    """ IConfigurable """

    def do_setup(self):
        ret = super().do_setup()
        self.__workers_nbr = int(self.get_conf("process_workers"))
        return ret

    def do_channels(self):
        ret = super().do_channels()
        return ret

    """ Worker method """

    def do_work(self, i):
        raise NotImplementedError("do_work not implemented")

    def on_worker_start(self, number):
        self.log_debug("Worker[{}]: started (pid={})".format(number, os.getpid()))

    def on_worker_stop(self, number, total):
        self.log_debug("Worker[{}]: stopped".format(number))

    """ IService """

    def _start_impl(self):
        worker = SihdWorker(self)
        worker.set_work_method(self.do_work)
        worker.set_worker_number(self.__workers_nbr)
        self.__worker = worker
        input_channels = self.get_channels_input()
        output_channels = self.get_channels_output()
        for _, channel in input_channels.items():
            channel.clear_observers()
        if not worker:
            return False
        if worker.make_workers(input_channels, output_channels):
            if self.is_paused():
                self.__worker.pause_workers()
            if worker.start_workers():
                self.__process_start_time = time.time()
                return True
            else:
                self.log_error("Could not start workers")
        else:
            self.log_error("Could not make workers")
        return False 

    def _stop_impl(self):
        self.__worker.stop_workers()
        self.__worker = None
        return True

    def _pause_impl(self):
        self.__worker.pause_workers()
        return True

    def _resume_impl(self):
        self.__worker.resume_workers()
        return True
