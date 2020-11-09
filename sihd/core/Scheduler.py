#!/usr/bin/python
# coding: utf-8

import time
import threading
from .ANamedObject import ANamedObject
from .IService import IService

from .SihdRunnableObject import SihdRunnableObject

class Calendar(object):

    def __init__(self, get_time=time.time):
        self.time = 0
        self.get_time = get_time

    def _decal(self, sec):
        self.time += sec
        return self

    def reset(self):
        self.time = 0
        return self

    def now(self):
        self.time = self.get_time()
        return self

    def at(self, t):
        self.time = t
        return self

    def day(self, day):
        return self._decal(day * 24 * 60 * 60)

    def hour(self, hour):
        return self._decal(hour * 60 * 60)

    def minute(self, minute):
        return self._decal(minute * 60)

    def second(self, sec):
        return self._decal(sec)

    def ms(self, ms):
        return self._decal(ms / 1E3)

class Task(object):

    def __init__(self, fun, at=0, period=0, end=None, args=(), kwargs={}):
        self.fun = fun
        self.on_end = end
        self.args = args
        self.kwargs = kwargs
        self.period = period
        self.at = at
        self.min = 1E9
        self.max = 0
        self.uptime = 0
        self.n = 0

    def step(self):
        self.fun(*self.args, **self.kwargs)

    def add_stat(self, exe_time):
        if exe_time < self.min:
            self.min = exe_time
        if exe_time > self.max:
            self.max = exe_time
        self.uptime += exe_time
        self.n += 1

    def end(self):
        callback = self.on_end
        if callback is not None:
            callback(self)

    def __str__(self):
        return "Task: period={} at={}".format(self.period, self.at)

class Scheduler(ANamedObject, IService):

    def __init__(self, name="Scheduler", timeout=None,
                    thread=False, daemon=None,
                    get_time=time.time, **kwargs):
        # Objects
        self.plan = Calendar(get_time)
        # Settings
        self.daemon = daemon
        self.thread = thread
        self.timeout = timeout
        # Other
        self.timedout = False
        self.overruns = 0
        # Internal
        self._scheduled = []
        self.__timer = False
        self.__get_time = get_time
        self.__event = threading.Event()
        self.__stop = False
        super().__init__(name, **kwargs)

    #
    # Service
    #

    def start(self):
        #self._scheduled.sort(key=lambda task: (task.at, task.period))
        self.overruns = 0
        self.timedout = False
        threaded = self.thread == True
        if self.plan.time > 0:
            threaded = True
            self.__timer = True
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
            if self.__timer is True:
                self.daemon.cancel()
            else:
                self.daemon.join()
            self.daemon = None

    #
    # Config
    #

    def freq2time(self, freq):
        frequency = float(freq)
        if not frequency > 0.0:
            raise ValueError("Frequency is not a positive float")
        return float(1. / float(frequency))

    def calendar(self):
        return Calendar()

    #
    # Step settings
    #

    def remove(self, task):
        self._scheduled.remove(task)
        return self

    def schedule_sihd_obj(self, obj):
        if not isinstance(obj, SihdRunnableObject):
            raise ValueError("{} -> Not a SihdRunnableObject"\
                                .format(obj.__class__.__name__))
        frequency = obj.configuration.get("runnable_frequency")
        obj.configuration.set("runnable_type", "none")
        return self.schedule_frequency(obj.step, frequency)

    def schedule_frequency(self, step, freq, *args, **kwargs):
        return self.schedule_period(step, self.freq2time(freq), *args, **kwargs)

    def schedule_period(self, step, period, *args, **kwargs):
        if not callable(step):
            raise ValueError("Not scheduling a function")
        task = Task(step, period=period, args=args, kwargs=kwargs)
        self._scheduled.append(task)
        return self

    def schedule_once(self, step, time, *args, **kwargs):
        if not callable(step):
            raise ValueError("Not scheduling a function")
        task = Task(step, at=time, args=args, kwargs=kwargs)
        self._scheduled.append(task)
        return self

    #
    # Scheduler
    #

    def __run_thread(self):
        if self.__timer is False:
            self.daemon = threading.Thread(name=self.get_name(),
                                            target=self.__start_thread)
        else:
            self.daemon = threading.Timer(self.plan.time, self.__start_timer)
        self.daemon.setDaemon(True)
        self.daemon.start()

    def __start_thread(self):
        self.__run()

    def __start_timer(self):
        self.__timer = False
        self.__run()

    def __run(self):
        self.__stop = False
        lock = self.__event
        lock.clear()
        sched = self._scheduled.copy()
        period = None
        timeout = self.timeout
        get_time = self.__get_time
        steps = 0
        todelete = []
        overruns = 0
        start = get_time()
        for task in sched:
            if task.period != 0:
                task.at = start + task.period
        while not self.__stop:
            now = get_time()
            after = now
            # Timeout
            if timeout is not None and now - start >= timeout:
                break
            lowest = -1
            for task in sched:
                next_time = task.at
                # Check overruns
                if next_time != 0 and now >= next_time + 0.001:
                    overruns += 1
                if now >= next_time:
                    # Run task
                    proceed = task.step() is not False
                    # Stat
                    after = get_time()
                    task.add_stat(after - now)
                    period = task.period
                    if proceed and period != 0:
                        # Reschedule
                        t = now + period
                        task.at = t
                        if t <= lowest or lowest == -1:
                            lowest = t
                    else:
                        proceed = False
                    if not proceed:
                        todelete.append(task)
                elif next_time <= lowest or lowest == -1:
                    lowest = next_time
            # Delete tasks
            for task in todelete:
                task.end()
                sched.remove(task)
            todelete.clear()
            # Leave if no more tasks
            if len(sched) == 0:
                break
            after = get_time()
            towait = lowest - after
            # Timeout
            if timeout is not None and ((after + towait) - start) > timeout:
                self.timedout = True
                break
            # Wait
            if towait >= 0.0:
                lock.wait(timeout=towait)
        for task in sched:
            task.end()
        self.overruns = overruns
