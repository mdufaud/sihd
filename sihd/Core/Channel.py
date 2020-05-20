#!/usr/bin/python
#coding: utf-8

#
# System
#
import time
import queue
import threading
import ctypes

import logging
logger = logging.getLogger("sihd")

from .AObservable import AObservable
from .IObserver import IObserver
from .ALoggable import ALoggable

array = None
RawArray = None
multiprocessing = None
mp_manager = None
BaseManager = None
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
        mp_base_manager.start()

def _setup_mp_manager():
    _setup_mp()
    global mp_manager
    if mp_manager is None:
        mp_manager = multiprocessing.Manager()

def register_channel_object(name, cls):
    global BaseManager
    if BaseManager is None:
        try:
            _setup_mp()
            from multiprocessing.managers import BaseManager
        except (ImportError, FileNotFoundError):
            pass
    if BaseManager is not None:
        BaseManager.register(name, cls)
    ChannelObject.register_class(name, cls)

class Channel(AObservable, IObserver, ALoggable):

    def __init__(self, name="Channel", mp=False, parent=None,
                    block=True, timeout=0.001, log=True,
                    default=None, timestamp=False):
        super().__init__(name, parent=parent)
        #Timestamping and lock
        self.__ts = False
        if log is False:
            self._set_log(False)
        if mp is True:
            _setup_mp()
            self.__last_ts = multiprocessing.Value('i', 0)
            self.lock = self.__lock_mp
            self.do_timestamp = self.__do_timestamp_mp
            self.get_timestamp = self.__get_timestamp_mp
            self.__lock = multiprocessing.Lock()
        else:
            self.__last_ts = 0
            self.__lock = threading.Lock()
        self.__alock = self.__lock.acquire
        self.__rlock = self.__lock.release
        self.__locked = False
        #Stored last data
        self._last_data = None
        #Multiprocessing
        self.__mp = mp
        self.set_timeout(timeout)
        self.set_block(block)
        self.set_timestamp(timestamp)
        if default is not None:
            self.write(default)

    """ Base methods """

    def get_lock(self):
        return self.__lock

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

    def do_timestamp(self):
        if self.__ts:
            self.__last_ts = time.time()

    def __do_timestamp_mp(self):
        if self.__ts:
            self.__last_ts.value = time.time()

    def get_timestamp(self):
        return self.__last_ts

    def __get_timestamp_mp(self):
        return self.__last_ts.value

    def notify(self):
        """ Trigger an observation to all observers """
        self.notify_observers()

    def on_notify(self, channel):
        """
            Called when notified by a channel.
            Write channel's value in its own.
                channel wrote -> obs notified -> writing value -> notifies
        """
        if channel.is_readable():
            data = channel.read()
            self.write(data)

    #
    # Lock
    #

    def __lock_mp(self, block=True, timeout=0.01) -> bool:
        """
            Lock read/write for a channel in multiprocess
            :return: True if channel is locked
        """
        ret = self.__alock(block=block, timeout=timeout)
        self.__locked = ret
        return ret

    def lock(self, block=True, timeout=0.01) -> bool:
        """
            Lock read/write for a channel
            :return: True if channel is locked
        """
        timeout = -1 if timeout is None else timeout
        ret = self.__alock(blocking=block, timeout=timeout)
        self.__locked = ret
        return ret

    def unlock(self) -> bool:
        """
            Unlock read/write for a channel
            :return: True if channel was locked
        """
        try:
            self.__rlock()
        except (ValueError, threading.ThreadError):
            return False
        self.__locked = False
        return True

    def is_locked(self) -> bool:
        """ Check if channel is locked """
        locked = self.lock(False, None)
        if locked:
            self.unlock()
        return locked is False

    def write(self, *args, notify=True):
        """ 
            Write a data to a channel
            which trigger an observation to all observers
        """
        if self.lock() is False:
            self.log_debug("Trying to write in locked channel")
            return False
        ret = self._write(*args)
        self.unlock()
        if ret is not False:
            self.do_timestamp()
            self._last_data = args[0] if len(args) == 1 else args
            if notify:
                self.notify()
            return True
        return False

    #
    # Inheritance
    #

    def task_done(self):
        """ Can be called after channel is read """
        pass

    def _write(self, *args):
        return True

    def read(self):
        """ Return the channel's data """
        return self._last_data

    def clear(self):
        """ Clear channel data """
        self._last_data = None

    def is_readable(self):
        """ Return True when channel can be read """
        return not self.is_locked()

    def is_pollable(self):
        """ Return True if the channel if readable in a loop """
        return False

    def get_data(self):
        """ Get the data object """
        return self._last_data

    #
    # ANamedObject
    #

    def _get_attributes(self):
        l = super()._get_attributes()
        if self.is_block():
            l.append("block")
        timeout = self.get_timeout()
        if timeout is not None and timeout != -1:
            l.append("timeout={}".format(timeout))
        if self.is_pollable():
            l.append("is_pollable")
        if self.is_readable():
            l.append("is_readable")
        if self.is_multiprocess():
            l.append("multiprocessed")
        if self.is_locked():
            l.append("locked")
        return l

    #
    # ALoggable
    #

    def _log_format(self, *msg):
        parent = self.get_parent() or ""
        if parent:
            parent = "{}.".format(parent.get_name())
        return "Channel {0}{1}: {2}".format(parent, self.get_name(), ' '.join(msg))

