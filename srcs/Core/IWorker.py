#!/usr/bin/python
#coding: utf-8

""" System """
import time

from .ILoggable import ILoggable
from .IProducer import IProducer

multiprocessing = None

class Namespace:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class IWorker(ILoggable, IProducer):

    _states = Namespace(work=1, pause=2, stop=3)

    def __init__(self, name="IWorker"):
        super(IWorker, self).__init__(name)
        global multiprocessing
        try:
            if multiprocessing is None:
                import multiprocessing
            self.__worker_state = multiprocessing.Value('i', self._states.work)
        except ImportError:
            self._worker_state = None
        self.__proc = {}
        self.__n_workers = 2

    def set_max_worker(self):
        return self.set_worker_number(multiprocessing.cpu_count())

    def set_worker_number(self, num):
        if isinstance(num, int):
            if num > 0:
                self.__n_workers = num
                return True
        self.log_error("Worker number {} not positive int".format(num))
        return False

    def make_workers(self, n=None):
        if multiprocessing is None:
            self.log_error("Multiprocessing is not supported on your machine")
            return
        if n is None:
            n = self.__n_workers
        in_queue = self.get_producing_queue()
        state = self.__worker_state
        fun = self.__worker_loop
        for i in range(n):
            args = (i + 1, in_queue, out_queue, state,)
            out_queue = multiprocessing.Queue()
            proc = multiprocessing.Process(target=fun, daemon=True, args=args)
            self.__proc[proc] = out_queue

    def pause_workers(self):
        self.__worker_state.value = self._states.pause

    def resume_workers(self):
        self.__worker_state.value = self._states.work

    def stop_workers(self):
        self.__worker_state.value = self._states.stop
        dic = self.__proc
        for proc, queue in dic.items():
            if proc.is_alive():
                proc.terminate()
            proc.join(timeout=1.0)
            queue.close()
        self.__proc = {}

    def start_workers(self):
        dic = self.__proc
        if not dic:
            self.log_error("You have to make workers before you start")
            return
        for proc, queue in dic.items():
            proc.start()

    def do_work(self, data, queue, i):
        raise NotImplementedError("do_work not implemented")

    def __worker_loop(self, worker_number, in_queue, out_queue, state):
        stop_state = self._states.stop
        while True:
            if state.value == stop_state:
                return
            while state.value == pause_state:
                time.sleep(0.05)
            try:
                data = in_queue.get(timeout=1.0)
            except queue.Empty:
                continue
            ret = False
            try:
                ret = self.do_work(data, out_queue, worker_number)
            except Exception as e:
                self.log_error("Worker {} exception: {}".format(worker_number, e))
            if ret is not True:
                return
