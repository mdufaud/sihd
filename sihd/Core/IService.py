#!/usr/bin/python
#coding: utf-8

from .ILoggable import ILoggable
from .IConfigurable import IConfigurable
from .IObserver import IObserver
from .Channel import Channel, ChannelQueue, ChannelValue, ChannelArray, ChannelPipe

multiprocessing = None

class IService(ILoggable, IConfigurable, IObserver):

    def __init__(self, name="IService"):
        super(IService, self).__init__(name)
        self.__stopped = True
        self.__paused = False
        self._state_observers = set()
        self.__channels = dict()
        self.__channels_input = dict()
        self.__channels_output = dict()
        self.__channels_creation = {
            "queue": self.create_channel_queue,
            "pipe": self.create_channel_pipe,
            "value": self.create_channel_value,
            "array": self.create_channel_array,
        }

    def _set_stopped(self, stopped):
        self.__stopped = stopped

    def _set_paused(self, paused):
        self.__paused = paused

    """ Channels """

    def get_channels_input(self):
        return self.__channels_input

    def get_channels_output(self):
        return self.__channels_output

    def set_channel_input(self, channel):
        name = channel.get_name()
        if self.__channels_input.get(name, None) is None:
            self.__channels_input[name] = channel
            setattr(self, name, channel)
            channel.add_observer(self)
            return True
        else:
            self.log_error("Channel {} already exists".format(c.get_name()))
        return False

    def set_channel_output(self, channel):
        name = channel.get_name()
        if self.__channels_output.get(name, None) is None:
            self.__channels_output[name] = channel
            setattr(self, name, channel)
            return True
        else:
            self.log_error("Channel {} already exists".format(c.get_name()))
        return False

    def create_channel_pipe(self, name, **kwargs):
        """ Args: block=True/False ; timeout=float """
        global multiprocessing
        if multiprocessing is None:
            import multiprocessing
        parent, child = multiprocessing.Pipe()
        parent_chan = ChannelPipe(name=name + "_parent",
                                    parent=self,
                                    child=False, pipe=parent, **kwargs)
        child_chan = ChannelPipe(name=name + "_child",
                                    parent=self,
                                    child=True, pipe=child, **kwargs)
        return parent_chan, child_chan

    def create_channel_array(self, name, **kwargs):
        """ Args: type='i'/'d' ; default=value """
        return ChannelArray(name=name, **kwargs)

    def create_channel_value(self, name, **kwargs):
        """ Args: type='i'/'d' ; default=value """
        return ChannelValue(name=name, **kwargs)

    def create_channel_queue(self, name, **kwargs):
        """ Args: block=True/False ; timeout=float """
        return ChannelQueue(name=name, **kwargs)

    def create_channel_default(self, name, **kwargs):
        """ Args: block=True/False ; timeout=float """
        return Channel(name=name, **kwargs)

    def create_channel(self, name, **kwargs):
        """ Args: block=True/False ; timeout=float """
        return self.__create_channel_type(name, **kwargs)

    def create_input(self, name, **kwargs):
        channel = self.create_channel(name, **kwargs)
        if channel:
            self.set_channel_input(channel)
        return channel

    def create_output(self, name, **kwargs):
        channel = self.create_channel(name, **kwargs)
        if channel:
            self.set_channel_output(channel)
        return channel

    def __create_channel_type(self, *args, **kwargs):
        type = kwargs.get('type', None)
        if type is not None:
            method = self.__channels_creation.get(type, None)
            if method is None:
                self.log_error("No such type for channel {}".format(type))
                return None
            del kwargs['type']
        else:
            method = self.create_channel_default
        kwargs['parent'] = self
        return method(*args, **kwargs)

    def read_channels_input(self):
        for name, channel in self.get_channels_input().items():
            if channel.pollable() and channel.readable():
                channel.notify()
        return True

    """ IConfigurable """

    def _setup_impl(self):
        if super()._setup_impl():
            return self.do_setup() and self.do_channels()
        return False

    def do_setup(self):
        return True

    def do_channels(self):
        return True

    """ Life cycle """

    def is_paused(self):
        return self.__paused

    def is_running(self):
        return (not self.__stopped)

    def is_active(self):
        return (not self.__paused and not self.__stopped)

    def get_service_state(self):
        s = ""
        if self.__stopped:
            s += "Stopped"
        else:
            s += "Running"
        if self.__paused:
            s += " (paused)"
        return s

    """ Start """

    def start(self, force=False):
        if self.__stopped is False:
            self.log_debug("Starting an already started service")
            if force is False:
                return False
        if self._start_impl() is True:
            self.__stopped = False
            self.__notify_change()
        self.log_debug("%s" %
                ("is started" if not self.__stopped else "did not start"))
        return self.is_running()

    def _start_impl(self):
        return True

    """ Stop """

    def stop(self, force=False):
        if self.__stopped is True:
            self.log_debug("Stopping an already stopped service")
            if force is False:
                return True
        if self._stop_impl() is True:
            self.__stopped = True
            self.__notify_change()
        self.log_debug("%s" %
                ("is stopped" if self.__stopped else "did not stop"))
        return self.is_running() == False

    def _stop_impl(self):
        return True

    """ Pause """

    def pause(self, force=False):
        if self.__paused is True:
            self.log_debug("Pausing an already paused service")
            if force is False:
                return True
        if self._pause_impl() is True:
            self.__paused = True
            self.__notify_change()
        self.log_debug("%s" %
            ("is paused" if self.__paused else "did not pause"))
        return self.is_paused()

    def _pause_impl(self):
        return True

    """ Resume """

    def resume(self, force=False):
        if self.__paused is False:
            self.log_debug("Resuming a non paused service")
            if force is False:
                return True
        if self._resume_impl() is True:
            self.__paused = False
            self.__notify_change()
        self.log_debug("%s" %
                ("is resumed" if not self.__paused else "did not resume"))
        return self.is_paused() == False

    def _resume_impl(self):
        return True

    """ Reset """

    def reset(self):
        if self._reset_impl() is True:
            self.__stopped = True
            self.__paused = False
            self.__notify_change()
        return self.__stopped and not self.__paused

    def _reset_impl(self):
        self.log_debug("Called a non implemented function: reset")
        return False

    """ State Observation """

    def add_state_observer(self, obs):
        self._state_observers.add(obs)

    def __notify_change(self):
        for obs in self._state_observers:
            obs.service_state_changed(self, self.__stopped, self.__paused)