###############################################################################

class ChannelQueue(Channel):
    """
        Multipurpose queue
    """

    def __init__(self, size=0, name="ChannelQueue", mp=False,
                    from_manager=False, simple=False, **kwargs):
        if mp is True:
            _setup_mp()
            if from_manager is True:
                _setup_mp_manager()
                self.__queue = mp_manager.Queue(maxsize=size)
            elif simple is True:
                self.__queue = multiprocessing.SimpleQueue()
                self._write = self.__simple_write
                self.read = self.__simple_read
            else:
                self.__queue = multiprocessing.Queue(maxsize=size)
            self.clear = self.__clear_get
        else:
            self.__queue = queue.Queue(maxsize=size)
        self.__put = self.__queue.put
        self.__get = self.__queue.get
        self.__empty = self.__queue.empty
        self.__size = size
        super().__init__(mp=mp, name=name, **kwargs)

    def _write(self, data):
        try:
            #self.__put(data, block=self.is_block(), timeout=self.get_timeout())
            self.__put(data, False)
        except queue.Full:
            return False
        return True

    def read(self, timeout=None):
        timeout = timeout or self.get_timeout()
        try:
            #data = self.__get(block=self.is_block(), timeout=timeout)
            data = self.__get(False)
        except queue.Empty:
            data = None
        return data

    def get_size(self):
        return self.__size

    def is_readable(self):
        return super().is_readable() and self.__empty() is False

    def is_pollable(self):
        return True

    def get_data(self):
        return self.__queue

    def __clear_get(self):
        if self.lock():
            q = self.__queue
            empty = q.empty
            get = q.get
            while not empty():
                get()
            self.unlock()
        else:
            self.log_error("Could not clear")

    def clear(self):
        q = self.__queue
        with q.mutex:
            q.queue.clear()

    #
    # Multiprocessing simple
    #

    def __simple_write(self, data):
        self.__put(data)
        return True

    def __simple_read(self):
        #Might get stuck if not careful
        if self.is_readable():
            return self.__get()
        return None

###############################################################################

class PollableChannel(Channel):
    """ Enable any children to be pollable when data is written in channel """

    def __init__(self, poll=True, name="PollableChannel", mp=False, **kwargs):
        self.__poll = bool(poll)
        if poll:
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
        if self.__poll:
            self.__set()
        super().notify()

    def task_done(self):
        """ Clear event """
        if self.__poll:
            self.__clear()

    def is_pollable(self):
        return self.__poll

    def is_readable(self):
        return super().is_readable() and (self.__poll is False or self.__is())

    def wait(self, timeout):
        return self.__event.wait(timeout=timeout)

###############################################################################

