#!/usr/bin/python
#coding: utf-8

""" System """


from .INamedObject import INamedObject
from .SihdThread import SihdThread

class IRunnable(INamedObject):

    def __init__(self, name="IRunnable"):
        super(IRunnable, self).__init__(name)
        self._thread = None

    def step_method(self):
        raise NotImplementedError("step_method is not implemented")

    def get_step_method(self):
        if self._thread:
            return self._thread.get_step_method()
        else:
            return self._step_method
        return None

    def set_step_method(self, method):
        if isinstance(method, str):
            self._step_method = SihdThread.find_method_by_str(self, method)
        else:
            self._step_method = method
        self.step_method = method
        if self._thread is not None:
            self._thread.set_step_method(method)

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

    def setup_thread(self, **kwargs):
        """ Method to call before anything else """
        if self._thread is not None:
            self.stop_thread()
        self._thread = SihdThread(self, **kwargs)
        self._thread.daemon = True
        self.set_step_method(self.step_method)
