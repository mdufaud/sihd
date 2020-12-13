#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import queue
import threading
import ctypes

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
        BaseManager.register('LifoQueue', queue.LifoQueue)
        BaseManager.register('PriorityQueue', queue.PriorityQueue)
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


###############################################################################

class Channel(AObservable, IObserver, ALoggable):

    def __init__(self, name="Channel", mp=False, parent=None,
                    block=True, timeout=10, log=True,
                    default=None, timestamp=False, pollable=True,
                    lfilter=None, time_method=time.time):
        super().__init__(name, parent=parent)
        self.args = None
        self.write_args = None
        self.read_args = 0
        self.type = None
        self.now = time_method
        if lfilter and not callable(lfilter):
            raise RuntimeError(str(self) + ": Filter is not a callable")
        self.filter = lfilter
        #Timestamping and lock
        self.__ts = False
        if log is False:
            self.set_log_active(False)
        if mp is True:
            _setup_mp()
        self.pollable = pollable
        if pollable:
            if mp is True:
                self.__event = multiprocessing.Event()
            else:
                self.__event = threading.Event()
            self.consumed_data = self.__event.clear
            self.set_new_data = self.__event.set
            self.is_new_data = self.__event.is_set
            self.wait = self.__event.wait
        if mp is True:
            # Multiprocessing
            self.__last_ts = multiprocessing.Value('i', 0)
            self.do_timestamp = self.__do_timestamp_mp
            self.get_timestamp = self.__get_timestamp_mp
            self.__lock = multiprocessing.Lock()
            self.lock = self.__lock_mp
        else:
            # Threading
            self.__last_ts = 0
            self.__lock = threading.Lock()
        self.__alock = self.__lock.acquire
        self.__rlock = self.__lock.release
        self.__locked = False
        #Stored last data
        self._last_data = None
        #Multiprocessing
        self.__mp = mp
        self.timeout = timeout
        self.block = block
        self.set_timestamp(timestamp)
        self.default_value = default
        if default is not None:
            self.write(default)
            self.consumed_data()

    def is_multiprocess(self):
        return self.__mp

    #
    # Timestamp
    #

    def set_timestamp(self, active):
        """ Activate timestamping """
        self.__ts = active

    def do_timestamp(self):
        if self.__ts:
            self.__last_ts = self.now()

    def __do_timestamp_mp(self):
        if self.__ts:
            self.__last_ts.value = self.now()

    def get_timestamp(self):
        return self.__last_ts

    def __get_timestamp_mp(self):
        return self.__last_ts.value

    #
    # Observer/Observable
    #

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
            data = None
            if channel.lock(0.01):
                try:
                    #Some channels need keys to read
                    data = channel.read()
                except TypeError:
                    data = None
                channel.unlock()
            if data is not None:
                self.write(data)
            else:
                #Some channels do not need keys to write
                self.write()

    #
    # Lock
    #

    def get_lock(self):
        return self.__lock

    def __lock_mp(self, timeout=None) -> bool:
        """
            Lock read/write for a channel in multiprocess
            :return: True if channel is locked
        """
        timeout = self.timeout if timeout is None else timeout
        block = timeout is not None
        ret = self.__alock(block=block, timeout=timeout)
        self.__locked = ret
        return ret

    def lock(self, timeout=None) -> bool:
        """
            Lock read/write for a channel - For thread timeout None is -1
            :return: True if channel is locked
        """
        to = self.timeout if timeout is None else timeout
        to = -1 if to is None else to
        block = to != -1
        ret = self.__alock(blocking=block, timeout=to)
        self.__locked = ret
        return ret

    def unlock(self) -> bool:
        """
            Unlock read/write for a channel
            :return: True if channel was locked
        """
        try:
            self.__rlock()
        except (RuntimeError, ValueError, threading.ThreadError):
            return False
        self.__locked = False
        return True

    def is_locked(self) -> bool:
        """ Check if channel is locked """
        locked = self.lock(None)
        if locked:
            self.unlock()
        return locked is False

    #
    # Writing method
    #

    def write(self, *args, notify=True):
        """
            Write a data to a channel
            which trigger an observation to all observers
        """
        lfilter = self.filter
        if lfilter and lfilter(*args) is False:
            return True
        if self.lock() is False:
            self.log_warning("Trying to write in locked channel")
            return False
        ret = self._write(*args)
        self.unlock()
        if ret is False:
            self.log_warning("Could not write into channel")
        if ret is not False:
            self.set_new_data()
            self.do_timestamp()
            if notify:
                self.notify()
        return ret

    def _write(self, val):
        self._last_data = val
        return True

    #
    # Polling methods
    #

    def consumed_data(self):
        pass

    def set_new_data(self):
        pass

    def is_new_data(self):
        return False

    #
    # Read method
    #

    def read(self):
        """ Return the channel's data """
        return self._last_data

    def is_readable(self):
        return self.pollable is False or self.is_new_data()

    def get_data(self):
        """ Get the data object """
        return self._last_data

    #
    # Utility
    #

    def clear(self):
        """ Clear channel data """
        self._last_data = None

    def wait(self, timeout=None):
        """ Wait on a new value """
        return False

    def get_size(self):
        try:
            ret = len(self._last_data)
        except TypeError:
            ret = 1
        return ret

    #
    # ANamedObject
    #

    def _get_attributes(self):
        l = super()._get_attributes()
        if self.block:
            l.append("blocked")
        timeout = self.timeout
        if timeout is not None and timeout != -1:
            l.append("timeout=" + str(timeout))
        if not self.pollable:
            l.append("not pollable")
        if self.is_readable():
            l.append("readable")
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
        return "Channel {0}{1}: {2}"\
                .format(parent, self.get_name(),
                        ' '.join((str(m) for m in msg)))

