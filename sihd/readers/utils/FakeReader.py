#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import os
import sys
import sihd
from random import random
from sihd.readers.AReader import AReader

class FakeReader(AReader):

    def __init__(self, name="FakeReader", app=None):
        super().__init__(app=app, name=name)
        self.configuration.add_defaults({
            "channels": {},
            "random": False,
            "multiplier": 100,
        })
        self.rand = False
        self.mult = 1
        self.channels = []
        self.fake_keys = {}

    def add_fake_channel(self, name, **kwargs):
        keys = kwargs.pop('fake_keys', None)
        if keys is not None:
            self.fake_keys[name] = keys
        self.add_channel(name, **kwargs)

    #
    # Configuration
    #

    def on_setup(self, config):
        ret = super().on_setup(config)
        self.rand = config.get('random')
        self.mult = config.get('multiplier')
        channels = config.get('channels')
        for name, conf in channels.items():
            if isinstance(conf, str):
                conf = sihd.var.tokenize(conf)
            if conf is None:
                conf = {}
            self.add_fake_channel(name, **conf)
        return True

    def on_init(self):
        super().on_init()
        self.channels = self.get_channels()

    #
    # Step
    #

    def random(self):
        return random() * self.mult

    def rand_list(self, channel):
        size = channel.get_size()
        channel.write([self.random() for i in range(0, size)])

    def rand_dict(self, channel):
        keys = self.fake_keys.get(channel.get_name(), None)
        if keys is None:
            return
        for key in keys:
            channel.write(key, self.random())

    def rand_obj(self, channel):
        keys = self.fake_keys.get(channel.get_name(), None)
        if keys is None:
            return
        for key in keys:
            channel.write(key, self.random())

    def rand_channel(self, channel):
        t = channel.type
        if t == object:
            self.rand_obj(channel)
        elif t == list:
            self.rand_list(channel)
        elif t == dict:
            self.rand_dict(channel)
        elif channel.write_args == 0:
            channel.write()
        elif t == int or t == None:
            channel.write(int(self.random()))
        elif t == float:
            channel.write(self.random())

    def on_step(self):
        for channel in self.channels:
            if self.rand is True:
                self.rand_channel(channel)
        return True

    def on_reset(self):
        self.fake_keys.clear()
        self.channels.clear()
        super().on_reset()
