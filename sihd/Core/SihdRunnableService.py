#!/usr/bin/python
#coding: utf-8

""" System """
import time

import sihd
from .SihdService import SihdService
from .RunnableThread import RunnableThread
from .RunnableProcess import RunnableProcess
from .Channel import Channel

class SihdRunnableService(SihdService):

    def __init__(self, name="SihdRunnableService", **kwargs):
        super().__init__(name, **kwargs)
        self.set_default_conf({
            "runnable_frequency": 50, 
            "runnable_timeout": 0,
            "runnable_steps": 0,
            "runnable_type": "thread",
            "runnable_processes": 1,
        })
        self.__runnable = None
        self.__run_freq = None
        self.__run_timeout = None
        self.__run_steps = None
        self.__is_thread = False
        self.__is_process = False
        self.__run_proc = None

    def is_service_multiprocessing(self):
        return self.__is_process

    def is_service_threading(self):
        return self.__is_thread

    def is_service_default(self):
        return not self.is_service_threading() and not self.is_service_multiprocessing()

    """ ANamedObject """

    def _get_attributes(self):
        lst = super()._get_attributes()
        lst.append("runnable=" + str(self.__run_proc))
        return lst

    """ SihdService """

    def create_channel(self, name, **kwargs):
        if self.is_service_multiprocessing():
            kwargs['mp'] = True
        return super().create_channel(name, **kwargs)

    def on_link(self, name, new_channel):
        if isinstance(new_channel, Channel) and self.is_service_multiprocessing()\
            and new_channel.is_multiprocess() is False:
            raise ValueError("Trying to link a non multiprocessed channel to"
                                " a processed service ({})".format(new_channel))
        return super().on_link(name, new_channel)

    """ Runnable """

    def on_runnable_start(self, runnable):
        self.log_debug("{} started".format(runnable.get_name()))

    def on_runnable_stop(self, runnable, iteration):
        self.log_debug("{} stopped after {} iterations"\
                        .format(runnable.get_name(), iteration))

    def on_runnable_error(self, runnable, iteration, error):
        self.log_error("{} error: {}".format(runnable.get_name(), error))
        self.log_error(sihd.get_traceback())

    def __make_runnable(self):
        runnable = None
        kwargs = {
            'step': self.step,
            'on_start': self.on_runnable_start,
            'on_stop': self.on_runnable_stop,
            'on_err': self.on_runnable_error,
            'frequency': self.__run_freq,
            'timeout': self.__run_timeout,
            'max_iter': self.__run_steps,
            'parent': self,
            'daemon': True,
        }
        name = self.get_name()
        if self.__is_thread:
            runnable = RunnableThread(**kwargs)
            self.log_debug("Service is a threaded runnable")
        elif self.__is_process:
            kwargs['worker_number'] = self.__run_proc
            runnable = RunnableProcess(**kwargs)
            self.log_debug("Service is a processed runnable")
        else:
            return
        self.__runnable = runnable

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

    """ AConfigurable """

    def on_setup(self):
        ret = super().on_setup()
        self.__run_freq = float(self.get_conf("runnable_frequency"))
        self.__run_timeout = float(self.get_conf("runnable_timeout"))
        self.__run_steps = int(self.get_conf("runnable_steps"))
        self.__run_proc = int(self.get_conf("runnable_processes"))
        type = self.get_conf("runnable_type")
        if type == 'thread':
            self.__is_thread = True
        elif type == 'process':
            self.__is_process = True
        return ret

    """ SihdService """

    def _init_impl(self):
        self.__make_runnable()
        return True

    def on_start(self):
        """ Done after start because you want service to be 'running' """
        r = self.__runnable
        if r:
            r.start()

    def _stop_impl(self):
        ret = super()._stop_impl()
        r = self.__runnable
        if r:
            try:
                r.stop()
            except RuntimeError as e:
                self.log_error(e)
                ret = False
        return ret

    def _pause_impl(self):
        ret = super()._pause_impl()
        r = self.__runnable
        if r:
            try:
                r.pause()
            except RuntimeError as e:
                self.log_error(e)
                ret = False
        return ret

    def _resume_impl(self):
        ret = super()._resume_impl()
        r = self.__runnable
        if r:
            try:
                r.resume()
            except RuntimeError as e:
                self.log_error(e)
                ret = False
        return ret
