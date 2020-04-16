#!/usr/bin/python
#coding: utf-8

""" System """
import time
import queue
import threading

from .IObservable import IObservable
from .IObserver import IObserver

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
                    default=None, timestamp=False):
        super(Channel, self).__init__(name)
        self.__last_data = None
        self.__ts = 0
        if mp is True:
            _setup_mp()
            self.__lock = multiprocessing.Lock()
        else:
            self.__lock = threading.Lock()
        self.__alock = self.__lock.acquire
        self.__rlock = self.__lock.release
        self.__locked = False
        self.set_timeout(timeout)
        self.set_block(block)
        self.set_timestamp(timestamp)
        self.__parent = parent
        self.__mp = mp
        if default is not None:
            self.write(default)

    """ Base methods """

    def is_multiprocess(self):
        return self.__mp

    def is_block(self):
        return self.__block

    def set_block(self, block):
        self.__block = block

    def get_timeout(self):
        return self.__timeout

    def set_timeout(self, timeout):
        self.__timeout = timeout

    def set_timestamp(self, active):
        """ Activate timestamping """
        self.__ts = active

    def save_time(self):
        if self.__ts:
            self.__last_ts = time.time()

    def get_time(self):
        return self.__last_ts

    def notify(self):
        """ Trigger an observation to all observers """
        self.notify_observers()

    def on_notify(self, observable):
        """ Called when notified by observable """
        #If notified, write data from observable into channel
        if observable.is_readable():
            data = observable.read()
            self.write(data)

    def get_parent(self):
        """ Get channel's parent """
        return self.__parent

    def lock(self):
        """ Lock readability of channel """
        ret = self.__alock(block=self.is_block(), timeout=self.get_timeout())
        self.__locked = ret
        return ret

    def unlock(self):
        """ Unlock readability of channel """
        try:
            self.__rlock()
        except (ValueError, threading.ThreadError):
            return False
        self.__locked = False
        return True

    def is_locked(self):
        """ Check if channel is locked """
        return self.__locked

    """ Methods to change while inheriting """

    def write(self, data):
        """ Write a data to a channel
            which trigger an observation to all observers """
        self.save_time()
        self.__last_data = data
        self.notify_observers()
        return True

    def read(self):
        """ Return the channel's data """
        return self.__last_data

    def clear(self):
        """ To be called after reading the channel if implemented """
        pass

    def is_readable(self):
        """ Return True when channel can be read """
        return not self.is_locked()

    def is_pollable(self):
        """ Return True if the channel if readable in a loop """
        return False

    def get_data(self):
        """ Get the data object """
        return self.__last_data

    def get_description(self):
        l = []
        if self.is_block():
            l.append("block")
        else:
            l.append("no-block")
        timeout = self.get_timeout()
        if timeout:
            l.append("timeout={}".format(timeout))
        if self.is_pollable():
            l.append("is_pollable")
            if self.is_readable():
                l.append("is_readable")
        if self.is_multiprocess():
            l.append("multiprocessed")
        return ", ".join(l)

""" Children """

class ChannelQueue(Channel):

    def __init__(self, size=0, mp=False, from_manager=False, simple=False, name="ChannelQueue", **kwargs):
        if mp is True:
            _setup_mp()
            if from_manager is True:
                _setup_mp_manager()
                self.__queue = mp_manager.Queue(maxsize=size)
            elif simple is True:
                self.__queue = multiprocessing.SimpleQueue()
                self.write = self.__simple_write
                self.read = self.__simple_read
            else:
                self.__queue = multiprocessing.Queue(maxsize=size)
        else:
            self.__queue = queue.Queue(maxsize=size)
        self.__put = self.__queue.put
        self.__get = self.__queue.get
        self.__empty = self.__queue.empty
        super(ChannelQueue, self).__init__(mp=mp, name=name, **kwargs)

    def __simple_write(self, data):
        self.__put(data)
        super().write(data)
        return True

    def __simple_read(self):
        return self.__get()

    def write(self, data):
        try:
            self.__put(data, block=self.is_block(), timeout=self.get_timeout())
        except queue.Full:
            return False
        super().write(data)
        return True

    def read(self):
        try:
            data = self.__get(block=self.is_block(), timeout=self.get_timeout())
        except queue.Empty:
            data = None
        return data

    def is_readable(self):
        return self.is_locked() is False and self.__empty() is False

    def is_pollable(self):
        return True

    def get_data(self):
        return self.__queue

class PollableChannel(Channel):

    def __init__(self, poll=False, mp=False, name="PollableChannel", **kwargs):
        self.__poll = poll
        if poll is True:
            if mp is True:
                _setup_mp()
                self.__event = multiprocessing.Event()
            else:
                self.__event = threading.Event()
            self.__clear = self.__event.clear
            self.__set = self.__event.set
            self.__is = self.__event.is_set
        super(PollableChannel, self).__init__(mp=mp, name=name, **kwargs)

    def notify(self):
        super().notify()
        if self.__poll is True:
            self.__set()

    def clear(self):
        if self.__poll is True:
            self.__clear()

    def is_pollable(self):
        return self.__poll is True

    def is_readable(self):
        return not self.is_locked() and (self._poll is False or self.__is())

