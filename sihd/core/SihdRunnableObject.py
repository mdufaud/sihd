#!/usr/bin/python
# coding: utf-8

""" System """
import time

import sihd
from .IService import IService
from .SihdObject import SihdObject
from .RunnableThread import RunnableThread
from .RunnableProcess import RunnableProcess
from .Channel import Channel

class SihdRunnableObject(SihdObject):

    def __init__(self, name="SihdRunnableObject", **kwargs):
        super().__init__(name, **kwargs)
        self.configuration.add_defaults({
            "runnable_frequency": 30,
            "runnable_type": "thread",
        })
        self.__runnable = None
        self.__run_freq = None
        self.__is_thread = False
        self.__is_process = False
        self.__is_default = False
        self.__run_timeout = 0
        self.__run_steps = 0
        self.__run_proc = 1

    #TODO change name
    def is_service_multiprocessing(self):
        return self.__is_process

    def is_service_threading(self):
        return self.__is_thread

    def is_service_default(self):
        return self.__is_default

    def get_runnable_type(self):
        if self.__is_process:
            return "process"
        elif self.__is_thread:
            return "thread"
        elif self.__is_default:
            return "main"
        return None


    #
    # NamedObject description
    #

    def _get_attributes(self):
        lst = super()._get_attributes()
        type = self.get_runnable_type()
        if type:
            lst.append("runnable=" + type)
        return lst

    #
    # Iterations
    #

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

    def on_step(self):
        pass

    #
    # Configuration
    #

    def on_setup(self, conf):
        ret = super().on_setup(conf)
        timeout = conf.get("runnable_timeout", dynamic=True)
        if timeout:
            self.__run_timeout = float(timeout)
        steps = conf.get("runnable_steps", dynamic=True)
        if steps:
            self.__run_steps = int(steps)
        procs = conf.get("runnable_workers", dynamic=True)
        if procs:
            self.__run_proc = int(procs)
        self.__run_freq = float(conf.get("runnable_frequency"))
        type = conf.get("runnable_type")
        if type == 'thread':
            self.__is_thread = True
        elif type == 'process':
            self.__is_process = True
        elif type == 'default':
            self.__is_default = True
        elif type != 'none':
            self.log_error("No such runnable type " + str(type))
            ret = False
        return ret

    #
    # Runnable
    #

    def __start_children(self, runnable):
        """ Start child services here in thread/process """
        ret = self.call_children('start', cls=IService,
                                 pass_children=[runnable])
        if ret:
            self.on_runnable_start(runnable)
        else:
            self.stop()

    def on_runnable_start(self, runnable):
        self.log_info("runnable {} started".format(self.get_runnable_type()))

    def on_runnable_stop(self, runnable, iteration):
        self.log_debug("runnable stopped after {} iterations".format(iteration))

    def on_runnable_error(self, runnable, iteration, error):
        self.log_error("runnable error: {}".format(error))
        self.log_error(sihd.sys.get_traceback())

    def is_runnable_active(self):
        r = self.__runnable
        return r and r.is_active() or False

    #
    # Runnable creation
    #

    def __make_runnable(self):
        runnable = None
        kwargs = {
            'name': self.get_name(),
            'step': self.step,
            'on_start': self.on_runnable_start,
            'on_stop': self.on_runnable_stop,
            'on_err': self.on_runnable_error,
            'frequency': self.__run_freq,
            'timeout': self.__run_timeout,
            'max_iter': self.__run_steps,
            'worker_number': self.__run_proc,
            'parent': self,
            'daemon': True,
        }
        if self.__is_thread:
            runnable = RunnableThread(**kwargs)
            self.log_debug("service is a threaded runnable")
        elif self.__is_process:
            kwargs['on_start'] = self.__start_children
            runnable = RunnableProcess(**kwargs)
            self.log_debug("service is a processed runnable")
        elif self.__is_default:
            runnable = Runnable(**kwargs)
            self.log_debug("service is a main thread runnable")
        else:
            return
        self.__runnable = runnable

    #
    # SihdObject
    #

    def is_runnable_active(self):
        r = self.__runnable
        return r and r.is_active() or False

    def is_runnable_paused(self):
        r = self.__runnable
        return r and r.is_paused() or False

    def is_runnable_running(self):
        r = self.__runnable
        return r and r.is_running() or False

    # override
    def create_channel(self, name, **kwargs):
        if self.is_service_multiprocessing():
            kwargs['mp'] = True
        return super().create_channel(name, **kwargs)

    # override
    def on_link(self, name, new_channel):
        if isinstance(new_channel, Channel) \
                and self.is_service_multiprocessing() \
                and new_channel.is_multiprocess() is False:
            raise ValueError("Trying to link a non multiprocessed channel to"
                             " a processed service ({})".format(new_channel))
        return super().on_link(name, new_channel)

    def _init_impl(self):
        """ Create runnable """
        self.__make_runnable()
        return super()._init_impl()

    def on_start(self):
        """ Done after start because you want service to be 'running' """
        r = self.__runnable
        if r:
            r.start()

    def _start_impl(self):
        """ If runnable then start it in on_start """
        r = self.__runnable
        if r:
            return True
        return super()._start_impl()
