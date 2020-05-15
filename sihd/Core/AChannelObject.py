#!/usr/bin/python
#coding: utf-8

#
# System
#
import time

from .ANamedObject import ANamedObject
from .Channel import *

class AChannelObject(ANamedObject):

    def __init__(self, name="AChannelObject", **kwargs):
        super().__init__(name=name, **kwargs)
        self.__channels = dict()
        self.__channel_links = dict()

    #
    # Lock
    #

    def unlock_channels(self, channels=None):
        if channels is None:
            channels = self.get_channels_list()
        for channel in channels:
            if channel.is_locked():
                channel.unlock()

    def lock_channels(self, channels=None):
        if channels is None:
            channels = self.get_channels_list()
        for channel in channels:
            if not channel.is_locked():
                channel.lock()

    def clear_channels(self):
        self.__channels.clear()

    #
    # Getters
    #

    def get_channels_list(self):
        return [chan for name, chan in self.__channels.items()]

    def get_channel(self, name):
        return self.__channels.get(name, None)

    #
    # Links
    #

    def on_channel_link(self, name, channel):
        """ Callback """
        old_channel = self.get_channel(name)
        if old_channel is not None:
            if old_channel.is_multiprocess() and not channel.is_multiprocess():
                raise ValueError("Link is downgrading multiprocess "
                                    "channel to none for: " + name)
        self.on_new_channel(item)

    def link_channel(self, name: name, path_or_channel: Channel or str):
        """ Add a channel link """
        if isinstance(path_or_channel, str):
            self.__channel_links[name] = path_or_channel
        elif isinstance(path_or_channel, Channel):
            self.on_channel_link(name, path_or_channel)

    def process_channels_links(self):
        """ Process all links """
        for name, path in self.__channels_links.items():
            item = self.get_channel_link(name)
            if isinstance(item, Channel):
                self.on_channel_link(name, item)
            elif item is not None:
                raise ValueError("Linked object is not a channel: {}"\
                                    .format(item))
            else:
                raise ValueError("Linked object does not exists: {}"\
                                    .format(path))

    def _get_channel_link(self, name):
        link = self.__channels_links.get(name, None)
        if link is not None:
            item = self.find_namedobject(link)
            if item is not None:
                return item
        return None

    #
    # Creation
    #

    def on_new_channel(self, channel):
        self.__channels[name] = channel 

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
        link = self._get_channel_link(name)
        if link is not None and isinstance(link, Channel):
            return link
        type = kwargs.pop('type', None)
        if type is None:
            type = "default"
        try:
            method = getattr(self, "create_channel_{}".format(type))
        except AttributeError:
            raise ValueError("No such type for channel {}".format(type))
        kwargs['parent'] = self
        channel = method(name, **kwargs)
        self.on_new_channel(channel)
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