class ChannelObject(PollableChannel):
    """
        Object has to be added to BaseManager.register and also ChannelObject's
        class factory.
        Do not support nested objects.
        Can not make use of attributes directly, call methods.

        :Example:
            >> from sihd.Core.Channel import register_channel_object
            >> register_channel_object("MyId", MyClass)
            >> channel = ChannelObject({
                    ident = "MyId",
                    args = [var1, var2],
                    kwargs = {"key": value}
                }, name="MyChannel")
            >> channel.write("x", 420)
            True
            >> channel.read("x")
            420
    """

    __classes = {}

    def __init__(self, ident, obj_args=[], obj_kwargs={},
                    name="ChannelObject", mp=False, **kwargs):
        """
        ident = conf.pop("ident")
        obj_args = conf.pop("args", ())
        obj_kwargs = conf.pop("kwargs", {})
        """
        kwargs.pop('default', None)
        if mp is True:
            _setup_mp_base_manager()
            create_obj = getattr(mp_base_manager, ident)
            self.__obj = create_obj(*obj_args, **obj_kwargs)
        else:
            self.__obj = self.__classes[ident](*obj_args, **obj_kwargs)
        super(ChannelObject, self).__init__(mp=mp, name=name, **kwargs)

    @staticmethod
    def register_class(ident, cls):
        ChannelObject.__classes[ident] = cls

    def _write(self, key, value):
        try:
            fun = getattr(self.__obj, key)
        except AttributeError:
            self.log_error("No such attribute {}".format(key))
            return False
        if callable(fun):
            fun(value)
        else:
            self.log_error("Attribute {} is not a callable".format(key))
        return True

    def read(self, key, *args, **kwargs):
        ret = None
        try:
            fun = getattr(self.__obj, key)
        except AttributeError:
            return ret
        if callable(fun):
            ret = fun(*args, **kwargs)
        else:
            self.log_error("Attribute {} is not a callable".format(key))
        return ret

    def get_data(self):
        return self.__obj

###############################################################################

class ChannelBool(PollableChannel):
    """ Pollable trigger/activation channel """

    def __init__(self, name="ChannelBool", default=False,
                        timeout=0.01, mp=False, **kwargs):
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
        super(ChannelBool, self).__init__(name=name, mp=mp, default=default,
                                            timeout=timeout, **kwargs)

    def _write(self, data):
        if data:
            self.__set()
        else:
            self.__clear()
        return True

    def read(self):
        if self.__wait(self.get_timeout()):
            return True
        return False

###############################################################################

class ChannelString(PollableChannel):
    """ Channel for storing a sized string """

    def __init__(self, size=50, name="ChannelString",
                    unicode=False, mp=False, **kwargs):
        if mp:
            _setup_mp()
            global RawArray
            if RawArray is None:
                from multiprocessing.sharedctypes import RawArray
            type = ctypes.c_wchar if unicode is True else ctypes.c_char
            self.__string = RawArray(type, size)
            self.read = self._read_mp
            self._write = self._write_mp
            self.clear = self._clear_mp
        else:
            self.__string = "" * size
        self.__size = size
        super(ChannelString, self).__init__(mp=mp, name=name, **kwargs)

    def _write(self, data):
        if not isinstance(data, str):
            data = str(data)
        if len(data) >= self.__size:
            self.log_error("String too long ({})".format(data))
            return False
        self.__string = data
        return True

    def get_size(self):
        self.__size = size

    def clear(self):
        self.__string = "" * self.__size

    def read(self):
        return self.__string

    def get_data(self):
        return self.__string

    #
    # Multiprocessing
    #

    def _write_mp(self, data):
        if not isinstance(data, bytes):
            if not isinstance(data, str):
                data = str(data)
            data = data.encode()
        try:
            self.__string.value = data
        except ValueError:
            self.log_error("String too long ({})".format(data.decode()))
            return False
        return True

    def _read_mp(self):
        return self.__string.value.decode()

    def _clear_mp(self):
        self.__string.value = "".encode()

###############################################################################

_typecode_to_type = {
    'c': ctypes.c_char,     'u': ctypes.c_wchar,
    'b': ctypes.c_byte,     'B': ctypes.c_ubyte,
    'h': ctypes.c_short,    'H': ctypes.c_ushort,
    'i': ctypes.c_int,      'I': ctypes.c_uint,
    'l': ctypes.c_long,     'L': ctypes.c_ulong,
    'q': ctypes.c_longlong, 'Q': ctypes.c_ulonglong,
    'f': ctypes.c_float,    'd': ctypes.c_double
}

def get_ctype(c):
    return _typecode_to_type.get(c, None)

