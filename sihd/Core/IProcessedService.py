#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

from .IService import IService
from .IConfigurable import IConfigurable

from .SihdWorker import SihdWorker

from .Channel import ChannelArray, ChannelValue

class IProcessedService(IService):

    def __init__(self, name="IProcessedService"):
        super(IProcessedService, self).__init__(name)
        self._set_default_conf({
            "process_workers": 1,
            "process_frequency": 100,
            "process_timeout": 0,
            "process_max_iterations": 0,
        })
        self.__worker = None
        self.__workers_nbr = 1
        self.__workers_freq = 0
        self.__workers_iter = None
        self.__workers_timeout = None
        self.__main_pid = os.getpid()

    """ IConfigurable """

    def do_setup(self):
        ret = super().do_setup()
        self.__workers_nbr = int(self.get_conf("process_workers"))
        self.__workers_freq = int(self.get_conf("process_frequency"))
        self.__workers_timeout = int(self.get_conf("process_timeout"))
        self.__workers_iter = int(self.get_conf("process_max_iterations"))
        return ret

    """ Worker method """

    def work(self):
        self.read_channels_input()
        return self.do_work()

    def do_work(self):
        pass

    def on_worker_start(self, worker, *args):
        #little trick to have the service started for channel notification
        self._set_stopped(False)
        self.log_debug("Worker[{}]: started (pid={})"\
                .format(worker.get_number(), os.getpid()))

    def on_worker_error(self, worker, total_iter, error):
        self.log_debug("Worker[{}]: {}"\
                .format(worker.get_number(), error))

    def on_worker_stop(self, worker, total_iter):
        self.log_debug("Worker[{}]: stopped after {} iterations"\
                .format(worker.get_number(), total_iter))

    """ IService """

    def _start_impl(self):
        worker = SihdWorker(
            name="{}.Worker".format(self.get_name()),
            work=self.work,
            on_start=self.on_worker_start,
            on_stop=self.on_worker_stop,
            on_err=self.on_worker_error,
            daemon=True
        )
        #Check user input
        if worker.set_worker_number(self.__workers_nbr) is False:
            return False
        if worker.set_worker_frequency(self.__workers_freq) is False:
            return False
        if worker.set_worker_max_iterations(self.__workers_iter) is False:
            return False
        if worker.set_worker_timeout(self.__workers_timeout) is False:
            return False
        self.__worker = worker
        input_channels = self.get_channels_input()
        output_channels = self.get_channels_output()
        if worker.make_workers(args=(input_channels, output_channels,)):
            if self.is_paused():
                self.__worker.pause_workers()
            if worker.start_workers():
                """ After workers started their processes
                    Channels must be cleared of observers in the main process
                    Or this process will be notified of inputs
                """
                for channel in input_channels:
                    channel.clear_observers()
                return True
            else:
                self.log_error("Could not start workers")
        else:
            self.log_error("Could not make workers")
        return False

    def _stop_impl(self):
        if os.getpid() == self.__main_pid:
            self.log_debug("Stopping from main process")
            self.__worker.clear_workers()
            self.__worker = None
        else:
            self.log_debug("Stopping from child process")
            self.__worker.stop_workers()
        return True

    def _pause_impl(self):
        self.__worker.pause_workers()
        return True

    def _resume_impl(self):
        self.__worker.resume_workers()
        return True
