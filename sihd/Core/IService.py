#!/usr/bin/python
#coding: utf-8
""" System """
import time

from .ILoggable import ILoggable
from .IConfigurable import IConfigurable
from .IObserver import IObserver
from .Channel import Channel, ChannelQueue, ChannelDict, ChannelList, \
                        ChannelCondition, ChannelEvent

class IService(ILoggable, IConfigurable, IObserver):

    def __init__(self, name="IService"):
        super(IService, self).__init__(name)
        self.__stopped = True
        self.__paused = False
        self._state_observers = set()
        self.__channels = dict()
        self.__channels_input = dict()
        self.__channels_output = dict()
        self.__todo_ichan = list()
        self.__todo_ochan = list()
        self.__start_time = None
        self.__stop_time = None

    """ Channel creation """

    def create_channel_condition(self, name, **kwargs):
        return ChannelCondition(name=name, **kwargs)

    def create_channel_event(self, name, **kwargs):
        return ChannelEvent(name=name, **kwargs)

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
        """ Args:
                type = channel_type
                block = bool
                timeout = float
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
        return method(name, **kwargs)

    """ Channels Input/Output """

    def read_channels_input(self):
        for name, channel in self.get_channels_input().items():
            if channel.pollable() and channel.readable():
                channel.notify()
                channel.clear()
        return True

    def _make_channels(self):
        for name, dic in self.__todo_ichan:
            self.create_input_channel(name, **dic)
        for name, dic in self.__todo_ochan:
            self.create_output_channel(name, **dic)
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

    def get_channels_input(self):
        return self.__channels_input

    def get_channels_output(self):
        return self.__channels_output

    def set_channel_input(self, channel, name=None):
        if name is None:
            name = channel.get_name()
        if self.__channels_input.get(name, None) is not None:
            self.log_debug("Channel {} already exists".format(channel.get_name()))
        self.__channels_input[name] = channel
        setattr(self, name, channel)
        channel.add_observer(self)
        return True

    def set_channel_output(self, channel):
        name = channel.get_name()
        if self.__channels_output.get(name, None) is not None:
            self.log_debug("Channel {} already exists".format(channel.get_name()))
        self.__channels_output[name] = channel
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
            if self.on_start() is not False:
                self.__stopped = False
                self.__notify_state_change()
        if self.__stopped is True:
            self.__start_time = None
        self.log_debug("%s" %
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
            if self.on_stop() is not False:
                self.__stopped = True
                self.__notify_state_change()
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
            if self.on_pause() is not False:
                self.__paused = True
                self.__notify_state_change()
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
            if self.on_resume() is not False:
                self.__paused = False
                self.__notify_state_change()
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
            if self.on_reset() is not False:
                self.__stopped = True
                self.__paused = False
                self.__notify_state_change()
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
