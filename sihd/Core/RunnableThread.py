#!/usr/bin/python
#coding: utf-8

""" System """
import time
import threading

from .ARunnable import ARunnable

class RunnableThread(ARunnable):

    def __init__(self, name="RunnableThread", *args, **kwargs):
        super().__init__(name, *args, **kwargs)
        self.__stopped = False
        self.__paused = False
        self.__pause_cdt = threading.Condition(lock=None)

    #
    # ARunnable
    #

    def _setup_runnable(self, *args, **kwargs):
        thread = threading.Thread(*args, **kwargs)
        thread.run = self.run
        self._set_runnable(thread)

    def _is_paused(self):
        return self.__paused

    def _is_stopped(self):
        return self.__stopped

    def do_pause(self, time):
        cdt = self.__pause_cdt
        with cdt:
            cdt.wait(timeout=time)

    def get_id(self):
        thread = self.get_runnable()
        if thread:
            return thread.ident
        return None

    #
    # Service
    #

    def start(self):
        thread = self.get_runnable()
        if not thread:
            raise RuntimeError("No thread to start")
        thread.start()

    def stop(self):
        thread = self.get_runnable()
        if not thread:
            raise RuntimeError("No thread to stop")
        self.resume()
        self.__stopped = True
        if self.is_main_thread():
            thread.join(1)
        self._set_runnable(None)

    def pause(self):
        thread = self.get_runnable()
        if not thread:
            raise RuntimeError("No thread to pause")
        self.__paused = True

    def resume(self):
        thread = self.get_runnable()
        if not thread:
            raise RuntimeError("No thread to resume")
        cdt = self.__pause_cdt
        with cdt:
            cdt.notify(1)
        self.__paused = False

    #
    # RunnableThread
    #

    @staticmethod
    def is_main_thread():
        return isinstance(threading.current_thread(),
                            threading._MainThread)
