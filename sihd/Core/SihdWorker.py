#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

import sihd

from .Stats import PerfStat
from .ILoggable import ILoggable
from .IObserver import IObserver

multiprocessing = None
queue = None

class ProcessStates:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class SihdWorker(ILoggable, IObserver):

    _states = ProcessStates(work=1, pause=2, stop=3)

    def __init__(self, name="SihdWorker", work=None,
                    frequency=60, timeout=0, max_iter=0, 
                    args=(), daemon=True, worker_number=1,
                    on_start=None, on_stop=None, on_err=None):
        super(SihdWorker, self).__init__(name)
        """ Importing """
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        global queue
        if queue is None:
            import queue
        """ Life cycle controller """
        try:
            self.__worker_stop = multiprocessing.Event()
            self.__worker_work = multiprocessing.Event()
            self.__worker_work.set()
        except FileNotFoundError:
            self.__worker_stop = None
            self.__worker_work = None
        self.__proc_lst = []
        self.__n_workers = worker_number
        self.stats = PerfStat()
        """ Callable """
        self.set_work_method(work)
        self.set_work_callbacks(on_start, on_stop, on_err)
        """ Worker properties """
        self.__args = args
        self.__nproc = None
        self.__proc = None
        """ Work properties """
        if not self.set_worker_timeout(timeout):
            self.__timeout = None
        if not self.set_worker_frequency(frequency):
            self.__sleep = 0.0
        if not self.set_worker_max_iterations(max_iter):
            self.__max_iter = None

    """ Getters """

    def get_worker_number(self):
        return self.__n_workers

    def get_process(self):
        return self.__proc

    def get_number(self):
        return self.__nproc

    """ Setters """

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

    def set_work_callbacks(self, on_start, on_stop, on_err):
        if on_start and not callable(on_start):
            raise ValueError("Start method is not callable")
        if on_stop and not callable(on_stop):
            raise ValueError("Stop method is not callable")
        if on_err and not callable(on_err):
            raise ValueError("Error method is not callable")
        self.__on_start = on_start
        self.__on_stop = on_stop
        self.__on_err = on_err

    def set_work_method(self, method):
        if not callable(method):
            raise ValueError("Not a callable")
        self.on_work = method

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
        if freq <= 0.0:
            self.log_error("Worker frequency {} not a strict positive int".format(freq))
            return False
        self.__sleep = float(1. / float(freq))
        return True

    """ Life cycle """

    def make_workers(self, args=(), number=None):
        if self.get_process() is not None:
            self.log_error("You cannot make workers inside a children process")
            return False
        if multiprocessing is None:
            self.log_error("Multiprocessing is not supported on your system")
            return False
        if self.__worker_stop is None:
            self.log_error("Shared memory not found on your system")
            return False
        if number is None:
            number = self.__n_workers
        if args:
            self.__args = args
        else:
            args = self.__args
        stop = self.__worker_stop
        work = self.__worker_work
        fun = self.__worker_loop
        proc_lst = self.__proc_lst
        for i in range(number):
            worker_args = (i + 1, stop, work, *args,)
            proc = multiprocessing.Process(target=fun,
                    daemon=True, args=worker_args)
            proc_lst.append(proc)
        return True

    def are_workers_running(self):
        return (not self.__worker_stop.is_set())

    def are_workers_paused(self):
        return (not self.__worker_work.is_set())

    def pause_workers(self):
        self.__worker_work.clear()
        return True

    def resume_workers(self):
        self.__worker_work.set()
        return True

    def stop_workers(self):
        self.resume_workers()
        stop_evt = self.__worker_stop
        if stop_evt.is_set():
            return True
        stop_evt.set()
        return stop_evt.is_set()

    def clear_workers(self):
        if self.get_process() is not None:
            self.log_error("You cannot clear workers inside a children process")
            return False
        if self.stop_workers() is False:
            return False
        proc_lst = self.__proc_lst
        for i, proc in enumerate(proc_lst):
            s = "Worker[{}]: "
            if proc.is_alive():
                proc.terminate()
                s += "terminated"
            else:
                s += "had stopped"
            proc.join(timeout=2.0)
            self.log_debug(s.format(i + 1))
        self.__proc_lst = []
        return True

    def start_workers(self):
        if self.get_process() is not None:
            self.log_error("You cannot start workers inside a children process")
            return False
        ret = True
        if not self.__proc_lst:
            ret = self.make_workers(self.__args)
        if ret:
            proc_lst = self.__proc_lst
            for proc in proc_lst:
                proc.start()
        return ret

    """ Loop """

    def __worker_loop(self, n_proc, stop_evt, work_evt, *args):
        self.__nproc = n_proc
        self.__proc = multiprocessing.current_process()
        # Iterations
        i = 0
        max_iter = self.__max_iter
        # Time
        get_now = time.time
        sleep = time.sleep
        timeout = self.__timeout
        sleeptime = self.__sleep
        sleep = time.sleep
        start = get_now()
        steptime = 0.0
        uptime = 0.0
        downtime = 0.0
        maxtime = 0.0
        mintime = 100000000.0
        beg = 0.0
        end = 0.0
        # Work
        work = self.on_work
        if self.__on_start:
            self.__on_start(self, *args)
        while not stop_evt.is_set():
            while not work_evt.is_set():
                #Pause wait
                work_evt.wait(timeout=1)
                if stop_evt.is_set():
                    break 
            beg = get_now()
            # Check timeout
            if timeout is not None:
                if ((start + timeout) <= beg):
                    break
            # Execution
            ret = False
            try:
                ret = work()
            except Exception as e:
                if self.__on_err:
                    self.__on_err(self, i, e)
                else:
                    raise
            #Stats
            end = get_now()
            steptime = end - beg
            uptime += steptime
            if steptime > maxtime:
                maxtime = steptime
            if steptime < mintime:
                mintime = steptime
            pause = sleeptime - steptime
            i += 1
            if ret is False:
                break
            # Iterations
            if max_iter is not None and i >= max_iter:
                break
            # Pause
            if pause > 0.0:
                sleep(pause)
                downtime += pause
        self.__do_stat(uptime, downtime, maxtime, mintime, i)
        if self.__on_stop:
            self.__on_stop(self, i)

    def __do_stat(self, uptime, downtime, maxtime, mintime, i):
        self.stats = PerfStat(uptime, downtime,
                                maxtime, mintime, i)
