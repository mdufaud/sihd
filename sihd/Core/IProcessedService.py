#!/usr/bin/python
#coding: utf-8

""" System """
import time

from .IService import IService
from .IConfigurable import IConfigurable
from .IProducer import IProducer
from .IConsumer import IConsumer

from .SihdWorker import SihdWorker

class IProcessedService(IService, IProducer, IConsumer):

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

    def _setup_impl(self):
        ret = super()._setup_impl()
        self.__workers_nbr = int(self.get_conf("process_workers"))
        return ret

    """ Worker method """

    def do_work(self, i, queue, data, producer):
        raise NotImplementedError("do_work not implemented")

    def on_worker_start(self, number, queue):
        self.log_debug("Worker[{}]: started".format(number))

    def on_worker_stop(self, number, total):
        self.log_debug("Worker[{}]: stopped".format(number))

    """ IService """

    def _start_impl(self):
        worker = SihdWorker(self)
        worker.set_work_method(self.do_work)
        worker.set_worker_number(self.__workers_nbr)
        self.__worker = worker 
        producers = self.get_producers()
        out_queue = self.get_producing_queue()
        if not out_queue:
            self.log_error("Not supporting multiprocessing queue")
        if not worker or not out_queue:
            return False
        if worker.make_workers(out_queue, producers):
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
