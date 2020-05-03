#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

import sihd

from .SihdService import SihdService
from .IConfigurable import IConfigurable
from .SihdWorker import SihdWorker

class IProcessedService(SihdService):

    def __init__(self, name="IProcessedService", **kwargs):
        super(IProcessedService, self).__init__(name, **kwargs)
        self._set_default_conf({
            "process_workers": 1,
            "process_frequency": 50,
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

    def on_setup(self):
        ret = super().on_setup()
        self.__workers_nbr = int(self.get_conf("process_workers"))
        self.__workers_freq = float(self.get_conf("process_frequency"))
        self.__workers_timeout = float(self.get_conf("process_timeout"))
        self.__workers_iter = int(self.get_conf("process_max_iterations"))
        return ret

    """ Worker method """

    def is_paused(self):
        worker = self.__worker
        if worker:
            return worker.are_workers_paused()
        return super().is_paused()

    def is_running(self):
        worker = self.__worker
        if worker:
            return worker.are_workers_running()
        return super().is_running()

    def work(self):
        self.read_channels_input()
        ret = self.is_running()
        if ret:
            ret = self.on_work()
        return ret

    def on_work(self):
        pass

    def on_worker_start(self, worker, *args):
        self.log_debug("Worker[{}]: started (pid={})"\
                .format(worker.get_number(), os.getpid()))

    def on_worker_error(self, worker, total_iter, error):
        self.log_error("Worker[{}] exception: {}"\
                .format(worker.get_number(), error))
        self.log_error(sihd.get_traceback())

    def on_worker_stop(self, worker, total_iter):
        self.log_debug("Worker[{}]: stopped after {} iterations"\
                .format(worker.get_number(), total_iter))

    """ SihdService """

    def create_channel(self, name, **kwargs):
        kwargs['mp'] = True
        return super().create_channel(name, **kwargs)

    def link_channel(self, name, new_channel):
        if new_channel.is_multiprocess() is False:
            self.log_warning("Trying to link a non multiprocessed channel to"
                    " a processed service ({})".format(new_channel))
        return super().link_channel(name, new_channel)

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
            return True
        else:
            self.log_error("Could not make workers")
        return False

    def on_start(self):
        worker = self.__worker
        if worker.start_workers():
            """ After workers started their processes
                Channels must be cleared of observers in the main process
                Or this process will be notified of inputs
            """
            input_channels = self.get_channels_input()
            for channel in input_channels:
                channel.clear_observers()
            return True
        else:
            self.log_error("Could not start workers")

    def _stop_impl(self):
        ret = False
        if os.getpid() == self.__main_pid:
            self.log_debug("Stopping from main process")
            ret = self.__worker.clear_workers()
            if ret:
                self.__worker = None
        else:
            self.log_debug("Stopping from child process")
            ret = self.__worker.stop_workers()
        return ret

    def _pause_impl(self):
        return self.__worker.pause_workers()

    def _resume_impl(self):
        return self.__worker.resume_workers()