class ChannelObject(PollableChannel):

    def __init__(self, obj_args, mp=False, name="ChannelObject", **kwargs):
        cls = obj_args.pop("class")
        args = obj_args.pop("args", ())
        kwargs = obj_args.pop("kwargs", {})
        if mp is True:
            _setup_mp_base_manager()
            create_obj = getattr(mp_base_manager, cls)
            self.__obj = create_obj(*args, **kwargs)
        else:
            self.__obj = cls(*args, **kwargs)
        super(ChannelObject, self).__init__(mp=mp, name=name, **kwargs)

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

class ChannelDict(PollableChannel):

    def __init__(self, mp=False, name="ChannelDict", **kwargs):
        if mp is True:
            _setup_mp_manager()
            self.__dict = mp_manager.dict()
        else:
            self.__dict = dict()
        super(ChannelDict, self).__init__(mp=mp, name=name, **kwargs)

    def write(self, key, value=None):
        if key is None:
            self.__dict.clear()
        elif isinstance(key, dict):
            d = self.__dict
            d.clear()
            d.update(key)
        else:
            self.__dict[key] = value
        return super().write((key, value))

    def read(self, key, ret=None):
        return self.__dict.get(key, ret)

    def get_data(self):
        return self.__dict

class ChannelList(PollableChannel):

    def __init__(self, mp=False, name="ChannelList", **kwargs):
        if mp is True:
            _setup_mp_manager()
            self.__list = mp_manager.list()
        else:
            self.__list = list()
        self.__append = self.__list.append
        self.__clear = self.__list.clear
        super(ChannelList, self).__init__(mp=mp, name=name, **kwargs)

    def write(self, value, i=None):
        l = self.__list
        if isinstance(value, list):
            self.__clear()
            for v in value:
                self.__append(v)
        elif i is None:
            self.__append(v)
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
        if mp is True:
            _setup_mp()
            self.__condition = multiprocessing.Condition(lock=lock)
        else:
            self.__condition = threading.Condition(lock=lock)
        super(ChannelCondition, self).__init__(name=name, mp=mp, **kwargs)

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

class ChannelBool(PollableChannel):

    def __init__(self, default=False, mp=False, name="ChannelBool", **kwargs):
        if mp is True:
            _setup_mp()
            self.__event = multiprocessing.Event()
        else:
            self.__event = threading.Event()
        if default is True:
            self.__event.set()
        else:
            self.__event.clear()
        self.__set = self.__event.set
        self.__clear = self.__event.clear
        self.__is = self.__event.is_set
        self.__wait = self.__event.wait
        super(ChannelBool, self).__init__(name=name, mp=mp, **kwargs)

    def write(self, data):
        if data:
            self.__set()
        else:
            self.__clear()
        return super().write(bool(data))

    def read(self):
        if self.__wait(timeout=self.get_timeout()):
            return True
        return False

""" Multiprocess only """

class ChannelValue(PollableChannel):

    def __init__(self, var_type='i', default=0, mp=True, name="ChannelValue", **kwargs):
        """
            Codes for type
            Type C / Type Python / Min Byte size / Notes
            'b' signed char int 1
            'B' unsigned char int 1
            'u' Py_UNICODE Caractère Unicode 2 (1)
            'h' signed short int 2
            'H' unsigned short int 2
            'i' signed int int 2
            'I' unsigned int int 2
            'l' signed long int4
            'L' unsigned long int 4
            'q' signed long long int 8
            'Q' unsigned long long int 8
            'f' float float 4
            'd' double float 8
        """
        _setup_mp()
        self.__value = multiprocessing.Value(var_type, default)
        self.__type = var_type
        self.__alock = self.__value.get_lock().acquire
        self.__rlock = self.__value.get_lock().release
        super(ChannelValue, self).__init__(mp=True, name=name, **kwargs)

    def write(self, data):
        ret = False
        if self.__alock(block=self.is_block(), timeout=self.get_timeout()):
            self.__value.value = data
            ret = super().write(data)
            self.__rlock()
        return ret

    def read(self):
        if self.__alock(block=self.is_block(), timeout=self.get_timeout()):
            ret = self.__value.value
            return ret
        return None

    def get_data(self):
        return self.__value

class ChannelArray(PollableChannel):

    def __init__(self, var_type='i', size=10, mp=True, name="ChannelArray", **kwargs):
        """
            Codes for type
            Type C / Type Python / Min Byte size / Notes
            'b' signed char int 1
            'B' unsigned char int 1
            'u' Py_UNICODE Caractère Unicode 2 (1)
            'h' signed short int 2
            'H' unsigned short int 2
            'i' signed int int 2
            'I' unsigned int int 2
            'l' signed long int4
            'L' unsigned long int 4
            'q' signed long long int 8
            'Q' unsigned long long int 8
            'f' float float 4
            'd' double float 8
        """
        _setup_mp()
        self.__array = multiprocessing.Array(var_type, range(size))
        self.__idx = 0
        self.__type = var_type
        self.__size = size
        self.__idx = 0
        super(ChannelArray, self).__init__(mp=True, name=name, **kwargs)

    def write(self, data, i=None):
        l = self.__array
        if isinstance(value, list):
            for i, v in enumerate(value):
                l[i] = v
        elif i is None:
            l[self.__idx] = data
            if self.__idx + 1 >= self.__size:
                self.__idx = 0
            else:
                self.__idx += 1
        elif i < self.__size:
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

    def clear(self):
        a = self.__array
        for i in range(0, self.__size):
            a = 0
