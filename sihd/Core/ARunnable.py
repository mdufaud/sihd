#!/usr/bin/python
# coding: utf-8

#
# System
#
import time

from .ANamedObject import ANamedObject
from .IService import IService
from .Stats import PerfStat

class ARunnable(ANamedObject, IService):

    def __init__(self, name="ARunnable", step=None,
                    frequency=50, timeout=None, max_iter=None,
                    on_start=None, on_stop=None, on_err=None,
                    do_sleep=None, args=(),
                    *otherargs, **otherkwargs):
        parent = otherkwargs.pop('parent', None)
        super().__init__(name=name, parent=parent)
        self.__stats = PerfStat()
        self.__runnable = None
        self.__do_sleep = do_sleep
        self.__step_method = self.step
        # Callables
        self.set_callbacks(on_start, on_stop, on_err)
        if step:
            self.set_step_method(step)
        # Step properties
        self.__max_iter = None
        self.__timeout = None
        self.__sleep = 0.05
        if max_iter:
            self.set_max_iter(max_iter)
        if timeout:
            self.set_timeout(timeout)
        if frequency:
            self.set_frequency(frequency)
        self.set_args(args)
        otherkwargs['name'] = self.get_name()
        self._setup_runnable(*otherargs, **otherkwargs)

    #
    # Configurations
    #

    def set_max_iter(self, max_iter):
        """ Setting max step before exiting """
        if not isinstance(max_iter, int) or max_iter <= 0:
            raise ValueError("Maximum iteration: {} is not an int "
                                "or negative".format(max_iter))
        self.__max_iter = max_iter

    def set_frequency(self, frequency):
        """ Setting frequency in Hz for step execution """
        if frequency <= 0.0:
            raise ValueError("Frequency: {} is negative".format(frequency))
        self.__sleep = float(1. / float(frequency))

    def set_timeout(self, timeout):
        """ Set a timeout for exiting after certain time """
        if not isinstance(timeout, (int, float)) or timeout <= 0.0:
            raise ValueError("Timeout: {} is NaN "
                                "or negative".format(timeout))
        self.__timeout = timeout

    def set_callbacks(self, on_start, on_stop, on_err):
        if on_start and not callable(on_start):
            raise ValueError("Start method is not callable")
        if on_stop and not callable(on_stop):
            raise ValueError("Stop method is not callable")
        if on_err and not callable(on_err):
            raise ValueError("Error method is not callable")
        self.__on_start = on_start
        self.__on_stop = on_stop
        self.__on_error = on_err

    #
    # Callbacks
    #

    def on_runnable_start(self, runnable):
        pass

    def on_runnable_stop(self, runnable, itr):
        pass

    def on_runnable_error(self, runnable, itr, err):
        pass

    #
    # Getters/Setters
    #

    def set_args(self, args):
        self.__args = args

    def get_args(self):
        return self.__args

    def _set_runnable(self, runnable):
        self.__runnable = runnable

    def get_runnable(self):
        return self.__runnable

    def get_stats(self):
        return self.__stats

    def _setup_runnable(self, *args, **kwargs):
        raise NotImplementedError("setup_runnable")

    def get_id(self):
        raise NotImplementedError("get_id")


    #
    # Step
    #

    def set_step_method(self, method):
        if not callable(method):
            raise ValueError("Step method is not callable")
        self.__step_method = method

    def get_step_method(self):
        return self.__step_method

    def _is_paused(self):
        raise NotImplementedError("_is_paused")

    def _is_stopped(self):
        raise NotImplementedError("_is_stopped")

    def is_active(self):
        return not self._is_paused() and not self._is_stopped()

    def is_running(self):
        return not self._is_stopped()

    def is_stopped(self):
        return self._is_stopped()

    def is_paused(self):
        return self._is_paused()

    def step(self):
        raise NotImplementedError("step")

    def do_pause(self, t):
        time.sleep(t)

    def _on_runnable_args(self, *args):
        #Process args passed on to runnable 
        pass

    def run(self, *args):
        self._on_runnable_args(*args)
        #time
        get_now = time.time
        do_sleep = self.__do_sleep or time.sleep
        start = get_now()
        sleeptime = self.__sleep
        timeout = self.__timeout
        steptime = 0.0
        uptime = 0.0
        downtime = 0.0
        maxtime = 0.0
        mintime = 100000.0
        beg = 0.0
        end = 0.0
        #step
        step = self.get_step_method()
        i = 0
        max_iter = self.__max_iter
        ret = None
        do_pause = self.do_pause
        check_stopped = self._is_stopped
        check_paused = self._is_paused
        if self.__on_start:
            self.__on_start(self)
        try:
            while not check_stopped():
                while check_paused():
                    #Pause wait
                    do_pause(1)
                    if check_stopped():
                        break
                beg = get_now()
                # Check timeout
                if timeout is not None and (start + timeout) <= beg:
                    break
                # Execution
                try:
                    ret = step()
                except Exception as e:
                    print(e)
                    self.stop()
                    if self.__on_error:
                        self.__on_error(self, i, e)
                        break
                    else:
                        raise
                #Stats
                end = get_now()
                steptime = end - beg
                uptime += steptime
                if steptime > maxtime:
                    maxtime = steptime
                if steptime < mintime:
                    mintime = steptime
                i += 1
                if ret is False:
                    break
                # Iterations
                if max_iter is not None and i >= max_iter:
                    break
                # Pause
                pause = sleeptime - steptime
                if pause > 0.0:
                    do_sleep(pause)
                    downtime += pause
        except KeyboardInterrupt:
            pass
        self.__do_stat(uptime, downtime, maxtime, mintime, i)
        if self.__on_stop:
            self.__on_stop(self, i)

    def __do_stat(self, uptime, downtime, maxtime, mintime, i):
        self.__stats = PerfStat(uptime, downtime,
                                maxtime, mintime, i)
