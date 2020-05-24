#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

from .ARunnable import ARunnable
import sihd
from .Stats import PerfStat

multiprocessing = None

class RunnableProcess(ARunnable):

    def __init__(self, name="RunnableProcess",
                    worker_number=1, *args, **kwargs):
        """ Worker properties """
        self.__nproc = None
        self.__proc = None
        self.__proc_status = []
        self.__parent_pid = os.getpid()
        """ Importing """
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        """ Life cycle controller """
        try:
            self.__worker_stop = multiprocessing.Event()
            self.__worker_stop.clear()
            self.__worker_work = multiprocessing.Event()
            self.__worker_work.set()
        except FileNotFoundError:
            self.__worker_stop = None
            self.__worker_work = None
        self.__proc_lst = []
        if isinstance(worker_number, int) and worker_number > 0:
            self.__n_workers = worker_number
        else:
            raise ValueError("Worker number: {} is either not an integer"
                                " or negative".format(worker_number))
        super().__init__(name, *args, **kwargs)

    #
    # ARunnable
    #

    def _on_runnable_args(self, n_proc, stop, work, *args):
        self.__proc = multiprocessing.current_process()
        self.__nproc = n_proc
        self._set_runnable(self.__proc)

    def do_pause(self, t):
        self.__worker_work.wait(timeout=t)

    def _setup_runnable(self, *pargs, **pkwargs):
        if self.get_process() is not None:
            raise RuntimeError("You cannot make workers inside a children process")
        if multiprocessing is None:
            raise RuntimeError("Multiprocessing is not supported on your system")
        if self.__worker_stop is None:
            raise RuntimeError("Shared memory not found on your system")
        if self.__proc_lst:
            raise RuntimeError("Process are made but not cleared")
        args = self.get_args()
        stop = self.__worker_stop
        work = self.__worker_work
        self.__proc_status = []
        self.__proc_lst = []
        proc_lst = self.__proc_lst
        for i in range(self.__n_workers):
            worker_args = (i + 1, stop, work, *args,)
            proc = multiprocessing.Process(target=self.run,
                                            args=worker_args,
                                            *pargs, **pkwargs)
            proc_lst.append(proc)
        #self._set_runnable(proc_lst)

    def get_id(self):
        proc = self.get_runnable()
        if proc:
            return proc.pid
        return None

    def _is_paused(self):
        return (not self.__worker_work.is_set())

    def _is_stopped(self):
        return self.__worker_stop.is_set()

    #
    # Service
    #

    def start(self):
        if self.get_process() is not None:
            raise RuntimeError("You cannot start workers inside a children process")
        ret = True
        if not self.__proc_lst:
            raise RuntimeError("No process to start")
        for proc in self.__proc_lst:
            proc.start()

    def stop(self):
        stop_evt = self.__worker_stop
        if stop_evt.is_set():
            self.resume()
            return True
        stop_evt.set()
        self.resume()
        if self.get_process() is None and self.__parent_pid == os.getpid():
            self.__clear_workers()

    def pause(self):
        self.__worker_work.clear()

    def resume(self):
        self.__worker_work.set()

    #
    # RunnableProcess
    #

    def __clear_workers(self):
        #Careful ! Should only be called on stop
        pstatus = self.__proc_status
        for i, proc in enumerate(self.__proc_lst):
            s = "Worker[{}]: ".format(i + 1)
            tries = 3
            while tries > 0:
                alive = proc.is_alive()
                if not alive:
                    break
                time.sleep(0.05)
                tries -= 1
            if alive:
                proc.terminate()
                s += "has been terminated (timeout)"
            else:
                s += "has stopped"
            pstatus.append(s)
            proc.join(timeout=1.0)
        self._set_runnable(None)
        self.__proc_lst = []

    def get_worker_status(self, n=-1):
        pstatus = self.__proc_status
        if n >= 0 and n < len(pstatus):
            return pstatus[n]
        return pstatus

    def get_worker_number(self):
        return self.__n_workers

    def get_process(self):
        return self.__proc

    def get_number(self):
        return self.__nproc
