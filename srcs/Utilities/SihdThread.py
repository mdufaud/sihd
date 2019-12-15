#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import threading
import time

class SihdThread(threading.Thread):
 
    def __init__(self, parent):
        threading.Thread.__init__(self)
        self._stopped = True
        self._paused = False
        self._stepfun = None
        self._parent = parent
        self._sleep = parent.get_conf_val("thread_usleep") or 100

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
        self._stopped = False
        if self._stepfun is None:
            raise RuntimeError("No function to execute")
        while not self._stopped:
            if self._stepfun() == False:
                break
            while self._paused:
                time.usleep(self._sleep)

    def is_running(self):
        return self._stopped == False
