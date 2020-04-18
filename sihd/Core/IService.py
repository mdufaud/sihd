#!/usr/bin/python
#coding: utf-8
""" System """
import time

from .ILoggable import ILoggable
from .IConfigurable import IConfigurable
from .IObserver import IObserver
from .IDumpable import IDumpable
from .Channel import Channel, ChannelQueue, ChannelDict, ChannelList, \
                        ChannelCondition, ChannelBool

class IService(ILoggable, IConfigurable, IObserver, IDumpable):

    def __init__(self, name="IService"):
        super(IService, self).__init__(name)
        self.__stopped = True
        self.__paused = False
        self._state_observers = set()
        self.__channels = dict()
        self.__channels_input = list()
        self.__channels_output = list()
        self.__todo_ichan = list()
        self.__todo_ochan = list()
        self.__start_time = None
        self.__stop_time = None

    """ IDumpable """

    def on_dump(self) -> dict:
        if self.is_active():
            raise RuntimeError("Cannot dump a running service")
        dic = super().on_dump()
        return dic

    def on_load(self, dic: dict):
        if self.is_active():
            raise RuntimeError("Cannot load a running service")
        super().on_load(dic)

    """ IObserver """

    def handle(self, channel):
        """ Handle service's input """
        pass

    def _pre_handle(self, channel) -> bool:
        """ Used for services that want to handle stuff before user parsing """
        return False

    def on_notify(self, channel):
        if self.is_active():
            if not self._pre_handle(channel):
                self.handle(channel)

    """ Channel creation """

    def create_channel_condition(self, name, **kwargs):
        return ChannelCondition(name=name, **kwargs)

    def create_channel_bool(self, name, **kwargs):
        return ChannelBool(name=name, **kwargs)

    def create_channel_list(self, name, **kwargs):
        return ChannelList(name=name, **kwargs)

    def create_channel_dict(self, name, **kwargs):
        return ChannelDict(name=name, **kwargs)

    def create_channel_queue(self, name, **kwargs):
        return ChannelQueue(name=name, **kwargs)

    def create_channel_int(self, name, **kwargs):
        return Channel(name=name, **kwargs)

    def create_channel_double(self, name, **kwargs):
        return Channel(name=name, **kwargs)

    def create_channel_default(self, name, **kwargs):
        return Channel(name=name, **kwargs)

    def create_channel(self, name, **kwargs):
        """
            Gets a create_channel_TYPE from service methods
            and make a channel from it. TYPE is contained as a key argument in
            'type', if not, make a default channel.

            :param name: str channel name
            :param type: str channel type
            :param block: bool permits blocking on certain locks in channels
                            on read/write before returning result
            :param timeout: float when blocking is enabled, set the timeout before
                            returning
            :param poll: bool enable polling from channel when a write is done on
                                the channel in any thread/process
            :param mp: bool change in some channels internal variable from
                            threading to multiprocessing or Manager based

            :param var_type: char in some channel you have to set the variable
                                type to be used in the internal variable
            :return: A Channel if the creation method was found else None
            :rtype: Channel
        """
        type = kwargs.pop('type', None)
        if type is None:
            type = "default"
        try:
            method = getattr(self, "create_channel_{}".format(type))
        except AttributeError:
            self.log_error("No such type for channel {}".format(type))
            return None
        kwargs['parent'] = self
        channel = method(name, **kwargs)
        if channel:
            self.log_debug("--> {}".format(channel))
        return channel

    """ Channels Input/Output """

    """     Reading """

    def read_channels_input(self) -> bool:
        for channel in self.get_channels_input():
            if channel.is_pollable() and channel.is_readable():
                channel.notify()
                channel.task_done()
        return True

    """     Locking """

    def __lock_channels(self, channels):
        for channel in channels:
            if not channel.is_locked():
                channel.lock()

    def __unlock_channels(self, channels):
        for channel in channels:
            if not channel.is_locked():
                channel.lock()

    def lock_channels_output(self):
        self.__lock_channels(self.get_channels_output())

    def unlock_channels_output(self):
        self.__lock_channels(self.get_channels_output())

    def lock_channels_input(self):
        self.__lock_channels(self.get_channels_input())

    def unlock_channels_input(self):
        self.__lock_channels(self.get_channels_input())

    def clear_channels(self):
        for channel in self.get_channels_input():
            name = channel.get_name()
            attr = getattr(self, name)
            attr = None
        self.__channels_input = []
        for channel in self.get_channels_output():
            name = channel.get_name()
            attr = getattr(self, name)
            attr = None
        self.__channels_output = []

    """     Making """

    def _make_channels(self):
        for name, dic in self.__todo_ichan:
            self.create_input_channel(name, **dic)
        self.__todo_ichan = []
        for name, dic in self.__todo_ochan:
            self.create_output_channel(name, **dic)
        self.__todo_ochan = []
        return True

    def add_channel_input(self, name, **kwargs):
        self.__todo_ichan.append((name, kwargs))

    def add_channel_output(self, name, **kwargs):
        self.__todo_ochan.append((name, kwargs))

    def create_input_channel(self, name, **kwargs):
        channel = self.create_channel(name, **kwargs)
        if channel:
            self.set_channel_input(channel)
        return channel

    def create_output_channel(self, name, **kwargs):
        channel = self.create_channel(name, **kwargs)
        if channel:
            self.set_channel_output(channel)
        return channel

    """     Getter/Setter """

    def get_channel(self, name):
        try:
            channel = getattr(self, name)
        except AttributeError:
            channel = None
        if channel and not isinstance(channel, Channel):
            channel = None
        return channel

    def get_channels_input(self):
        return self.__channels_input

    def get_channels_output(self):
        return self.__channels_output

    def set_channel_input(self, channel, name=None):
        if name is None:
            name = channel.get_name()
        input_lst = self.__channels_input
        if any((name == c.get_name() for c in input_lst)):
            self.log_warning("Channel {} already exist".format(name))
        else:
            input_lst.append(channel)
            setattr(self, name, channel)
            channel.add_observer(self)
        return True

    def set_channel_output(self, channel, name=None):
        if name is None:
            name = channel.get_name()
        output_lst = self.__channels_output
        if any((name == c.get_name() for c in output_lst)):
            self.log_warning("Channel {} already exist".format(name))
        else:
            output_lst.append(channel)
            setattr(self, name, channel)
        return True

    """ IConfigurable """

    def _setup_impl(self):
        if super()._setup_impl():
            return self.do_setup() and self._make_channels() and self.on_setup()
        return False

    def do_setup(self):
        return True

    def on_setup(self):
        return True

    """ Life cycle """

    def is_paused(self):
        return self.__paused

    def is_running(self):
        return (not self.__stopped)

    def is_active(self):
        return (not self.__paused and not self.__stopped)

    def get_service_state(self):
        if self.is_running():
            s = "Running"
        else:
            s = "Stopped"
        if self.__paused:
            s += " (paused)"
        return s

    """ Start """

    def get_service_start_time(self):
        return self.__start_time

    def start(self, force=False):
        if self.is_configured() is False and self.setup() is False:
            return False
        if self.__stopped is False:
            self.log_debug("Starting an already started service")
            if force is False:
                return False
        self.__start_time = time.time()
        if self._start_impl() is True:
            self.__stopped = False
            self.__notify_state_change()
            self.on_start()
        if self.__stopped is True:
            self.__start_time = None
        self.log_info("%s" %
                ("is started" if not self.__stopped else "did not start"))
        return self.is_running()

    def _start_impl(self):
        return True

    def on_start(self):
        return True

    """ Stop """

    def get_service_stop_time(self):
        return self.__stop_time

    def stop(self, force=False):
        if self.__stopped is True:
            self.log_debug("Stopping an already stopped service")
            if force is False:
                return True
        self.__stop_time = time.time()
        if self._stop_impl() is True:
            self.__stopped = True
            self.__notify_state_change()
            self.on_stop()
            self._set_unconfigured()
        if self.__stopped is False:
            self.__stop_time = None
        self.log_debug("%s" %
                ("is stopped" if self.__stopped else "did not stop"))
        return self.is_running() == False

    def _stop_impl(self):
        return True

    def on_stop(self):
        return True

    """ Pause """

    def pause(self, force=False):
        if self.__paused is True:
            self.log_debug("Pausing an already paused service")
            if force is False:
                return True
        if self._pause_impl() is True:
            self.__paused = True
            self.__notify_state_change()
            self.on_pause()
        self.log_debug("%s" %
            ("is paused" if self.__paused else "did not pause"))
        return self.is_paused()

    def _pause_impl(self):
        return True

    def on_pause(self):
        return True

    """ Resume """

    def resume(self, force=False):
        if self.__paused is False:
            self.log_debug("Resuming a non paused service")
            if force is False:
                return True
        if self._resume_impl() is True:
            self.__paused = False
            self.__notify_state_change()
            self.on_resume()
        self.log_debug("%s" %
                ("is resumed" if not self.__paused else "did not resume"))
        return self.is_paused() == False

    def _resume_impl(self):
        return True

    def on_resume(self):
        return True

    """ Reset """

    def reset(self):
        if self._reset_impl() is True:
            self.clear_channels()
            self.on_reset()
            self.__stopped = True
            self.__paused = False
            self.__notify_state_change()
            self._set_unconfigured()
        return self.__stopped and not self.__paused

    def _reset_impl(self):
        self.log_debug("Called a non implemented function: reset")
        return False

    def on_reset(self):
        return True

    """ State Observation """

    def add_state_observer(self, obs):
        self._state_observers.add(obs)

    def __notify_state_change(self):
        for obs in self._state_observers:
            obs.service_state_changed(self, self.__stopped, self.__paused)

    """ Careful """

    def _set_stopped(self, stopped):
        """ Do not call if you do not know what you are doing """
        self.__stopped = stopped

    def _set_paused(self, paused):
        """ Do not call if you do not know what you are doing """
        self.__paused = paused
