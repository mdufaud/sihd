#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import sihd
import weakref
from .ANamedObjectContainer import ANamedObjectContainer
from .Channel import *

class AChannelObject(ANamedObjectContainer):

    def __init__(self, name="AChannelObject", **kwargs):
        super().__init__(name=name, **kwargs)

    #
    # Lock
    #

    def unlock_channels(self, channels=None):
        if channels is None:
            channels = self.get_channels()
        for channel in channels:
            if channel.is_locked():
                channel.unlock()

    def lock_channels(self, channels=None):
        if channels is None:
            channels = self.get_channels()
        for channel in channels:
            if not channel.is_locked():
                channel.lock()

    #
    # Getters
    #

    def get_channels(self):
        children = self.get_children()
        channels = [c for c in children.values()\
                    if isinstance(c, Channel)]
        return channels

    def get_channel(self, name):
        child = self.get_child(name)
        child = child if isinstance(child, Channel) else None
        return child

    #
    # Links
    #

    # override
    def on_link(self, name, obj):
        """ Callback """
        if isinstance(obj, Channel):
            old_channel = self.get_channel(name)
            if old_channel is not None\
                    and old_channel.is_multiprocess()\
                    and not obj.is_multiprocess():
                raise ValueError("{}: link is downgrading multiprocess "
                                    "channel to none for: {}".format(
                                    self, name))
        super().on_link(name, obj)
        if isinstance(obj, Channel):
            self.on_new_channel(name, obj)

    #
    # Creation
    #

    def on_new_channel(self, name, channel):
        setattr(self, name, channel)

    def create_channel(self, name, **kwargs):
        """
            Gets a create_channel_TYPE from service methods
            and make a channel from it. TYPE is contained as a key argument in
            'type', if not, make a default channel.

            :param name: str channel name
            :param type: str channel type
            :param block: bool permits blocking with certain locks in channels
                            on read/write before returning result
            :param timeout: float when blocking is enabled, set the timeout before
                            returning
            :param pollable: bool enable polling from channel when a write is done on
                                the channel in any thread/process
            :param mp: bool change in some channels internal variable from
                            threading to multiprocessing or Manager based
            :param timestamp: bool timestamp every write
            :param default: default value in channel
            :param lfilter: method to filter value when channel is writing
            :return: A Channel if the creation method was found else None
            :rtype: Channel
        """
        strconf = kwargs.pop('strconf', None)
        if strconf is not None:
            kwargs.update(sihd.var.tokenize(strconf))
        if self.get_channel(name) is not None:
            raise ValueError("Already a channel named {}".format(name));
        if self.is_linked(name):
            return None
        chan_type = kwargs.pop('type', None)
        if chan_type is None:
            chan_type = "default"
        cls = channel_factory.get(chan_type, None)
        if cls is None:
            raise ValueError("No such type for channel {}".format(chan_type))
        kwargs['parent'] = self
        channel = cls(name=name, **kwargs)
        self.on_new_channel(name, channel)
        return channel

    #
    # Removal
    #

    def remove_channel(self, ch):
        if isinstance(ch, Channel):
            name = ch.get_name()
        else:
            name = ch
        if self.get_channel(name) is None:
            raise ValueError("No such channel: {}".format(name))
        setattr(self, name, None)