class ChannelArray(PollableChannel):
    """ Using fast arrays """

    def __init__(self, ctype, size=10,
                    name="ChannelArray", mp=False, **kwargs):
        # Create a default array
        default = [0] * size
        if mp:
            _setup_mp()
            self.__array = multiprocessing.Array(ctype, default)
            self.get_bytes = self._mp_get_bytes
        else:
            global array
            if array is None:
                import array
            self.__array = array.array(str(ctype), default)
        # Case for bytes
        if ctype in ('b', 'B'):
            self.get_value = self.get_bytes if mp is False else self._mp_get_bytes
        self.__type = ctype
        self.__cast = get_ctype(ctype)
        self.__size = size
        super(ChannelArray, self).__init__(mp=mp, name=name, **kwargs)

    def get_size(self):
        return self.__size

    def _write(self, data, i=None):
        """
            :exemple:
                > channel.get_bytes()
                    b'hello'
                > channel.write(b'world')
                > channel.get_bytes()
                    b'world'
                > channel.write(b'AB', 3)
                > channel.get_bytes()
                    b'worAB'
                > channel.write(b'A', 0)
                > channel.get_bytes()
                    b'AorAB'
        """
        l = self.__array
        if isinstance(data, (bytes, bytearray, list, tuple, set)):
            data_len = len(data)
            total = data_len if i is None else data_len + i
            if i is None:
                i = 0
            if total > self.__size:
                self.log_error("Write size {} too long (>{})"\
                        .format(total, self.__size))
                return False
            for j in range(data_len):
                l[i + j] = data[j]
        elif i < self.__size:
            l[i] = data
        else:
            self.log_error("Index error {} (>{})".format(i, self.__size))
            return False
        return True

    def read(self, n, n2=None):
        """
            :exemple:
                > channel.get_bytes()
                    b'hello'
                > channel.read(0)
                    104
                > channel.read(0, 5)
                    b'hello'
                > channel.read(1, 4)
                    b'ell'
        """
        ret = None
        s = self.__size
        if n is not None:
            if n < s:
                if n2 is not None:
                    if n2 < s:
                        ret = self.get_value()[n:n2]
                    else:
                        self.log_error("Index {} >= size {}".format(n2, s))
                else:
                    ret = self.get_value()[n]
            else:
                self.log_error("Index {} >= size {}".format(n, s))
        else:
            self.log_error("Index is None")
        return ret

    def get_value(self):
        return self.__array

    def _mp_get_bytes(self):
        return bytes(self.__array)

    def get_bytes(self):
        return self.__array.tobytes()

    def get_stripped(self):
        return self.get_bytes().decode().rstrip('\x00')

    def get_data(self):
        return self.__array

    def clear(self):
        l = self.__array
        for i, v in enumerate(l):
            l[i] = 0

###############################################################################

import pickle

class ChannelPickle(ChannelArray):

    def __init__(self, size=200, name="ChannelPickle", **kwargs):
        self.__loads = pickle.loads
        self.__dumps = pickle.dumps
        super().__init__(ctype='B', size=size, name=name, **kwargs)

    def read(self):
        b = self.get_bytes()
        l = self.__loads(b)
        return l

    def _write(self, topickle):
        pickled = self.__dumps(topickle)
        return super()._write(pickled)

###############################################################################

class ChannelCType(PollableChannel):
    """ Pollable ctype value """

    def __init__(self, ctype, name="ChannelCType", mp=False, **kwargs):
        if mp:
            #default = kwargs.pop('default', 0)
            _setup_mp()
            self.__value = multiprocessing.Value(ctype)
            self.__type = ctype
            self.__alock = self.__value.get_lock().acquire
            self.__rlock = self.__value.get_lock().release
            self.read = self._read_mp
            self._write = self._write_mp
        else:
            #Be a simple pollable channel
            self.__value = get_ctype(ctype)()
        super(ChannelCType, self).__init__(mp=mp, name=name, **kwargs)

    def _write(self, data):
        self.__value.value = data

    def read(self):
        return self.__value.value

    def _write_mp(self, data):
        ret = False
        if self.__alock(block=self.is_block(), timeout=self.get_timeout()):
            ret = True
            self.__value.value = data
            self.__rlock()
        return ret

    def _read_mp(self):
        if self.__alock(block=self.is_block(), timeout=self.get_timeout()):
            ret = self.__value.value
            self.__rlock()
            return ret
        return None

    def get_data(self):
        return self.__value

    def clear(self):
        self._write(0)

###############################################################################

class ChannelByte(ChannelCType):

    def __init__(self, name="ChannelByte", unsigned=False, **kwargs):
        t = 'b' if unsigned is False else 'B'
        super(ChannelByte, self).__init__(name=name, ctype=t, **kwargs)

