#!/usr/bin/python
#coding: utf-8

""" System """
import time

from .ILoggable import ILoggable

multiprocessing = None
queue = None

class ProcessStates:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

class SihdWorker(ILoggable):

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
            self.__worker_state = multiprocessing.Value('i', self._states.work)
        except FileNotFoundError:
            self._worker_state = None
        self.__parent = parent
        self.__proc = {}
        self.__n_workers = 2
        self.__timeout = 0.1
        self.__pause_time = 0.05

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

    def make_workers(self, out_queue, producers=None, number=None):
        if multiprocessing is None:
            self.log_error("Multiprocessing is not supported on your system")
            return False
        if self.__worker_state is None:
            self.log_error("Shared memory not found on your system")
            return False
        if number is None:
            number = self.__n_workers
        state = self.__worker_state
        fun = self.__worker_loop
        for i in range(number):
            args = (i + 1, state, out_queue, producers,)
            proc = multiprocessing.Process(target=fun, daemon=True, args=args)
            self.__proc[proc] = out_queue
        return True

    """ States """

    def pause_workers(self):
        self.__worker_state.value = self._states.pause
        return True

    def resume_workers(self):
        self.__worker_state.value = self._states.work
        return True

    def stop_workers(self):
        self.__worker_state.value = self._states.stop
        dic = self.__proc
        i = 0
        for proc, queue in dic.items():
            s = "Worker[{}]: "
            if proc.is_alive():
                proc.terminate()
                s += "terminated"
            else:
                s += "had stopped"
            i += 1
            proc.join(timeout=1.0)
            self.log_debug(s.format(i))
            queue.close()
        self.__proc = {}
        return True

    def start_workers(self):
        dic = self.__proc
        if not dic:
            self.log_error("You have to make workers before you start")
            return False
        for proc, queue in dic.items():
            proc.start()
        return True

    """ Loop """

    def __worker_loop(self, worker_number, state, out_queue, producers):
        stop_state = self._states.stop
        pause_state = self._states.pause
        empty = queue.Empty
        parent = self.__parent
        work = parent.do_work
        timeout = self.__timeout
        pause_time = self.__pause_time
        parent.on_worker_start(worker_number, out_queue)
        number = 0
        while True:
            if state.value == stop_state:
                parent.on_worker_stop(worker_number, number)
                return
            while state.value == pause_state:
                time.sleep(pause_time)
            ret = False
            if producers:
                for producer, q in producers.items():
                    try:
                        data = q.get(timeout=timeout)
                    except empty:
                        continue
                    try:
                        ret = work(worker_number, out_queue, data, producer)
                    except Exception as e:
                        self.log_error("Worker {} exception: {}".format(worker_number, e))
                    if ret is None or ret is False:
                        parent.on_worker_stop(worker_number, number)
                        return
                    else:
                        number += 1
            else:
                #Yes i know, but making a function here might impact performance
                producer = None
                data = None
                try:
                    ret = work(worker_number, out_queue, data, producer)
                except Exception as e:
                    self.log_error("Worker {} exception: {}".format(worker_number, e))
                if ret is None or ret is False:
                    parent.on_worker_stop(worker_number, number)
                    return
                else:
                    number += 1
