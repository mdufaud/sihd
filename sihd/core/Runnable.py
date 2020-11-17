#!/usr/bin/python
# coding: utf-8

""" System """
import time
import threading

from .ARunnable import ARunnable

class Runnable(ARunnable):

    __runnable_id = 0

    def __init__(self, name="Runnable", *args, **kwargs):
        super().__init__(name, *args, **kwargs)
        self.__id = self.__runnable_id
        self.__runnable_id += 1
        self.__stopped = False
        self.__paused = False
        self.__pause_cdt = threading.Condition(lock=None)

    #
    # ARunnable
    #

    def _setup_runnable(self, *args, **kwargs):
        self._set_runnable(self)

    def _is_paused(self):
        return self.__paused

    def _is_stopped(self):
        return self.__stopped

    def do_pause(self, t):
        cdt = self.__pause_cdt
        with cdt:
            cdt.wait(timeout=t)

    def get_id(self):
        return self.__id

    #
    # Service
    #

    def start(self):
        self.__stopped = False
        self.run()

    def stop(self):
        self.__stopped = True
        self._set_runnable(None)

    def pause(self):
        self.__paused = True

    def resume(self):
        cdt = self.__pause_cdt
        with cdt:
            cdt.notify(1)
        self.__paused = False
