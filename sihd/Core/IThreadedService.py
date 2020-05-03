#!/usr/bin/python
#coding: utf-8

""" System """
import time

import sihd

from .SihdService import SihdService
from .IConfigurable import IConfigurable
from .IRunnable import IRunnable

class IThreadedService(SihdService, IRunnable):

    def __init__(self, name="IThreadedService", **kwargs):
        super(IThreadedService, self).__init__(name, **kwargs)
        self._set_default_conf({
            "thread_frequency": 50, 
            "thread_timeout": 0,
            "thread_max_iterations": 0,
        })
        self.__thread_freq = None
        self.__thread_timeout = None
        self.__thread_max_iter = None
        self.__thread_args = []

    """ IRunnable """

    def on_thread_start(self, thread, *args):
        self.log_debug("Thread started")

    def on_thread_stop(self, thread, iteration):
        self.log_debug("Thread stopped after {} iterations"\
                .format(iteration))

    def on_thread_error(self, thread, iteration, error):
        self.log_error("Thread error: {}".format(error))
        self.log_error(sihd.get_traceback())

    def step(self):
        """
            Not necessarily useful since we have observer/observable
            But in some cases when service is stopped you may want
                to check your inputs
        """
        self.read_channels_input()
        ret = self.is_running()
        if ret:
            ret = self.on_step()
        return ret

    """ IThreadedService """

    def on_step(self):
        pass

    def set_thread_args(self, args):
        self.__thread_args = args

    """ IConfigurable """

    def on_setup(self):
        ret = super().on_setup()
        self.__thread_freq = float(self.get_conf("thread_frequency"))
        self.__thread_timeout = float(self.get_conf("thread_timeout"))
        self.__thread_max_iter = int(self.get_conf("thread_max_iterations"))
        return ret

    """ SihdService """

    def _start_impl(self):
        self.setup_thread(
            name="{}.MainThread".format(self.get_name()),
            frequency=self.__thread_freq,
            timeout=self.__thread_timeout,
            max_iter=self.__thread_max_iter,
            args=self.__thread_args)
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
