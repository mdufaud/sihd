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

    """ Channels creation methods """

    def create_channel_list(self, name, **kwargs):
        if kwargs.get('size', None) is not None \
            and kwargs.get('var_type', None) is not None:
            return ChannelArray(name=name, **kwargs)
        return ChannelList(name=name, **kwargs)

    def create_channel_int(self, name, **kwargs):
        return ChannelValue(name=name, var_type='i', **kwargs)

    def create_channel_double(self, name, **kwargs):
        return ChannelValue(name=name, var_type='d', **kwargs)

    def create_channel_default(self, name, **kwargs):
        if kwargs.get('var_type', None):
            return ChannelValue(name=name, **kwargs)
        return super().create_channel_default(name, **kwargs)

    def create_channel_value(self, name, **kwargs):
        return ChannelValue(name=name, **kwargs)

    def create_channel_array(self, name, **kwargs):
        return ChannelArray(name=name, **kwargs)

    """ IConfigurable """

    def do_setup(self):
        ret = super().do_setup()
        self.__workers_nbr = int(self.get_conf("process_workers"))
        self.__workers_freq = int(self.get_conf("process_frequency"))
        self.__workers_timeout = int(self.get_conf("process_timeout"))
        self.__workers_iter = int(self.get_conf("process_max_iterations"))
        return ret

    """ Worker method """

    def work(self, i):
        self.read_channels_input()
        return self.do_work(i)

    def do_work(self, i):
        pass

    def on_worker_start(self, number):
        self.log_debug("Worker[{}]: started (pid={})".format(number, os.getpid()))

    def on_worker_stop(self, number, total):
        self.log_debug("Worker[{}]: stopped".format(number))

    """ IService """

    def _start_impl(self):
        worker = SihdWorker(self)
        worker.set_work_method(self.work)
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
        for _, channel in input_channels.items():
            channel.clear_observers()
        if worker.make_workers(input_channels, output_channels):
            if self.is_paused():
                self.__worker.pause_workers()
            if worker.start_workers():
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
