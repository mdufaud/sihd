#!/usr/bin/python
#coding: utf-8

""" System """
import time

from .ILoggable import ILoggable

queue = None
multiprocessing = None

class Namespace:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class IWorker(ILoggable):

    _states = Namespace(work=1, pause=2, stop=3)

    def __init__(self, name="IWorker"):
        super(IWorker, self).__init__(name)
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        global queue
        if queue is None:
            import queue
        self.__proc = {}
        self.__n_workers = 2
        self.__worker_queue = multiprocessing.Queue()
        self.__worker_state = multiprocessing.Value('i', self._states.work)

    def set_max_worker(self):
        return self.set_worker_number(multiprocessing.cpu_count())

    def set_worker_number(self, num):
        if isinstance(num, int):
            if num > 0:
                self.__n_workers = num
                return True
        self.log_error("Worker number {} not positive int".format(num))
        return False

    def make_workers(self):
        in_queue = self.__worker_queue
        state = self.__worker_state
        fun = self.__worker_loop
        for i in range(self.__n_workers):
            out_queue = multiprocessing.Queue()
            proc = multiprocessing.Process(target=fun, daemon=True,
                                            args=(i + 1, in_queue, out_queue, state,))
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
        for proc, queue in dic.items():
            proc.start()

    def get_work(self):
        queue = self.__worker_queue
        if queue.empty():
            return None
        return queue.get()

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
