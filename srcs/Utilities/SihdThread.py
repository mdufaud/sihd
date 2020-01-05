#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import threading
import time

class SihdThread(threading.Thread):
 
    def __init__(self, parent=None, frequency=50, timeout=None, max_iter=None):
        threading.Thread.__init__(self)
        self._stopped = True
        self._paused = False
        self._stepfun = None
        self._iter = 0
        self._parent = parent
        if not self.set_max_iter(max_iter):
            self._max_iter = None
        if not self.set_timeout(timeout):
            self._timeout = None
        if not self.set_frequency(frequency):
            self._sleep = 0.05

    @staticmethod
    def is_main_thread():
        return isinstance(threading.current_thread(), threading._MainThread)

    @staticmethod
    def find_method_by_str(instance, fun_name):
        try:
            method = getattr(instance, fun_name)
            return method
        except AttributeError as e:
            raise NotImplementedError("Class `{}` does not implement `{}`"\
                    .format(instance.__class__.__name__, fun_name))
        return None

    def set_max_iter(self, max_iter):
        if not isinstance(max_iter, int) or max_iter <= 0:
            return False
        self._max_iter = max_iter
        return True

    def set_frequency(self, frequency):
        if not isinstance(frequency, int) or frequency <= 0:
            return False
        self._sleep = float(1. / int(frequency))
        return True

    def set_timeout(self, timeout):
        if not isinstance(timeout, (int, float)) or timeout <= 0.0:
            return False
        self._timeout = timeout
        return True

    def stop(self):
        self._stopped = True

    def pause(self):
        self._paused = True

    def resume(self):
        self._paused = False

    def get_stepfun(self):
        return self._stepfun

    def set_stepfun(self, fun):
        self._stepfun = fun

    def run(self):
        if self._stepfun is None:
            raise RuntimeError("No function to execute")
        self._stopped = False
        get_now = time.time
        sleep = time.sleep
        start = get_now()
        sleep_time = self._sleep
        timeout = self._timeout
        fun = self._stepfun
        max_iter = self._max_iter
        ret = None
        while not self._stopped:
            while self._paused:
                sleep(0.05)
            now = get_now()
            # Check timeout
            if timeout is not None:
                if ((start + timeout) <= now):
                    return
            # Execution
            try:
                ret = fun()
            except Exception as e:
                self.stop()
                raise
            if not ret:
                return
            # Iterations
            self._iter += 1
            if max_iter is not None and self._iter >= max_iter:
                return
            # Pause
            end = get_now()
            pause = sleep_time - (end - now)
            if pause > 0.0:
                sleep(pause)

    def is_running(self):
        return self._stopped == False
