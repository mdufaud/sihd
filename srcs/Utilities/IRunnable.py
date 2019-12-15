#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .INamedObject import INamedObject
from .SihdThread import SihdThread

class IRunnable(INamedObject):

    def __init__(self, name="IRunnable"):
        super(IRunnable, self).__init__(name)
        self._thread = None
        self._run_method = self._to_run

    def _to_run(self):
        raise NotImplementedError("_to_run is not implemented")

    def get_run_method(self):
        if self._thread:
            return self._thread.get_stepfun()
        else:
            return self._run_method
        return None

    def set_run_method(self, method):
        if isinstance(method, basestring):
            self._run_method = SihdThread.find_method_by_str(self, method)
        else:
            self._run_method = method
        
        if self._thread is not None:
            self._thread.set_stepfun(method)

    def get_thread(self):
        return self._thread

    def pause_thread(self):
        if self._thread is not None:
            self._thread.pause()

    def resume_thread(self):
        if self._thread is not None:
            self._thread.resume()

    def start_thread(self):
        if self._thread is not None:
            self._thread.start()

    def stop_thread(self):
        if self._thread is None:
            return
        self._thread.stop()
        if self._thread.is_alive() is False:
            self._thread = None
            return
        if SihdThread.is_main_thread():
            self._thread.join(0.5)
        self._thread = None

    def is_thread_alive(self):
        if self._thread is None:
            return False
        return self._thread.is_alive()

    def setup_thread(self):
        """ Method to call before anything else """
        if self._thread is not None:
            self.stop_thread()
        self._thread = SihdThread(self)
        self._thread.daemon = True
        self.set_run_method(self._run_method)
