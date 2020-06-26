#!/usr/bin/python
# coding: utf-8

""" System """
import time
import collections

class PerfStat(object):
    def __init__(self, uptime=0.0, downtime=0.0,
                        maxtime=0.0, mintime=0.0,
                        calls=0):
        self.uptime = uptime
        self.downtime = downtime
        self.maxtime = maxtime
        self.mintime = mintime
        self.avgtime = uptime / calls if calls > 0 else 0.0
        self.calls = calls

    def add_time(self, time):
        if time > self.maxtime:
            self.maxtime = time
        if time < self.mintime:
            self.mintime = time
        self.calls += 1
        self.avgtime += (time - self.avg_time) / self.calls

    def __str__(self):
        string = ("Performances:\n\t"
                "Thread: uptime: {:.3f} s - downtime: {:.3f} s\n\t"
                "For {:d} steps: avg: {:.3f} ms (max: {:.3f} ms - min: {:.3f} ms)"\
                .format(self.uptime, self.downtime, self.calls,
                        self.avgtime * 1e3, self.maxtime * 1e3, self.mintime * 1e3))
        return string

#TODO kind of deprecated

class MethodStat(object):
    def __init__(self, method):
        self.name = method.__name__
        self.mod = method.__module__
        self.calls = 0
        self.best_time = 0.0
        self.worst_time = 0.0
        self.avg_time = 0.0
        self.ret_values = []

    def add_return(self, ret):
        self.ret_values.append(ret)

    def get_return_stat(self):
        vals = collections.defaultdict(int)
        for value in self.ret_values:
            vals[value] += 1
        return vals

    def format_return_stat(self):
        s = ""
        dic = self.get_return_stat()
        for key, value in dic.items():
            if s:
                s += " - "
            s += "{:s}: {:d}".format(str(key), value)
        return s


    def add_time(self, time):
        if time > self.worst_time:
            self.worst_time = time
        if time < self.best_time:
            self.best_time = time
        self.calls += 1
        self.avg_time = self.avg_time + ((time - self.avg_time) / self.calls)

    def add_count(self):
        self.calls += 1

    def __str__(self):
        string = ("Method '{:s}.{:s}': {:d} calls\n\t"
                "Time: {:.3f} ms avg (worst: {:.3f} ms - best: {:.3f} ms)")\
                        .format(self.mod, self.name, self.calls,
                                self.avg_time * 1e3, self.worst_time * 1e3,
                                self.best_time * 1e3)
        if self.ret_values:
            string += "\n\tReturned: [{}]".format(self.format_return_stat())
        return string

_stat_dic = dict()

def __get_stat(method):
    """ For internal usage only """
    global _stat_dic
    obj = _stat_dic.get(method, None)
    if obj is None:
        obj = MethodStat(method)
        _stat_dic[method] = obj
    return obj

def stat_it(method):
    def wrapper(*args, **kwargs):
        begin = time.time()
        ret = method(*args, **kwargs)
        end = time.time()
        obj = __get_stat(method)
        obj.add_time(end - begin)
        obj.add_return(ret)
        return ret
    return wrapper

def count_it(method):
    def wrapper(*args, **kwars):
        ret = method(*args, **kwargs)
        __get_stat(method).add_count()
        return ret
    return wrapper

def get_stats():
    return _stat_dic

def reset():
    global _stat_dic
    _stat_dic = dict()
