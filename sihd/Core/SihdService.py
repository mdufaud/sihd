#!/usr/bin/python
#coding: utf-8
""" System """
import time

from .ALoggable import ALoggable
from .AConfigurable import AConfigurable
from .IObserver import IObserver
from .ADumpable import ADumpable
from .IService import IService
from .Channel import *

class SihdService(ALoggable, AConfigurable, IObserver,
                    ADumpable, IService):

    STOP = 0b0001
    START = 0b0010
    PAUSE = 0b0100
    RESUME = 0b1000

    def __init__(self, name="SihdService", **kwargs):
        super(SihdService, self).__init__(name, **kwargs)
        self._set_default_conf({
            "channels_mp": 0,
        })
        self.__channels_mp = False
        self.__stopped = True
        self.__paused = False
        self.__channels = dict()
        self.__channels_input = list()
        self.__channels_output = list()
        self.__todo_ichan = list()
        self.__todo_ochan = list()
        self.__start_time = None
        self.__stop_time = None
        self.__channel_notif = True

    """ ADumpable """

    def on_dump(self) -> dict:
        if self.is_active():
            raise RuntimeError("Cannot dump a running service")
        dic = super().on_dump()
        return dic

    def on_load(self, dic: dict):
        if self.is_active():
            raise RuntimeError("Cannot load a running service")
        super().on_load(dic)

    """ State Observation """

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

    """ IObserver """

    def set_channel_notification(self, active):
        self.__channel_notif = active

    def handle(self, channel):
        """ Handle service's input """
        pass

    def _pre_handle(self, channel) -> bool:
        """ Used for services that want to handle stuff before user parsing """
        return False

    def on_notify(self, channel):
        if not self._pre_handle(channel):
            self.handle(channel)

    """ AConfigurable """

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

    """ Start """

    def get_service_start_time(self):
        return self.__start_time

    def start(self, silent=False):
        if self.__stopped is False:
            if not silent:
                self.log_debug("Starting an already started service")
            return False
        if self.is_configured() is False and self.setup() is False:
            return False
        self.__start_time = time.time()
        if self._start_impl() is True:
            self.__stopped = False
            self._service_state_changed()
            self.on_start()
        running = not self.__stopped
        if running is not True:
            self.__start_time = None
        if not silent:
            self.log_info("%s" %
                    ("is started" if running else "did not start"))
        return running

    def _start_impl(self):
        return True

    def on_start(self):
        return True

    """ Stop """

    def get_service_stop_time(self):
        return self.__stop_time

    def stop(self, silent=False):
        if self.__stopped is True:
            if not silent:
                self.log_debug("Stopping an already stopped service")
            return True
        self.__stop_time = time.time()
        if self._stop_impl() is True:
            self.__stopped = True
            self._service_state_changed()
            self.on_stop()
            self._set_unconfigured()
        running = not self.__stopped
        if running is True:
            self.__stop_time = None
        if not silent:
            self.log_debug("%s" %
                    ("is stopped" if not running else "did not stop"))
        return running is False

    def _stop_impl(self):
        return True

    def on_stop(self):
        return True

    """ Pause """

    def pause(self, silent=False):
        if self.__paused is True:
            if not silent:
                self.log_debug("Pausing an already paused service")
            return True
        if self._pause_impl() is True:
            self.__paused = True
            self._service_state_changed()
            self.on_pause()
        if not silent:
            self.log_debug("%s" %
                ("is paused" if self.__paused else "did not pause"))
        return self.__paused

    def _pause_impl(self):
        return True

    def on_pause(self):
        return True

    """ Resume """

    def resume(self, silent=False):
        if not self.__paused:
            if not silent:
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
        return True

    """ Reset """

    def reset(self):
        if not self.__stopped and not self.stop():
            return False
        if self._reset_impl() is True:
            self.clear_channels()
            self.on_reset()
            self.__stopped = True
            self.__paused = False
            self._service_state_changed()
            self._set_unconfigured()
        return self.__stopped and not self.__paused

    def _reset_impl(self):
        self.log_debug("Called a non implemented function: reset")
        return False

    def on_reset(self):
        return True


    """ Channel creation """

    def create_channel_condition(self, name, **kwargs):
        return ChannelCondition(name=name, **kwargs)

    def create_channel_bool(self, name, **kwargs):
        return ChannelBool(name=name, **kwargs)

    """     Data struct """

    def create_channel_list(self, name, **kwargs):
        return ChannelList(name=name, **kwargs)

    def create_channel_dict(self, name, **kwargs):
        return ChannelDict(name=name, **kwargs)

    def create_channel_array(self, name, **kwargs):
        return ChannelArray(name=name, **kwargs)

    def create_channel_dict(self, name, **kwargs):
        return ChannelDict(name=name, **kwargs)

    def create_channel_object(self, name, **kwargs):
        return ChannelObject(name=name, **kwargs)

    def create_channel_queue(self, name, **kwargs):
        return ChannelQueue(name=name, **kwargs)

    def create_channel_string(self, name, **kwargs):
        return ChannelString(name=name, **kwargs)

    def create_channel_pickle(self, name, **kwargs):
        return ChannelPickle(name=name, **kwargs)

    """     Values """

    def create_channel_byte(self, name, **kwargs):
        return ChannelByte(name=name, **kwargs)

    def create_channel_char(self, name, **kwargs):
        return ChannelChar(name=name, **kwargs)

    def create_channel_short(self, name, **kwargs):
        return ChannelShort(name=name, **kwargs)

    def create_channel_int(self, name, **kwargs):
        return ChannelInt(name=name, **kwargs)

    def create_channel_long(self, name, **kwargs):
        return ChannelLong(name=name, **kwargs)

    def create_channel_double(self, name, **kwargs):
        return ChannelDouble(name=name, **kwargs)

    """     Default """

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
        if self.__channels_mp is True:
            kwargs['mp'] = True
        kwargs['parent'] = self
        channel = method(name, **kwargs)
        if channel:
            #self.log_debug("<- {}".format(channel))
            pass
        return channel

    """ Channels Input/Output """

    def link_channel(self, name, new_channel):
        old_channel = self.get_channel(name)
        if not old_channel:
            self.log_error("No channel {} to link".format(name))
            return False
        if old_channel.is_multiprocess() and not new_channel.is_multiprocess():
            self.log_warning("Replacing channel {} with a not multiprocessed one {}"\
                                .format(old_channel, new_channel))
        if old_channel in self.__channels_input:
            if not self.set_channel_input(new_channel, name, replace=True):
                self.log_error("Cannot link channel {} to {}".format(name, new_channel))
                return False
        elif old_channel in self.__channels_output:
            if not self.set_channel_output(new_channel, name, replace=True):
                self.log_error("Cannot link channel {} to {}".format(name, new_channel))
                return False
        else:
            setattr(self, name, new_channel)
        self.log_debug("Channel {} linked to {}".format(name, new_channel))
        return True

    """     Reading """

    def read_channels_input(self) -> bool:
        if self.__channel_notif is False:
            return False
        for channel in self.get_channels_input():
            if channel.is_pollable() and channel.is_readable():
                self.on_notify(channel)
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
        if channel and self.set_channel_input(channel):
            return channel
        return None

    def create_output_channel(self, name, **kwargs):
        channel = self.create_channel(name, **kwargs)
        if channel and self.set_channel_output(channel):
            return channel
        return None

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

    def set_channel_input(self, channel, name=None, replace=False):
        if name is None:
            name = channel.get_name()
        input_lst = self.__channels_input
        has_to_replace = False
        for i, c in enumerate(input_lst):
            if c.get_name() == name:
                has_to_replace = True
                break
        if has_to_replace:
            if replace is False:
                self.log_warning("Input channel: '{}' already exist".format(name))
                return False
            input_lst[i] = channel
        else:
            if replace is True:
                self.log_warning("Input channel: '{}' cannot be replaced "
                    "as it does not exist".format(name))
                return False
            input_lst.append(channel)
        setattr(self, name, channel)
        #channel.add_observer(self)
        return True

    def set_channel_output(self, channel, name=None, replace=False):
        if name is None:
            name = channel.get_name()
        output_lst = self.__channels_output
        has_to_replace = False
        for i, c in enumerate(output_lst):
            if c.get_name() == name:
                has_to_replace = True
                break
        if has_to_replace:
            if replace is False:
                self.log_warning("Output channel: '{}' already exist".format(name))
                return False
            output_lst[i] = channel
        else:
            if replace is True:
                self.log_warning("Output channel: '{}' cannot be replaced "
                    "as it does not exist".format(name))
                return False
            output_lst.append(channel)
        setattr(self, name, channel)
        return True

    def __create_channel_state(self):
        name = "service_state"
        """
        self.add_channel_input(name, type='int', block=True,
                                timeout=0.03, default=1)
        """
        channel = self.create_channel(name, type='int', block=True,
                                        timeout=0.03, default=1)
        setattr(self, name, channel)

    """ Careful """

    def _set_stopped(self, stopped):
        """ Do not call if you do not know what you are doing """
        self.__stopped = stopped

    def _set_paused(self, paused):
        """ Do not call if you do not know what you are doing """
        self.__paused = paused
