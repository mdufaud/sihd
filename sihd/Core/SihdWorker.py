#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

from .ILoggable import ILoggable
from .IObserver import IObserver

multiprocessing = None
queue = None

class ProcessStates:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class SihdWorker(ILoggable, IObserver):

    _states = ProcessStates(work=1, pause=2, stop=3)

    def __init__(self, parent, name="SihdWorker"):
        super(SihdWorker, self).__init__(name)
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        global queue
        if queue is None:
            import queue
        try:
            self.__worker_stop = multiprocessing.Event()
            self.__worker_work = multiprocessing.Event()
            self.__worker_work.set()
        except FileNotFoundError:
            self.__worker_stop = None
            self.__worker_work = None
        self.__parent = parent
        self.__proc = []
        self.__n_workers = 2
        self.__timeout = 0.1
        self.__pause_time = 0.5

    """ Setup """

    def set_max_worker(self):
        return self.set_worker_number(multiprocessing.cpu_count())

    def queue_timeout(self, timeout):
        self.__timeout = float(timeout)

    def set_work_method(self, method):
        self.do_work = method

    def set_worker_number(self, num):
        if num == 'max':
            self.set_max_worker()
            return True
        if isinstance(num, int):
            if num > 0:
                self.__n_workers = num
                return True
        self.log_error("Worker number {} not positive int".format(num))
        return False

    def make_workers(self, input_channels, output_channels, number=None):
        if multiprocessing is None:
            self.log_error("Multiprocessing is not supported on your system")
            return False
        if self.__worker_stop is None:
            self.log_error("Shared memory not found on your system")
            return False
        if number is None:
            number = self.__n_workers
        stop = self.__worker_stop
        work = self.__worker_work
        fun = self.__worker_loop
        proc_lst = self.__proc
        for i in range(number):
            args = (i + 1, stop, work, input_channels, output_channels,)
            proc = multiprocessing.Process(target=fun, daemon=True, args=args)
            proc_lst.append(proc)
        return True

    """ States """

    def pause_workers(self):
        self.__worker_work.clear()
        return True

    def resume_workers(self):
        self.__worker_work.set()
        return True

    def stop_workers(self):
        self.__worker_stop.set()
        proc_lst = self.__proc
        i = 0
        for proc in proc_lst:
            s = "Worker[{}]: "
            if proc.is_alive():
                proc.terminate()
                s += "terminated"
            else:
                s += "had stopped"
            i += 1
            proc.join(timeout=1.0)
            self.log_debug(s.format(i))
        self.__proc = []
        return True

    def start_workers(self):
        proc_lst = self.__proc
        if not proc_lst:
            self.log_error("You have to make workers before you start")
            return False
        for proc in proc_lst:
            proc.start()
        return True

    """ Loop """

    def __worker_set_channels(self, input_channels, output_channels):
        for name, channel in input_channels.items():
            channel.add_observer(self.__parent)

    def __worker_loop(self, worker_number, stop_evt, work_evt,
                        input_channels, output_channels):
        # Channels
        self.__worker_set_channels(input_channels, output_channels)
        # State
        number = 0
        # Time
        pause_time = self.__pause_time
        # Parent
        parent = self.__parent
        parent._set_stopped(False)
        work = parent.do_work
        parent.on_worker_start(worker_number)
        while not stop_evt.is_set():
            while not work_evt.is_set():
                work_evt.wait(timeout=pause_time)
            # Execution
            try:
                ret = work(worker_number)
            except Exception as e:
                self.log_error("Worker {} exception: {}".format(worker_number, e))
            if ret is None or ret is False:
                break
        parent.on_worker_stop(worker_number, number)
