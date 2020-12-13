#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import sihd
from .AConfigurable import AConfigurable
from .AChannelObject import AChannelObject

class ChannelContainer(AConfigurable, AChannelObject):

    def __init__(self, name="ChannelContainer", **kwargs):
        super().__init__(name, **kwargs)
        self.configuration.add_defaults({
            'channels': {}
        })
        self.__init = False

    def make_channels(self):
        if self.__init is False:
            config = self.configuration
            channels = config.get('channels')
            for name, conf in channels.items():
                if isinstance(conf, str):
                    conf = sihd.var.tokenize(conf)
                self.create_channel(name, **conf)
            self.process_links()
            self.__init = True
        return self.__init

    def remove_channels(self):
        if self.__init is True:
            for channel in self.get_channels():
                self.remove_channel(channel)
            self.remove_children()
            self.__init = False
        return self.__init is False
