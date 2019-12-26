#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import time

from .IService import IService
from .IConfigurable import IConfigurable
from .IRunnable import IRunnable

class ISequenceable(IService, IConfigurable, IRunnable):

    def __init__(self, name="ISequenceable"):
        super(ISequenceable, self).__init__(name)
        self._set_default_conf({
            "thread_frequency": 50, 
            "thread_timeout": 0,
            "thread_max_iterations": 0,
        })
        self.__frequency = None
        self.__timeout = None
        self.__max_iter = None

    """ IConfigurable """

    def _load_conf_impl(self):
        frequency = self.get_conf_val("thread_frequency")
        if frequency:
            self.__frequency = int(frequency)
        timeout = self.get_conf_val("thread_timeout")
        if timeout:
            self.__timeout = float(timeout)
        max_iter = self.get_conf_val("thread_max_iterations")
        if max_iter:
            self.__max_iter = int(max_iter)
        return True

    """ IService """

    def _start_impl(self):
        ret = True
        if self.is_configured == False:
            ret = self.load_conf()
        if ret is True:
            self.setup_thread(frequency=self.__frequency,
                                timeout=self.__timeout,
                                max_iter=self.__max_iter)
            self._start_time = time.time()
            self.start_thread()
        return ret

    def _stop_impl(self):
        self.stop_thread()
        return True

    def _pause_impl(self):
        self.pause_thread()
        return True

    def _resume_impl(self):
        self.resume_thread()
        return True