###############################################################################

class ChannelQueue(Channel):
    """
        Multipurpose queue
    """

    def __init__(self, size=0, name="ChannelQueue", mp=False,
                    lifo=False, simple=False, priority=False, **kwargs):
        if mp is True:
            #Multiprocessing
            _setup_mp()
            if lifo:
                _setup_mp_base_manager()
                self.__queue = mp_base_manager.LifoQueue(maxsize=size)
            elif priority:
                _setup_mp_base_manager()
                self.__queue = mp_base_manager.PriorityQueue(maxsize=size)
            elif simple:
                self.__queue = multiprocessing.SimpleQueue()
                self._write = self.__simple_write
                self.read = self.__simple_read
            else:
                self.__queue = multiprocessing.Queue(maxsize=size)
            self.clear = self.__clear_get
        else:
            #Threading
            if lifo:
                self.__queue = queue.LifoQueue(maxsize=size)
            elif priority:
                self.__queue = queue.PriorityQueue(maxsize=size)
            else:
                self.__queue = queue.Queue(maxsize=size)
        self.__put = self.__queue.put
        self.__get = self.__queue.get
        self.__empty = self.__queue.empty
        self.__size = size
        kwargs['pollable'] = True
        super().__init__(mp=mp, name=name, **kwargs)
        self.write_args = 1
        self.read_args = 0
        self.wait = self._qwait

    #
    # Override
    #

    def _write(self, data):
        try:
            self.__put(data, False)
        except queue.Full:
            return False
        return True

    def read(self, timeout=None):
        timeout = timeout
        block = timeout is not None
        try:
            data = self.__get(block=block, timeout=timeout)
        except queue.Empty:
            data = None
        return data

    def is_readable(self):
        return not self.__empty()

    def get_data(self):
        return self.__queue

    def get_size(self):
        return self.__queue.qsize()

    def _qwait(self, timeout=None):
        if not self.__empty():
            return True
        return super().wait(timeout)

    #
    # Queue only
    #

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

