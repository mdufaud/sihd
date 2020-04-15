#!/usr/bin/python
#coding: utf-8

""" System """
from .IObservable import IObservable
from .IObserver import IObserver

from .IProducer import IProducer
from .IConsumer import IConsumer

import queue
import threading

multiprocessing = None
mp_manager = None
mp_base_manager = None

def _setup_mp():
    global multiprocessing
    if multiprocessing is None:
        import multiprocessing

def _setup_mp_base_manager():
    _setup_mp()
    global mp_base_manager
    if mp_base_manager is None:
        from multiprocessing.managers import BaseManager
        mp_base_manager = BaseManager()
        mp_base_manger.start()

def _setup_mp_manager():
    _setup_mp()
    global mp_manager
    if mp_manager is None:
        mp_manager = multiprocessing.Manager()

def register_channel_object(name, cls):
    global BaseManager
    if BaseManager is None:
        _setup_mp()
        from multiprocessing.managers import BaseManager
    BaseManager.register(name, cls)

class Channel(IObservable, IObserver):

    def __init__(self, mp=False, parent=None, name="Channel",
                    block=False, timeout=None,
                    default=None):
        super(Channel, self).__init__(name)
        self.__last_data = None
        if default is not None:
            self.write(default)
        self.__parent = parent
        self.__mp = mp
        self.set_timeout(timeout)
        self.set_block(block)

    """ Base methods """

    def is_multiprocess(self):
        return self.__mp

    def get_timeout(self):
        return self.__timeout

    def is_block(self):
        return self.__block

    def set_timeout(self, timeout):
        self.__timeout = timeout

    def set_block(self, block):
        self.__block = block

    def on_notify(self, observable):
        """ Called when notified by observable """
        #If notified, write data from observable into channel
        if observable.readable():
            data = observable.read()
            self.write(data)

    def get_parent(self):
        """ Get channel's parent """
        return self.__parent

    """ Methods to change """

    def write(self, data):
        """ Write a data to a channel
            which trigger an observation to all observers """
        self.__last_data = data
        self.notify_observers()
        return True

    def read(self):
        """ Return the channel's data """
        return self.__last_data

    def readable(self):
        """ Return True when channel can be read """
        return True

    def pollable(self):
        """ Return True if the channel if readable in a loop """
        return False

    def notify(self):
        """ Trigger an observation to all observers """
        self.notify_observers()

    def get_data(self):
        """ Get the data object """
        return self.__last_data

    def clear(self):
        """ Called when channel is read """
        pass

    def get_description(self):
        l = []
        if self.is_block():
            l.append("block")
        else:
            l.append("no-block")
        timeout = self.get_timeout()
        if timeout:
            l.append("timeout={}".format(timeout))
        if self.pollable():
            l.append("pollable")
            if self.readable():
                l.append("readable")
        if self.is_multiprocess():
            l.append("multiprocessed")
        return ", ".join(l)

""" Children """

class ChannelQueue(Channel):

    def __init__(self, size=0, mp=False, name="ChannelQueue", **kwargs):
        super(ChannelQueue, self).__init__(mp=mp, name=name, **kwargs)
        if mp is True:
            _setup_mp()
            #self.__queue = multiprocessing.JoinableQueue(maxsize=size)
            self.__queue = multiprocessing.Queue(maxsize=size)
        else:
            self.__queue = queue.Queue(maxsize=size)

    def write(self, data):
        try:
            self.__queue.put(data, block=self.is_block(), timeout=self.get_timeout())
        except queue.Full:
            return False
        super().write(data)
        return True

    def read(self):
        try:
            data = self.__queue.get(block=self.is_block(), timeout=self.get_timeout())
        except queue.Empty:
            data = None
        return data

    def readable(self):
        return self.__queue.empty() == False

    def pollable(self):
        return True

    def get_data(self):
        return self.__queue

    def clear(self):
        #self.__queue.task_done()
        pass

class ChannelTrigger(Channel):

    def __init__(self, trigger=False, mp=False, name="ChannelTrigger", **kwargs):
        super(ChannelTrigger, self).__init__(mp=mp, name=name, **kwargs)
        self.__trigger = trigger
        if trigger is True:
            if mp is True:
                _setup_mp()
                self.__event = multiprocessing.Event()
            else:
                if threading is None:
                    import threading
                self.__event = threading.Event()

    def notify(self):
        super().notify()
        if self.__trigger is True:
            self.__event.set()

    def clear(self):
        if self.__trigger is True:
            self.__event.clear()

    def pollable(self):
        return self.__trigger is True

    def readable(self):
        return self.__event.is_set()

class ChannelObject(ChannelTrigger):

    def __init__(self, obj_args, mp=False, name="ChannelDict", **kwargs):
        super(ChannelDict, self).__init__(mp=mp, name=name, **kwargs)
        cls = obj_args.pop("class")
        args = obj_args.pop("args", ())
        kwargs = obj_args.pop("kwargs", {})
        if mp is True:
            _setup_mp_base_manager()
            create_obj = getattr(mp_base_manager, cls)
            self.__obj = create_obj(*args, **kwargs)
        else:
            self.__obj = cls(*args, **kwargs)

    def write(self, key, value):
        try:
            item = getattr(self.__obj, key)
        except AttributeError:
            return False
        item = value
        return super().write((key, value))

    def read(self, key, ret=None):
        try:
            item = getattr(self.__obj, key)
        except AttributeError:
            return ret
        return item

    def get_data(self):
        return self.__obj

