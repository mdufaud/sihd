#!/usr/bin/python
# coding: utf-8

import time
import threading
from .ANamedObject import ANamedObject
from .IService import IService

class Calendar(object):

    def __init__(self):
        self.reset()

    def _decal(self, sec):
        self.time += sec
        return self

    def reset(self):
        self.time = 0
        return self

    def at(self, t):
        self.time = t
        return self

    def day(self, day):
        return self.hour(day * 24)

    def hour(self, hour):
        return self.minute(hour * 60)

    def minute(self, minute):
        return self._decal(minute * 60)

    def second(self, sec):
        return self._decal(sec)

    def ms(self, ms):
        return self._decal(ms / 1E3)

class Model(object):

    def __init__(self, step, sleeptime, args=(), kwargs={}):
        self.step = step
        self.sleeptime = sleeptime
        self.args = args
        self.kwargs = kwargs
        self.attime = 0
        self.min = 1E9
        self.max = 0
        self.uptime = 0
        self.n = 0

    def add_stat(self, exe_time):
        if exe_time < self.min:
            self.min = exe_time
        if exe_time > self.max:
            self.max = exe_time
        self.uptime += exe_time
        self.n += 1

class Scheduler(ANamedObject, IService):

    def __init__(self, name="Scheduler", get_time=time.time, **kwargs):
        # Objects
        self.sched = Calendar()
        self.daemon = None
        # Settings
        self.timer = False
        self.thread = False
        self.timeout = None
        self.maxsteps = 0
        self.pausetime = 1E9
        # Internal
        self.get_time = get_time
        self._scheduled = []
        self.__event = threading.Event()
        self.__stop = False
        super().__init__(name, **kwargs)

    #
    # Service
    #

    def start(self):
        threaded = self.thread == True
        if self.sched.time > 0:
            threaded = True
            self.timer = True
        if threaded:
            self.__run_thread()
        else:
            self.__run()

    def stop(self):
        if self.__stop == True:
            return
        self.__stop = True
        self.__event.set()
        if self.daemon is not None:
            if self.timer is True:
                self.daemon.cancel()
            else:
                self.daemon.join()
            self.daemon = None

    #
    # Config
    #

    def freq2time(self, freq):
        frequency = float(frequency)
        if not frequency > 0.0:
            raise ValueError("Frequency is not a positive float")
        return float(1. / float(frequency))

    def calendar(self):
        return Calendar()

    def set(self, timeout, maxsteps=0):
        self.timeout = timeout
        self.maxsteps = maxsteps
        return self

    #
    # Step settings
    #

    def remove(self, evt):
        self._scheduled.remove(evt)
        return self

    def add(self, step, steptime, *args, **kwargs):
        if not callable(step):
            raise ValueError("Not scheduling a function")
        if steptime < self.pausetime:
            self.pausetime = steptime
        evt = Model(step, steptime, args, kwargs)
        self._scheduled.append(evt)
        return self

    #
    # Scheduler
    #

    def __run_thread(self):
        if self.timer is True:
            self.daemon = threading.Thread(name=self.get_name(),
                                            target=self.__start_thread)
        else:
            self.daemon = threading.Timer(self.sched.time, self.__start_timer)
        self.daemon.setDaemon(True)
        self.daemon.start()

    def __start_thread(self):
        self.__run()

    def __start_timer(self):
        self.timer = False
        self.__run()

    def __run(self):
        self.__stop = False
        lock = self.__event
        lock.clear()
        timeout = self.timeout
        maxsteps = self.maxsteps
        get_time = self.get_time
        pausetime = self.pausetime
        sched = self._scheduled
        sched.sort(key=lambda evt: evt.sleeptime)
        steps = 0
        start = get_time()
        for evt in sched:
            evt.attime = start + evt.sleeptime
        while not self.__stop:
            now = get_time()
            if timeout is not None and now - start >= timeout:
                break
            sleeptime = pausetime
            for evt in sched:
                nexttime = evt.attime
                if now >= nexttime:
                    evt.step(*evt.args, **evt.kwargs)
                    after = get_time()
                    evt.add_stat(after - now)
                    evt.attime = now + evt.sleeptime
                    steps += 1
                    if steps == maxsteps:
                        break
                else:
                    diff = nexttime - now
                    if diff < sleeptime:
                        sleeptime = diff
                    break
            after = get_time()
            timespent = after - now
            diff = sleeptime - timespent
            sleeptime = diff if diff > 0.0 else 0
            if timeout is not None and ((after + sleeptime) - start) > timeout:
                break
            lock.wait(timeout=sleeptime)