class ChannelChar(ChannelCType):

    def __init__(self, name="ChannelChar", unicode=False, **kwargs):
        self.__unicode = unicode
        t = 'c' if unicode is False else 'u'
        super(ChannelChar, self).__init__(name=name, ctype=t, **kwargs)

    def is_unicode(self):
        return self.__unicode

    def _write(self, data):
        if isinstance(data, str) and len(data) == 1:
            data = data.encode()
        return super()._write(data)

    def read(self):
        return super().read().decode()

class ChannelShort(ChannelCType):

    def __init__(self, name="ChannelShort", unsigned=False, **kwargs):
        t = 'h' if unsigned is False else 'H'
        super(ChannelShort, self).__init__(name=name, ctype=t, **kwargs)

class ChannelInt(ChannelCType):

    def __init__(self, name="ChannelInt", unsigned=False, **kwargs):
        t = 'i' if unsigned is False else 'I'
        super(ChannelInt, self).__init__(name=name, ctype=t, **kwargs)

class ChannelLong(ChannelCType):

    def __init__(self, name="ChannelLong", unsigned=False, **kwargs):
        t = 'l' if unsigned is False else 'L'
        super(ChannelLong, self).__init__(name=name, ctype=t, **kwargs)

class ChannelDouble(ChannelCType):

    def __init__(self, name="ChannelDouble", float=False, **kwargs):
        t = 'd' if float is False else 'f'
        super(ChannelDouble, self).__init__(name=name, ctype=t, **kwargs)

###############################################################################

class ChannelDict(PollableChannel):
    """
        Use with caution in multiprocessing context as a Manager
        is quite costly in resources. It opens up a server and another
        process. Convenient but maybe prefer a design with queues if
        possible.
    """

    def __init__(self, name="ChannelDict", mp=False, **kwargs):
        if mp is True:
            _setup_mp_manager()
            self.__dict = mp_manager.dict()
            self._write = self.__write_mp
        else:
            self.__dict = dict()
        super(ChannelDict, self).__init__(mp=mp, name=name, **kwargs)

    def _write(self, key, value=None):
        if isinstance(key, dict):
            self.__dict.update(key)
        else:
            self.__dict[key] = value
        return True

    def __write_mp(self, key, value=None):
        try:
            if isinstance(key, dict):
                self.__dict.update(key)
            else:
                self.__dict[key] = value
        except (RuntimeError, TypeError) as e:
            self.log_error(e)
            return False
        return True

    def read(self, key, ret=None):
        return self.__dict.get(key, ret)

    def get_data(self):
        return self.__dict

    def clear(self, key=None):
        d = self.__dict
        if key is None:
            d.clear()
        else:
            del d[key]

###############################################################################

class ChannelList(PollableChannel):
    """
        Use with caution in multiprocessing context as a Manager
        is quite costly in resources. It opens up a server and another
        process. Convenient when having a dynamic list but prefer arrays
        as they are fixed and 'easily' accessible in shared memory.
    """

    def __init__(self, name="ChannelList", mp=False, **kwargs):
        if mp is True:
            _setup_mp_manager()
            self.__list = mp_manager.list()
        else:
            self.__list = list()
        self.__append = self.__list.append
        super(ChannelList, self).__init__(mp=mp, name=name, **kwargs)

    def _write(self, value, i=None):
        l = self.__list
        if isinstance(value, (list, tuple, set)):
            for v in value:
                self.__append(v)
        elif i is None:
            self.__append(value)
        elif i < len(l):
            l[i] = value
        else:
            return False
        return True

    def read(self, i):
        l = self.__list
        s = len(l)
        if s > 0 and i < s:
            return l[i]
        return None

    def get_data(self):
        return self.__list

    def clear(self):
        self.__list[:] = []

###############################################################################

class ChannelCondition(Channel):

    def __init__(self, name="ChannelCondition", lock=None, mp=False, **kwargs):
        if mp is True:
            _setup_mp()
            self.__condition = multiprocessing.Condition(lock=lock)
        else:
            self.__condition = threading.Condition(lock=lock)
        super(ChannelCondition, self).__init__(name=name, mp=mp, **kwargs)

    def _write(self, n=0):
        cd = self.__condition
        with cd:
            if n == 0:
                cd.notify_all()
            else:
                cd.notify(n)
            return True
        return False

    def read(self, timeout=None):
        cd = self.__condition
        with cd:
            ret = cd.wait(timeout=self.get_timeout())
        return ret