class ChannelDict(ChannelTrigger):

    def __init__(self, mp=False, name="ChannelDict", **kwargs):
        super(ChannelDict, self).__init__(mp=mp, name=name, **kwargs)
        if mp is True:
            _setup_mp_manager()
            self.__dict = mp_manager.dict()
        else:
            self.__dict = dict()

    def write(self, key, value):
        self.__dict[key] = value
        return super().write((key, value))

    def read(self, key, ret=None):
        return self.__dict.get(key, ret)

    def get_data(self):
        return self.__dict

class ChannelList(ChannelTrigger):

    def __init__(self, name="ChannelList", **kwargs):
        super(ChannelList, self).__init__(mp=mp, name=name, **kwargs)
        if mp is True:
            _setup_mp_manager()
            self.__list = mp_manager.list()
        else:
            self.__list = list()

    def write(self, value, i=None):
        l = self.__list
        if isinstance(value, list):
            l.clear()
            for v in value:
                l.append(v)
        elif i is None:
            l.append(value)
        elif i < len(l):
            l[i] = value
        else:
            return False
        return super().write((value, i))

    def read(self, i):
        l = self.__list
        if i < len(l):
            return l[i]
        return None

    def get_data(self):
        return self.__list

class ChannelCondition(Channel):

    def __init__(self, lock=None, mp=False, name="ChannelCondition", **kwargs):
        super(ChannelCondition, self).__init__(name=name, mp=mp, **kwargs)
        if mp is True:
            _setup_mp()
            self.__condition = multiprocessing.Condition(lock=lock)
        else:
            self.__condition = threading.Condition(lock=lock)

    def write(self, data):
        cd = self.__condition
        try:
            ret = cd.wait(timeout=self.get_timeout())
        except RuntimeError:
            return False
        if ret:
            ret = super().write(data)
            cd.notify_all()
        return ret

    def read(self):
        cd = self.__condition
        with cd:
            ret = cd.wait(timeout=timeout)
        return ret

class ChannelEvent(Channel):

    def __init__(self, default=False, mp=False, name="ChannelEvent", **kwargs):
        super(ChannelEvent, self).__init__(name=name, mp=mp, **kwargs)
        if mp is True:
            _setup_mp()
            self.__event = multiprocessing.Event()
        else:
            self.__event = threading.Event()
        if default is True:
            self.__event.set()
        else:
            self.__event.clear()

    def write(self, data):
        ev = self.__event
        if data:
            ev.set()
        else:
            ev.clear()
        return super().write(ev.is_set())

    def read(self):
        ev = self.__event
        if ev.wait(timeout=self.get_timeout()):
            return True
        return False

    def clear(self):
        self.__event.clear()

""" Multiprocess only """

class ChannelPipe(Channel):

    def __init__(self, pipe, name="ChannelPipe", child=False, **kwargs):
        super(ChannelPipe, self).__init__(name=name, mp=True, **kwargs)
        self.__pipe = pipe

    def write(self, data):
        self.__pipe.send(data)
        return super().write(data)

    def read(self):
        try:
            ret = self.__pipe.recv()
        except EOFError:
            return None
        return ret

    def readable(self):
        return self.__pipe.poll(timeout=self.get_timeout())

    def pollable(self):
        return True

class ChannelValue(ChannelTrigger):

    def __init__(self, type='i', default=0, mp=True, name="ChannelValue", **kwargs):
        super(ChannelValue, self).__init__(mp=True, name=name, **kwargs)
        _setup_mp()
        self.__value = multiprocessing.Value(type, default)
        self.__type = type

    def write(self, data):
        ret = False
        val = self.__value
        lock = val.get_lock()
        if lock.acquire(block=self.is_block(), timeout=self.get_timeout()):
            val.value = data
            ret = super().write(data)
            lock.release()
        return ret

    def read(self):
        val = self.__value
        lock = val.get_lock()
        if lock.acquire(block=self.is_block(), timeout=self.get_timeout()):
            ret = val.value
            return ret
        return None

    def get_data(self):
        return self.__value

class ChannelArray(ChannelTrigger):

    def __init__(self, type='i', size=10, mp=True, name="ChannelArray", **kwargs):
        super(ChannelArray, self).__init__(mp=True, name=name, **kwargs)
        _setup_mp()
        self.__array = multiprocessing.Array(type, range(size))
        self.__idx = 0
        self.__type = type
        self.__size = size

    def write(self, data, i=None):
        l = self.__array
        if isinstance(value, list):
            for i, v in enumerate(value):
                l[i] = v
        elif i is not None and i < self.__size:
            l[i] = value
        else:
            return False
        return super().write((data, i))

    def read(self, n):
        ret = None
        if n is not None:
            if n >= self.__size or n < 0:
                ret = self.__array[n]
        return ret

    def get_data(self):
        return self.__array
