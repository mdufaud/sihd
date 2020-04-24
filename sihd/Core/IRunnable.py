#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject
from .SihdThread import SihdThread

class IRunnable(INamedObject):

    def __init__(self, name="IRunnable"):
        super(IRunnable, self).__init__(name)
        self.__thread = None
        self.__step_method = self.step

    def step(self):
        raise NotImplementedError("step method is not implemented")

    def on_thread_start(self, thread, *args):
        pass

    def on_thread_stop(self, thread, iteration):
        pass

    def on_thread_error(self, thread, iteration, error):
        raise error

    def get_step_method(self):
        if self.__thread:
            return self.__thread.get_step_method()
        else:
            return self.__step_method
        return None

    def set_step_method(self, method):
        if isinstance(method, str):
            self.__step_method = SihdThread.find_method_by_str(self, method)
        else:
            self.__step_method = method
        self.step = method
        if self.__thread is not None:
            self.__thread.set_step_method(method)

    def get_thread(self):
        return self.__thread

    def pause_thread(self):
        if self.__thread is not None:
            self.__thread.pause()

    def resume_thread(self):
        if self.__thread is not None:
            self.__thread.resume()

    def start_thread(self):
        if self.__thread is not None:
            self.__thread.start()

    def stop_thread(self):
        if self.__thread is None:
            return
        self.__thread.stop()
        if self.__thread.is_alive() is False:
            self.__thread = None
            return
        if SihdThread.is_main_thread():
            self.__thread.join(0.5)
        self.__thread = None

    def is_thread_alive(self):
        if self.__thread is None:
            return False
        return self.__thread.is_alive()

    def setup_thread(self, *args, **kwargs):
        """ Method to call before anything else """
        if self.__thread is not None:
            self.stop_thread()
        step = kwargs.pop('step', None) or self.step
        on_start = kwargs.pop('on_start', None) or self.on_thread_start
        on_err = kwargs.pop('on_err', None) or self.on_thread_error
        on_stop = kwargs.pop('on_stop', None) or self.on_thread_stop
        self.__thread = SihdThread(
            step=step,
            on_start=on_start,
            on_stop=on_stop,
            on_err=on_err,
            *args, **kwargs)
