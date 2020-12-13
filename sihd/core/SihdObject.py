#!/usr/bin/python
# coding: utf-8

#
# System
#
import time

import sihd
from .ALoggable import ALoggable
from .AConfigurable import AConfigurable
from .IObserver import IObserver
from .ADumpable import ADumpable
from .IService import IService
from .AChannelObject import AChannelObject
from .Channel import Channel
from .ChannelContainer import ChannelContainer

class SihdObject(ALoggable, AConfigurable, IObserver,
                    AChannelObject, ADumpable, IService):

    STOP = 0b0001
    START = 0b0010
    PAUSE = 0b0100
    RESUME = 0b1000

    def __init__(self, name="SihdObject", **kwargs):
        super().__init__(name, **kwargs)
        # Configuration
        self.configuration.add_defaults({
            "channels_mp": False,
            "channels_input": 'poll',
            "channels_conf": None,
            "children": None,
            "links": None,
        }, expose=False)
        # Children
        self.__nostart_service = set()
        # States
        self.__stopped = True
        self.__paused = False
        self.__init = False
        # Channels input behavior
        self.__observe_inputs = False
        self.__poll_inputs = False
        self.__channel_notif = True
        self.__channels_mp = False
        self._chlock_timeout = 0.001
        # Channels
        self.__channels_input = list()
        self.__channels_made = False
        self.__chan_todo = list()
        self.__chan_conf = dict()
        # Time
        self.__start_time = None
        self.__stop_time = None

    #
    # Reading channels
    #

    def on_notify(self, obj):
        if isinstance(obj, Channel):
            if self.__channel_notif is True:
                self.handle(obj)
            return True
        return False

    def set_channel_notification(self, active):
        self.__channel_notif = bool(active)

    def is_channel_notification(self):
        return self.__channel_notif

    def handle(self, channel):
        """ Handle service's input """
        pass

    def _pre_handle(self, channel) -> bool:
        """ Used for services that want to handle stuff before user parsing """
        return False

    def _poll_channel(self, channel):
        # channel must be pollable, data to be consumed and lock must work
        if channel.pollable and channel.is_readable()\
                and channel.lock(self._chlock_timeout):
            # if pre handled -> no handle
            ret = self._pre_handle(channel)
            # data is consumed if handle does not return False
            ret = ret or self.handle(channel) is not False
            if not channel.unlock():
                self.log_warning("Could not unlock channel " + str(channel))
            # if channel handled - Notify channel that data is consumed
            if ret:
                channel.consumed_data()
            return ret
        return False

    def poll_channels_input(self) -> bool:
        if self.__poll_inputs is False:
            return False
        poll = self._poll_channel
        for channel in self.get_channels_input():
            poll(channel)
        return True

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
                stopped = bool(n & SihdObject.STOP)
                paused = bool(n & SihdObject.PAUSE)
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
            state |= SihdObject.STOP
        else:
            state |= SihdObject.START
        if self.__paused:
            state |= SihdObject.PAUSE
        else:
            state |= SihdObject.RESUME
        self.service_state.write(state)

    def set_service_state(self, state):
        if (state & SihdObject.RESUME):
            self.resume()
        elif (state & SihdObject.PAUSE):
            self.pause()
        if (state & SihdObject.STOP):
            self.stop()
        elif (state & SihdObject.START):
            self.start()

    #
    # Configuration
    #

    # override
    def load_conf(self, conf, **kwargs):
        ret = super().load_conf(conf, **kwargs)
        if ret:
            self.log_info("Service is configured")
        else:
            self.log_warning("Service failed to configure")
        return ret

    def _load_children_conf(self):
        ret = True
        children_conf = self.configuration.get_dynamic('children')
        if children_conf is not None:
            children_lst = self.get_children().values()
            for child in children_lst:
                if not isinstance(child, AConfigurable):
                    continue
                child_conf = children_conf.get(child.get_name(), None)
                if child_conf is None:
                    continue
                if not child.load_conf(child_conf, dynamic=True):
                    self.log_error("Could not load conf from child "
                                    + child.get_name())
                    ret = False
        return ret

    # override
    def get_conf(self):
        conf = super().get_conf()
        children_conf = {}
        children_lst = self.get_children().values()
        for child in children_lst:
            if not isinstance(child, AConfigurable):
                continue
            child_conf = child.get_conf()
            children_conf[child.get_name()] = child_conf
        if children_conf:
            if 'children' in conf:
                self.log_warning("Configuration 'children' is erased")
            conf['children'] = children_conf
        return conf

    def configure_links(self, dic):
        '''
            Waiting on links like: {
                channel: root.child.chan,
                channel2: ..same_parent_child.chan,
                ...
            }
        '''
        if dic is None:
            return
        for name, path in dic.items():
            self.link(name, path)

    def configure_channels(self, dic):
        ''' Configures channels differently before init '''
        if dic is None:
            return
        for name, conf in dic.items():
            self.set_channel_conf(name, conf)

    def __create_service_state(self):
        self.create_channel("service_state", type='int', default=1,
                            timeout=1, block=True)

    def _setup_impl(self, conf):
        ret = super()._setup_impl(conf)
        ret = ret and self.on_setup(conf) is not False
        self.__create_service_state()
        ret = ret and self._make_channels()
        ret = ret and self.post_setup() is not False
        ret = ret and self._load_children_conf()
        ret = ret and self.call_children('setup', cls=IService)
        if ret:
            self.log_debug("Service is configured")
        else:
            self.log_error("Service failed to configure")
        return ret

    def on_setup(self, conf):
        self.__channels_mp = bool(conf.get("channels_mp"))
        chinpt = conf.get("channels_input")
        if chinpt == "observe":
            self.set_observe_channels_input(True)
        elif chinpt == "poll":
            self.set_poll_channels_input(True)
        self.configure_links(conf.get("links"))
        self.configure_channels(conf.get("channels_conf"))
        return True

    def post_setup(self):
        return True

    #
    # Life cycle
    #

    def is_paused(self):
        return self.__paused

    def is_running(self):
        return not self.__stopped

    def is_stopped(self):
        return self.__stopped

    def is_active(self):
        return (not self.is_paused() and self.is_running())

    def is_init(self):
        return self.__init

    # Init

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
        self.log_debug("is initialised" if self.__init else "did not initialise")
        return self.__init

    def _init_impl(self):
        return self.call_children('init', cls=IService)

    def on_init(self):
        pass

    # Start

    def get_service_start_time(self):
        return self.__start_time

    def _has_started(self):
        running = not self.__stopped
        if running is not True:
            self.__start_time = None
        self.log_info("is started" if running else "did not start")
        return running

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
        return self._has_started()

    def _start_impl(self):
        return self.call_children('start', cls=IService)

    def on_start(self):
        pass

    # Stop

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
        self.log_info("is stopped" if not running else "did not stop")
        return running is False

    def _stop_impl(self):
        return self.call_children('stop', cls=IService)

    def on_stop(self):
        pass

    # Pause

    def pause(self):
        if self.__paused is True:
            self.log_debug("Pausing an already paused service")
            return True
        if self._pause_impl() is True:
            self.__paused = True
            self._service_state_changed()
            self.on_pause()
        self.log_debug("is paused" if self.__paused else "did not pause")
        return self.__paused

    def _pause_impl(self):
        return self.call_children('pause', cls=IService)

    def on_pause(self):
        pass

    # Resume

    def resume(self):
        if not self.__paused:
            self.log_debug("Resuming a non paused service")
            return True
        if self._resume_impl() is True:
            self.__paused = False
            self._service_state_changed()
            self.on_resume()
        self.log_debug("is resumed" if not self.__paused else "did not resume")
        return not self.__paused

    def _resume_impl(self):
        return self.call_children('resume', cls=IService)

    def on_resume(self):
        pass

    # Reset

    def reset(self):
        if not self.__stopped and not self.stop():
            return False
        if self._reset_impl() is True:
            self.on_reset()
            self.__init = False
            self.__stopped = True
            self.__paused = False
            self._service_state_changed()
            self._set_unconfigured()
            self.__remove_channels()
            self.remove_children()
            self.__chan_todo.clear()
            self.__chan_conf.clear()
            self.__start_time = None
            self.__stop_time = None
        return self.__stopped and not self.__paused

    def _reset_impl(self):
        return self.call_children('reset', cls=IService)

    def on_reset(self):
        pass

    #
    # Children recursive calls
    #

    def prevent_service_start(self, child):
        """ Prevent service state (start, stop, resume, pause) waterfall """
        if not isinstance(child, IService):
            raise TypeError("{}: not a service: {}".format(self, child))
        if not self.is_child(child):
            raise TypeError("{}: not a child: {}".format(self, child))
        name = str(child)
        self.__nostart_service.add(name)

    def allow_service_start(self, child):
        """ Allow back service state (start, stop, resume, pause) waterfall """
        if not isinstance(child, IService):
            raise TypeError("{}: not a service: {}".format(self, child))
        if not self.is_child(child):
            raise TypeError("{}: not a child: {}".format(self, child))
        name = str(child)
        self.__nostart_service.remove(name)

    def call_children(self, method, cls=None, fail_ret=False,
                      pass_children=[], args=[], kwargs={}) -> bool:
        """
            :param method: string method to call on children
            :param cls: class to match on children
            :param fail_ret: value that children should not return
            :param pass_children: list of children not concerned
            :param args: list of arguments for method
            :param kwargs: dict of keywords for method

            :return: True if every call successed
        """
        retval = True
        children_lst = self.get_children().values()
        prevent_start_children = None
        if method in ('start', 'stop', 'resume', 'pause'):
            prevent_start_children = self.__nostart_service
        for child in children_lst:
            #Call on specific class
            if not isinstance(child, cls):
                continue
            #Prevent child from being called
            if child in pass_children:
                continue
            #Prevent child from being started/stopped by name
            if prevent_start_children and str(child) in prevent_start_children:
                continue
            #Get callable
            try:
                fun = getattr(child, method)
            except AttributeError as e:
                raise NotImplementedError("{}: class '{}' "
                    "does not implement '{}'".format(self,
                        child.__class__.__name__, method))
            if not callable(fun):
                raise ValueError("{}: not a callable {}".format(self, method))
            #Execute
            try:
                ret = fun(*args, **kwargs)
                if ret == fail_ret:
                    self.log_warning("Child `{}` call `{}` failed"\
                                        .format(child.get_name(), method))
                    retval = False
            except Exception as e:
                self.log_error(e)
                self.log_error(sihd.sys.get_traceback())
                retval = False
        return retval

    #
    # Channels
    #


    def on_remove_channel(self, channel):
        name = channel.get_name()
        channel.remove_observer(self)
        self.remove_channel(name)

    def __remove_channels(self):
        for channel in self.get_channels():
            self.on_remove_channel(channel)
        self.__channels_input.clear()
        self.__channels_made = False

    # override
    def create_channel(self, name, strconf=None, **kwargs):
        self.log_debug("Creating channel: {}".format(name))
        if self.__channels_mp is True:
            kwargs['mp'] = True
        return super().create_channel(name, **kwargs)

    #
    # Channels input
    #

    def is_polling_channels_input(self):
        return self.__poll_inputs

    def is_observing_channels_input(self):
        return self.__observe_inputs

    def set_poll_channels_input(self, active):
        self.__poll_inputs = bool(active)

    def set_observe_channels_input(self, active):
        active = bool(active)
        self.__observe_inputs = active
        if self.__init is True:
            input_lst = self.__channels_input
            for channel in input_lst:
                if active:
                    channel.add_observer(self)
                else:
                    channel.remove_observer(self)

    def __replace_channel(self, old, new):
        ci = self.__channels_input
        if old in ci:
            for i, channel in enumerate(ci):
                if channel == old:
                    break
            ci[i] = new

    # override
    def link(self, name, path_or_no):
        self.log_debug("Link request: {} -> {}".format(name, path_or_no))
        return super().link(name, path_or_no)

    # override
    def on_link(self, name, obj):
        self.log_debug("Link: {}.{} -> {}".format(self, name, obj))
        old_channel = self.get_channel(name)
        super().on_link(name, obj)
        if old_channel:
            if isinstance(obj, Channel) and old_channel != obj:
                self.__replace_channel(old_channel, obj)
        elif old_channel is None:
            if self._is_channel_to_add(name, input=True):
                self.set_channel_input(obj)

    # Channels input add

    def __add_channel_type(self, type, name, conf):
        self.__chan_todo.append((name, type))
        self.__chan_conf[name] = conf

    def add_channel_input(self, name, **kwargs):
        self.__add_channel_type(1, name, kwargs)

    def add_channel(self, name, strconf=None, **kwargs):
        if strconf is not None:
            kwargs.update(sihd.var.tokenize(strconf))
        self.__add_channel_type(None, name, kwargs)

    def _get_channels_to_add(self):
        return [name for name, type in self.__chan_todo]

    # Channels input checkers

    def _is_channel_to_add(self, chan_name, input=False):
        for name, category in self.__chan_todo:
            if input is True and category != 1:
                continue
            if name == chan_name:
                return True
        return False

    def is_channel_input(self, channel):
        return channel in self.__channels_input

    # Channels input create

    def create_input_channel(self, name, **kwargs):
        channel = self.create_channel(name, **kwargs)
        if channel and self.set_channel_input(channel):
            return channel
        return None

    def _make_channels(self):
        """ Entry point of channel creation """
        if self.__channels_made:
            self.log_warning("Trying to make channels but already done")
            return False
        for name, type in self.__chan_todo:
            conf = self.__chan_conf.get(name, {})
            if type == 1:
                self.create_input_channel(name, **conf)
            else:
                self.create_channel(name, **conf)
        self.__channels_made = True
        return True

    # Channels input getters setters

    def set_channel_conf(self, name, strconf=None, **kwargs):
        if strconf is not None:
            kwargs.update(sihd.var.tokenize(strconf))
        if not kwargs:
            self.log_warning("No conf changes for channel: " + str(name))
            return False
        if self.__channels_made:
            self.log_warning("Trying to change already made "
                             "channel configuration: " + str(name))
            return False
        conf = self.__chan_conf.get(name, None)
        if conf is None:
            raise RuntimeError("{}: Conf not found for channel {}"\
                                .format(self, name))
        conf.update(kwargs)
        self.log_debug("Changed channel {} conf to {}".format(name, kwargs))
        return True

    def get_channels_input(self):
        return self.__channels_input

    def set_channel_input(self, channel):
        input_lst = self.__channels_input
        if channel in input_lst:
            raise RuntimeError("{}: Channel already in inputs: {}".format(self, channel))
        if self.__observe_inputs is True:
            channel.add_observer(self)
        input_lst.append(channel)

    def remove_channel_input(self, channel):
        self.__channels_input.remove(channel)
