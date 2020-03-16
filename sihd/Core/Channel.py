#!/usr/bin/python
#coding: utf-8

""" System """
from .IObservable import IObservable
from .IObserver import IObserver

multiprocessing = None

class Channel(IObservable, IObserver):

    def __init__(self, parent=None, name="Channel",
                    block=False, timeout=None):
        super(Channel, self).__init__(name)
        self.__last_data = None
        self.__parent = parent
        self.set_timeout(timeout)
        self.set_block(block)

    """ Base methods """

    def get_timeout(self):
        return self.__timeout

    def is_block(self):
        return self.__block

    def set_timeout(self, timeout):
        self.__timeout = timeout

    def set_block(self, block):
        self.__block = block

    def on_notify(self, observable):
        #If notified, write data from observable into channel
        data = observable.read()
        self.write(data)

    def get_parent(self):
        return self.__parent

    """ Methods to change """

    def write(self, data):
        self.__last_data = data
        self.notify_observers()
        return True

    def read(self):
        return self.__last_data

    def readable(self):
        return True

    def pollable(self):
        return False

    def notify(self):
        self.notify_observers()

""" Children """

from .IProducer import IProducer
from .IConsumer import IConsumer

class ChannelQueue(Channel, IProducer, IConsumer):

    def __init__(self, parent=None, name="ChannelQueue", **kwargs):
        super(ChannelQueue, self).__init__(name=name, parent=parent, **kwargs)
        self.consume_queue(self.get_producing_queue())

    def write(self, data):
        if self.produce(data, block=self.is_block(), timeout=self.get_timeout()):
            super().write(data)
            return True
        return False

    def read(self):
        data = self.consume(block=self.is_block(), timeout=self.get_timeout())
        return data

    def readable(self):
        return self.is_queue_empty() == False

    def pollable(self):
        return True

class ChannelValue(Channel):

    def __init__(self, type='i', default=0, parent=None, name="ChannelValue"):
        super(ChannelValue, self).__init__(name=name, parent=parent)
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        self.__value = multiprocessing.Value(type, default)
        self.__type = type

    def write(self, data):
        val = self.__value
        lock = val.get_lock()
        if lock.acquire(block=self.is_block(), timeout=self.get_timeout()):
            val.value = data
            super().write(data)
            lock.release()
            return True
        return False

    def read(self):
        val = self.__value
        lock = val.get_lock()
        if lock.acquire(block=self.is_block(), timeout=self.get_timeout()):
            ret = val.value
            return ret
        return None

class ChannelArray(Channel):

    def __init__(self, type='i', size=10, parent=None, name="ChannelArray"):
        super(ChannelArray, self).__init__(name=name, parent=parent)
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        self.__array = multiprocessing.Array(type, range(size))
        self.__lock = multiprocessing.Lock()
        self.__idx = 0
        self.__type = type
        self.__size = size

    def write(self, data):
        arr = self.__array
        size = self.__size
        if self.__lock.acquire(block=self.is_block(), timeout=self.get_timeout()):
            if isinstance(data, list):
                for i, d in enumerate(data):
                    if i >= size:
                        break
                    arr[i] = d
            else:
                arr[self.__idx] = data
                if self.__idx + 1 == size:
                    self.__idx = 0
                else:
                    self.__idx += 1
            super().write(data)
            self.__lock.release()
            return True
        return False

    def read(self, n):
        if n >= self.__size or n < 0:
            return None
        ret = None
        if self.__lock.acquire(block=self.is_block(), timeout=self.get_timeout()):
            ret = self.__array[n]
            self.__lock.release()
        return ret

class ChannelCondition(Channel):

    def __init__(self, parent=None, lock=None, name="ChannelCondition"):
        super(ChannelCondition, self).__init__(name=name, parent=parent)
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        self.__condition = multiprocessing.Condition(lock=lock)

    def write(self, data):
        cd = self.__condition
        try:
            ret = cd.wait(timeout=self.get_timeout())
        except RuntimeError:
            return False
        if ret:
            super().write(data)
            cd.notify_all()
        return ret

    def read(self):
        cd = self.__condition
        with cd:
            ret = cd.wait(timeout=timeout)
        return ret

class ChannelEvent(Channel):

    def __init__(self, parent=None, name="ChannelEvent"):
        super(ChannelEvent, self).__init__(name=name, parent=parent)
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        self.__event = multiprocessing.Event()

    def write(self, data):
        ev = self.__event
        if data:
            ev.set()
        else:
            ev.clear()
        super().write(ev.is_set())
        return True

    def read(self):
        ev = self.__event
        if ev.wait(timeout=self.get_timeout()):
            return True
        return False

class ChannelPipe(Channel):

    def __init__(self, pipe, parent=None, name="ChannelPipe", child=False):
        super(ChannelPipe, self).__init__(name=name, parent=parent)
        self.__pipe = pipe

    def write(self, data):
        self.__pipe.send(data)

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
