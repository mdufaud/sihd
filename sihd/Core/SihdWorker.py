#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os
import traceback

from .ILoggable import ILoggable
from .IObserver import IObserver

multiprocessing = None
queue = None

class ProcessStates:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class SihdWorker(ILoggable, IObserver):

    _states = ProcessStates(work=1, pause=2, stop=3)

    def __init__(self, parent, frequency=50, timeout=0,
                    max_iter=0, name="SihdWorker"):
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
        if not self.set_worker_timeout(timeout):
            self.__timeout = None
        if not self.set_worker_frequency(frequency):
            self.__sleep = 0.0
        if not self.set_worker_max_iterations(max_iter):
            self.__max_iter = None
        self.__pause_time = 0.5

    """ Setup """

    def set_max_worker(self):
        return self.set_worker_number(multiprocessing.cpu_count())

    def set_worker_timeout(self, timeout):
        if timeout < 0:
            self.log_error("Worker timeout {} not positive int".format(timeout))
            return False
        if timeout == 0:
            self.__timeout = None
        else:
            self.__timeout = float(timeout)
        return True

    def set_worker_max_iterations(self, iterations):
        if iterations < 0:
            self.log_error("Worker iterations {} not positive int".format(iterations))
            return False
        if iterations == 0:
            self.__max_iter = None
        else:
            self.__max_iter = iterations
        return True

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

    def set_worker_frequency(self, freq):
        if freq <= 0:
            self.log_error("Worker frequency {} not a strict positive int".format(freq))
            return False
        self.__sleep = float(1. / int(freq))
        return True

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
        stop_evt = self.__worker_stop
        if stop_evt.is_set():
            return True
        stop_evt.set()
        return stop_evt.is_set()

    def clear_workers(self):
        if self.stop_workers() is False:
            return False
        proc_lst = self.__proc
        for i, proc in enumerate(proc_lst):
            s = "Worker[{}]: "
            if proc.is_alive():
                proc.terminate()
                s += "terminated"
            else:
                s += "had stopped"
            proc.join(timeout=1.0)
            self.log_debug(s.format(i + 1))
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

    def __worker_loop(self, worker_number, stop_evt, work_evt,
                        input_channels, output_channels):
        # Iterations
        self.__iter = 0
        max_iter = self.__max_iter
        # State
        number = 0
        # Time
        get_now = time.time
        sleep = time.sleep
        start = get_now()
        timeout = self.__timeout
        pause_time = self.__pause_time
        sleep_time = self.__sleep
        # Parent
        parent = self.__parent
        parent._set_stopped(False)
        work = self.do_work
        sleep = time.sleep
        parent.on_worker_start(worker_number)
        while not stop_evt.is_set():
            while not work_evt.is_set():
                work_evt.wait(timeout=pause_time)
            now = get_now()
            # Check timeout
            if timeout is not None:
                if ((start + timeout) <= now):
                    break
            # Execution
            ret = False
            try:
                ret = work(worker_number)
            except Exception as e:
                self.log_error("Worker {} of {} exception: {}".format(worker_number, parent, e))
                traceback.print_exc()
            if ret is False:
                break
            # Iterations
            self.__iter += 1
            if max_iter is not None and self.__iter >= max_iter:
                break
            # Pause
            end = get_now()
            pause = sleep_time - (end - now)
            if pause > 0.0:
                sleep(pause)
        parent.on_worker_stop(worker_number, number)