class ChannelObject(Channel):
    """
        Object has to be added to BaseManager.register and also ChannelObject's
        class factory.
        Do not support nested objects.
        Can not make use of attributes directly, call methods.

        :Example:
            >> from sihd.core.Channel import register_channel_object
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
        super().__init__(mp=mp, name=name, **kwargs)
        self.type = object
        self.write_args = 2
        self.read_args = 1

    #
    # Channel Object only
    #

    @staticmethod
    def register_class(ident, cls):
        ChannelObject.__classes[ident] = cls

    #
    # Override
    #

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

    def get_size(self):
        return 1

###############################################################################

class ChannelBool(Channel):
    """ Pollable trigger/activation channel """

    def __init__(self, name="ChannelBool", default=False,
                    mp=False, **kwargs):
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
        super().__init__(name=name, mp=mp, default=default, **kwargs)
        self.write_args = 1
        self.type = bool

    def get_size(self):
        return 1

    def _write(self, flag):
        if flag:
            self.__set()
        else:
            self.__clear()
        return True

    def read(self, timeout=None):
        timeout = timeout or 0.001
        if self.__wait(timeout):
            return True
        return False

###############################################################################

class ChannelString(Channel):
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
        super().__init__(mp=mp, name=name, **kwargs)
        self.write_args = 1
        self.type = str

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

class ChannelArray(Channel):
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
        super().__init__(mp=mp, name=name, **kwargs)
        self.write_args = 2
        self.read_args = 2
        self.type = list

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

class ChannelBytes(ChannelArray):

    def __init__(self, size=200, name="ChannelBytes", **kwargs):
        self.__loads = pickle.loads
        self.__dumps = pickle.dumps
        super().__init__(ctype='B', size=size, name=name, **kwargs)
        self.write_args = 1
        self.read_args = 0
        self.type = bytes

    def read(self):
        b = self.get_bytes()
        l = self.__loads(b)
        return l

    def _write(self, topickle):
        pickled = self.__dumps(topickle)
        return super()._write(pickled)

###############################################################################

class ChannelCType(Channel):
    """ Pollable ctype value """

    def __init__(self, ctype, name="ChannelCType", mp=False, **kwargs):
        if mp:
            _setup_mp()
            self.__value = multiprocessing.Value(ctype)
            self.__type = ctype
            self.__alock = self.__value.get_lock().acquire
            self.__rlock = self.__value.get_lock().release
            self.read = self._read_mp
            self._write = self._write_mp
        else:
            self.__value = get_ctype(ctype)()
        super().__init__(mp=mp, name=name, **kwargs)
        self.write_args = 1
        self.type = int

    def get_size(self):
        return 1

    def _write(self, data):
        self.__value.value = data
        return True

    def read(self):
        return self.__value.value

    def _write_mp(self, data):
        ret = False
        if self.__alock(block=self.block, timeout=self.timeout):
            ret = True
            self.__value.value = data
            self.__rlock()
        return ret

    def _read_mp(self):
        if self.__alock(block=self.block, timeout=self.timeout):
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
        super().__init__(name=name, ctype=t, **kwargs)

class ChannelChar(ChannelCType):

    def __init__(self, name="ChannelChar", unicode=False, **kwargs):
        self.__unicode = unicode
        t = 'c' if unicode is False else 'u'
        super().__init__(name=name, ctype=t, **kwargs)

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
        super().__init__(name=name, ctype=t, **kwargs)

class ChannelInt(ChannelCType):

    def __init__(self, name="ChannelInt", unsigned=False, **kwargs):
        t = 'i' if unsigned is False else 'I'
        super().__init__(name=name, ctype=t, **kwargs)

class ChannelLong(ChannelCType):

    def __init__(self, name="ChannelLong", unsigned=False, **kwargs):
        t = 'l' if unsigned is False else 'L'
        super().__init__(name=name, ctype=t, **kwargs)

class ChannelDouble(ChannelCType):

    def __init__(self, name="ChannelDouble", type_float=False, **kwargs):
        t = 'd' if type_float is False else 'f'
        super().__init__(name=name, ctype=t, **kwargs)
        self.type = float

###############################################################################

class ChannelCounter(ChannelInt):

    def __init__(self, name="ChannelCounter",
            unsigned=False, default=None, **kwargs):
        super().__init__(name=name, unsigned=False, default=default, **kwargs)
        if default:
            super()._write(default)
        self.write_args = 0

    def _write(self, *args):
        return super()._write(self.read() + 1)

    def _write_mp(self, *args):
        return super()._write_mp(self.read() + 1)

    def clear(self):
        super()._write(self.default_value or 0)

###############################################################################

class ChannelDict(Channel):
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
        super().__init__(mp=mp, name=name, **kwargs)
        self.write_args = 2
        self.read_args = 2
        self.type = dict

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

class ChannelList(Channel):
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
        super().__init__(mp=mp, name=name, **kwargs)
        self.type = list
        self.write_args = 2
        self.read_args = 1

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

    def get_size(self):
        return len(self.__list)

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
        kwargs['pollable'] = False
        super().__init__(name=name, mp=mp, **kwargs)
        self.type = bool
        self.write_args = 1
        self.read_args = 0

    def get_size(self):
        return 1

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
            ret = cd.wait(timeout=self.timeout)
        return ret

###############################################################################

class ChannelFunction(Channel):

    def __init__(self, name="ChannelFunction", read=None, write=None, **kwargs):
        super().__init__(name=name, **kwargs)
        self.type = object
        self.write_args = None
        self.read_args = None
        if read is not None and not callable(read):
            raise RuntimeError("Read function not callable")
        if write is not None and not callable(write):
            raise RuntimeError("Write function not callable")
        self.read_method = read
        self.write_method = write

    def get_size(self):
        return 0

    def get_data(self):
        return None

    def is_readable(self):
        return self.read_method is not None and super().is_readable()

    def _write(self, *args, **kwargs):
        f = self.write_method
        if f is not None:
            return f(*args, **kwargs)
        return False

    def read(self, *args, **kwargs):
        f = self.read_method
        if f is not None:
            return f(*args, **kwargs)
        return None

###############################################################################

channel_factory = {
    'counter': ChannelCounter,
    'condition': ChannelCondition,
    'bool': ChannelBool,
    'list': ChannelList,
    'dict': ChannelDict,
    'array': ChannelArray,
    'object': ChannelObject,
    'queue': ChannelQueue,
    'string': ChannelString,
    'bytes': ChannelBytes,
    'byte': ChannelByte,
    'char': ChannelChar,
    'short': ChannelShort,
    'int': ChannelInt,
    'long': ChannelLong,
    'double': ChannelDouble,
    'function': ChannelFunction,
    'default': Channel
}
