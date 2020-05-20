#!/usr/bin/python
#coding: utf-8

#
# System
#
import time

from .ANamedObjectContainer import ANamedObjectContainer
from .Channel import *

class AChannelObject(ANamedObjectContainer):

    def __init__(self, name="AChannelObject", **kwargs):
        super().__init__(name=name, **kwargs)
        self.__channel_links = dict()

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
    def on_link(self, name, channel):
        """ Callback """
        if isinstance(channel, Channel):
            old_channel = self.get_channel(name)
            if old_channel is not None and old_channel.is_multiprocess()\
                and not channel.is_multiprocess():
                raise ValueError("Link is downgrading multiprocess "
                                    "channel to none for: " + name)
            self.on_new_channel(name, channel)
        super().on_link(name, channel)

    #
    # Creation
    #

    def on_new_channel(self, name, channel):
        pass

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
        if self.get_channel(name) is not None:
            raise ValueError("Already a channel named " + name);
        if self.is_linked(name):
            link = self._get_link(name)
            if link is not None and isinstance(link, Channel):
                return link
            return None
        type = kwargs.pop('type', None)
        if type is None:
            type = "default"
        try:
            method = getattr(self, "create_channel_{}".format(type))
        except AttributeError:
            raise ValueError("No such type for channel {}".format(type))
        kwargs['parent'] = self
        channel = method(name, **kwargs)
        self.on_new_channel(name, channel)
        return channel

    #
    # Channel types
    #

    def create_channel_condition(self, name, **kwargs):
        return ChannelCondition(name=name, **kwargs)

    def create_channel_bool(self, name, **kwargs):
        return ChannelBool(name=name, **kwargs)

    #   Data struct

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

    #   Values

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

    #   Default

    def create_channel_default(self, name, **kwargs):
        return Channel(name=name, **kwargs)
