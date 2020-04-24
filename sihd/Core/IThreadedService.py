#!/usr/bin/python
#coding: utf-8

""" System """
import time

from .IService import IService
from .IConfigurable import IConfigurable
from .IRunnable import IRunnable

class IThreadedService(IService, IRunnable):

    def __init__(self, name="IThreadedService"):
        super(IThreadedService, self).__init__(name)
        self._set_default_conf({
            "thread_frequency": 50, 
            "thread_timeout": 0,
            "thread_max_iterations": 0,
        })
        self.__thread_freq = None
        self.__thread_timeout = None
        self.__thread_max_iter = None

    def on_step(self):
        pass

    def step(self):
        """ Not necessarily useful since we have observer/observable
            But in some cases when service is stopped you may want
                to check your inputs
        """
        self.read_channels_input()
        return self.on_step()

    """ IConfigurable """

    def on_setup(self):
        ret = super().on_setup()
        self.__thread_freq = int(self.get_conf("thread_frequency"))
        self.__thread_timeout = float(self.get_conf("thread_timeout"))
        self.__thread_max_iter = int(self.get_conf("thread_max_iterations"))
        return ret

    """ IService """

    def _start_impl(self):
        self.setup_thread(
            name="{}.Thread".format(self.get_name()),
            frequency=self.__thread_freq,
            timeout=self.__thread_timeout,
            max_iter=self.__thread_max_iter)
        if self.is_paused():
            self.pause_thread()
        return True

    def on_start(self):
        self.start_thread()

    def _stop_impl(self):
        self.stop_thread()
        return True

    def _pause_impl(self):
        self.pause_thread()
        return True

    def _resume_impl(self):
        self.resume_thread()
        return True
