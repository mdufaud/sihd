#!/usr/bin/python
#coding: utf-8

from sihd.Handlers.AHandler import AHandler

class DuplicatorHandler(AHandler):

    def __init__(self, app=None, name="DuplicatorHandler"):
        super(DuplicatorHandler, self).__init__(app=app, name=name)
        self._set_default_conf({})
        self.__channel_lst = set()
        self.add_channel_input('input')

    def duplicate_to(self, channels):
        lst = self.__channel_lst
        if isinstance(channels, (list, tuple, set)):
            for channel in channels:
                lst.add(channel)
        else:
            lst.add(channels)

    """ SihdService """

    def handle(self, channel):
        data = channel.read()
        if data is None:
            return True
        for channel in self.__channel_lst:
            channel.write(data)
        return True
