#!/usr/bin/python
#coding: utf-8
#
# System
#
import time

from .ALoggable import ALoggable
from .AConfigurable import AConfigurable
from .IObserver import IObserver
from .ADumpable import ADumpable
from .IService import IService
from .AChannelObject import AChannelObject
from .Channel import *

class SihdService(ALoggable, AConfigurable, IObserver,
                    AChannelObject, ADumpable, IService):

    STOP = 0b0001
    START = 0b0010
    PAUSE = 0b0100
    RESUME = 0b1000

    def __init__(self, name="SihdService", **kwargs):
        super().__init__(name, **kwargs)
        self.set_default_conf({
            "channels_mp": 0,
        })
        # States
        self.__stopped = True
        self.__paused = False
        self.__init = False
        # Channels
        self.__channels_input = list()
        self.__channels_output = list()
        self.__todo_ichan = list()
        self.__todo_ochan = list()
        self.__todo_chan = list()
        self.__channel_notif = True
        self.__channels_mp = False
        # Time
        self.__start_time = None
        self.__stop_time = None

    #
    # ADumpable
    #

    def on_dump(self) -> dict:
        if self.is_active():
            raise RuntimeError("Cannot dump a running service")
        dic = super().on_dump()
        return dic

    def on_load(self, dic: dict):
        if self.is_active():
            raise RuntimeError("Cannot load a running service")
        super().on_load(dic)

    #
    # State Observation
    #

    def get_service_state(self):
        stopped = self.__stopped
        paused = self.__paused
        chan = self.get_channel("service_state")
        if chan:
            n = chan.read()
            if n is not None:
                stopped = bool(n & SihdService.STOP)
                paused = bool(n & SihdService.PAUSE)
            else:
                self.log_error("Could not read state")
        return stopped, paused

    def get_service_state_str(self):
        stopped, paused = self.get_service_state()
        if not stopped:
            s = "Running"
        else:
            s = "Stopped"
        if paused:
            s += " (paused)"
        return s

    def _service_state_changed(self):
        state = 0
        if self.__stopped:
            state |= SihdService.STOP
        else:
            state |= SihdService.START
        if self.__paused:
            state |= SihdService.PAUSE
        else:
            state |= SihdService.RESUME
        self.service_state.write(state)

    def set_service_state(self, state):
        if (state & SihdService.STOP):
            self.stop()
        elif (state & SihdService.START):
            self.start()
        if (state & SihdService.RESUME):
            self.resume()
        elif (state & SihdService.PAUSE):
            self.pause()

    #
    # AConfigurable
    #

    def _setup_impl(self):
        ret = super()._setup_impl()
        ret = ret and self.on_setup()
        self.__create_channel_state()
        ret = ret and self._make_channels()
        self.service_state.task_done()
        ret = ret and self.post_setup()
        return ret

    def on_setup(self):
        self.__channels_mp = bool(int(self.get_conf("channels_mp")))
        return True

    def post_setup(self):
        return True

    """ Life cycle """

    def is_paused(self):
        """
        stopped, paused = self.get_service_state()
        return paused
        """
        return self.__paused

    def is_running(self):
        """
        stopped, paused = self.get_service_state()
        return not stopped
        """
        return not self.__stopped

    def is_active(self):
        return (not self.is_paused() and self.is_running())

    def is_init(self):
        return self.__init

    #
    # Init
    #

    def init(self):
        if self.__init is True:
            self.log_debug("Service is already initialised")
            return True
        if self.is_configured() is False and self.setup() is False:
            return False
        self.process_links()
        if self._init_impl() is True:
            self.__init = True
            self.on_init()
        self.log_debug("%s" %
            ("is initialised" if self.__init else "did not initialise"))
        return self.__init

    def _init_impl(self):
        return True

    def on_init(self):
        pass

    #
    # Start
    #

    def get_service_start_time(self):
        return self.__start_time

    def start(self):
        if self.__stopped is False:
            self.log_debug("Starting an already started service")
            return False
        if self.__init is False and self.init() is False:
            return False
        self.__start_time = time.time()
        if self._start_impl() is True:
            self.__stopped = False
            self._service_state_changed()
            self.on_start()
        running = not self.__stopped
        if running is not True:
            self.__start_time = None
        self.log_info("%s" %
                ("is started" if running else "did not start"))
        return running

    def _start_impl(self):
        return True

    def on_start(self):
        pass

    #
    # Stop
    #

    def get_service_stop_time(self):
        return self.__stop_time

    def stop(self):
        if self.__stopped is True:
            self.log_debug("Stopping an already stopped service")
            return True
        self.__stop_time = time.time()
        if self._stop_impl() is True:
            self.__stopped = True
            self.__init = False
            self._service_state_changed()
            self.on_stop()
            self._set_unconfigured()
        running = not self.__stopped
        if running is True:
            self.__stop_time = None
        self.log_debug("%s" %
                ("is stopped" if not running else "did not stop"))
        return running is False

    def _stop_impl(self):
        return True

    def on_stop(self):
        pass

    #
    # Pause
    #

    def pause(self):
        if self.__paused is True:
            self.log_debug("Pausing an already paused service")
            return True
        if self._pause_impl() is True:
            self.__paused = True
            self._service_state_changed()
            self.on_pause()
        self.log_debug("%s" %
            ("is paused" if self.__paused else "did not pause"))
        return self.__paused

    def _pause_impl(self):
        return True

    def on_pause(self):
        pass

    #
    # Resume
    #

    def resume(self):
        if not self.__paused:
            self.log_debug("Resuming a non paused service")
            return True
        if self._resume_impl() is True:
            self.__paused = False
            self._service_state_changed()
            self.on_resume()
        self.log_debug("%s" %
                ("is resumed" if not self.__paused else "did not resume"))
        return not self.__paused

    def _resume_impl(self):
        return True

    def on_resume(self):
        pass

    #
    # Reset
    #

    def reset(self):
        if not self.__stopped and not self.stop():
            return False
        if self._reset_impl() is True:
            self.remove_children()
            self.__init = False
            self.__stopped = True
            self.__paused = False
            self._service_state_changed()
            self.on_reset()
            self._set_unconfigured()
        return self.__stopped and not self.__paused

    def _reset_impl(self):
        return True

    def on_reset(self):
        pass

    #
    # Channels Input/Output
    #

    def __replace_channel(self, old, new):
        ci = self.__channels_input
        co = self.__channels_output
        if old in ci:
            for i, channel in enumerate(ci):
                if channel == old:
                    break
            ci[i] = new
        elif old in co:
            for i, channel in enumerate(co):
                if channel == old:
                    break
            co[i] = new

    def on_new_channel(self, name, channel):
        old_channel = self.get_channel(name)
        if old_channel:
            self.__replace_channel(old_channel, channel)
        setattr(self, name, channel)
    
    def add_channel_input(self, name, **kwargs):
        self.__todo_ichan.append((name, kwargs))

    def add_channel_output(self, name, **kwargs):
        self.__todo_ochan.append((name, kwargs))

    def add_channel(self, name, **kwargs):
        self.__todo_chan.append((name, kwargs))

    def create_output_channel(self, name, **kwargs):
        channel = self.create_channel(name, **kwargs)
        if channel and self.set_channel_output(channel):
            return channel
        return None

    def create_input_channel(self, name, **kwargs):
        channel = self.create_channel(name, **kwargs)
        if channel and self.set_channel_input(channel):
            return channel
        return None

    def _make_channels(self):
        for name, dic in self.__todo_ichan:
            self.create_input_channel(name, **dic)
        #self.__todo_ichan = []
        for name, dic in self.__todo_ochan:
            self.create_output_channel(name, **dic)
        #self.__todo_ochan = []
        for name, dic in self.__todo_chan:
            self.create_channel(name, **dic)
        #self.__todo_chan = []
        return True

    def get_channels_input(self):
        return self.__channels_input

    def get_channels_output(self):
        return self.__channels_output

    def set_channel_input(self, channel, name=None, replace=False):
        if name is None:
            name = channel.get_name()
        input_lst = self.__channels_input
        if channel in input_lst:
            raise RuntimeError("Channel {} already in inputs".format(channel))
        input_lst.append(channel)
        return True

    def set_channel_output(self, channel, name=None, replace=False):
        if name is None:
            name = channel.get_name()
        output_lst = self.__channels_output
        if channel in output_lst:
            raise RuntimeError("Channel {} already in outputs".format(channel))
        output_lst.append(channel)
        return True

    #override
    def create_channel(self, name, **kwargs):
        if self.__channels_mp is True:
            kwargs['mp'] = True
        return super().create_channel(name, **kwargs)

    #   Reading

    def set_channel_notification(self, active):
        self.__channel_notif = active

    def handle(self, channel):
        """ Handle service's input """
        pass

    def _pre_handle(self, channel) -> bool:
        """ Used for services that want to handle stuff before user parsing """
        return False

    def read_channels_input(self) -> bool:
        if self.__channel_notif is False:
            return False
        for channel in self.get_channels_input():
            if channel.is_pollable() and channel.is_readable():
                if not self._pre_handle(channel):
                    self.handle(channel)
                channel.task_done()
        return True

    def __create_channel_state(self):
        name = "service_state"
        channel = self.create_channel(name, type='int', block=True,
                                        timeout=0.03, default=1)
