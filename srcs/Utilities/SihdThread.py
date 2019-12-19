#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import threading
import time

class SihdThread(threading.Thread):
 
    def __init__(self, parent=None, frequency=50, timeout=None):
        threading.Thread.__init__(self)
        self._stopped = True
        self._paused = False
        self._stepfun = None
        self._parent = parent
        self.set_timeout(timeout)
        self.set_frequency(frequency)

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

    def set_frequency(self, frequency):
        if frequency is 0 or frequency is None:
            return
        self._sleep = float(1. / int(frequency))

    def set_timeout(self, timeout):
        self._timeout = timeout

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
        sleep = self._sleep
        start = time.time()
        timeout = self._timeout
        fun = self._stepfun
        res = None
        while not self._stopped:
            if timeout is not None:
                now = time.time()
                if ((start + timeout) <= now):
                    return
            try:
                res = fun()
            except Exception as e:
                self.stop()
                raise
            if res is None or res is False:
                return
            time.sleep(sleep)
            while self._paused:
                time.sleep(0.05)

    def is_running(self):
        return self._stopped == False
