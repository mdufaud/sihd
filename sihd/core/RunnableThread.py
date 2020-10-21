#!/usr/bin/python
# coding: utf-8

""" System """
import time
import threading

from .ARunnable import ARunnable

class RunnableThread(ARunnable):

    def __init__(self, name="RunnableThread", worker_number=1, *args, **kwargs):
        if isinstance(worker_number, int) and worker_number > 0:
            self.__nworkers = worker_number
        else:
            raise ValueError("Worker number: {} is either not an integer"
                                " or negative".format(worker_number))
        self.__thread = None
        self.__thread_lst = []
        self.__nthread = 0
        self.__stopped = False
        self.__paused = False
        self.__pause_cdt = threading.Condition(lock=None)
        super().__init__(name, *args, **kwargs)

    #
    # ARunnable
    #

    def _on_runnable_args(self, n_thread, *args):
        self.__thread = threading.current_thread()
        self.__nthread = n_thread
        self._set_runnable(self.__thread)

    def _setup_runnable(self, *targs, **tkwargs):
        args = self.get_args()
        thread_lst = self.__thread_lst
        if thread_lst:
            raise RuntimeError("Thread are made but not cleared")
        for i in range(self.__nworkers):
            worker_args = (i + 1, *args,)
            thread = threading.Thread(target=self.run,
                                        args=worker_args,
                                        *targs, **tkwargs)
            thread_lst.append(thread)

    def _is_paused(self):
        return self.__paused

    def _is_stopped(self):
        return self.__stopped

    def do_pause(self, t):
        cdt = self.__pause_cdt
        with cdt:
            cdt.wait(timeout=t)

    def get_id(self):
        thread = self.get_runnable()
        if thread:
            return thread.ident
        return None

    def get_worker_number(self):
        return self.__nworkers

    def get_number(self):
        return self.__nthread

    #
    # Service
    #

    def start(self):
        thread_lst = self.__thread_lst
        if not thread_lst:
            raise RuntimeError("No thread to start")
        for thread in thread_lst:
            thread.start()

    def stop(self):
        thread_lst = self.__thread_lst
        if not thread_lst:
            raise RuntimeError("No thread to stop")
        self.resume()
        self.__stopped = True
        if self.is_main_thread():
            for thread in thread_lst:
                thread.join(1)
        self._set_runnable(None)

    def pause(self):
        thread_lst = self.__thread_lst
        if not thread_lst:
            return False
        self.__paused = True

    def resume(self):
        thread_lst = self.__thread_lst
        if not thread_lst:
            return False
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
