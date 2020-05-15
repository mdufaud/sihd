#!/usr/bin/python
#coding: utf-8

from sihd.Handlers.AHandler import AHandler
from sihd.Core.ADumpable import ADumpable

from sihd.Tools.pcap import PcapWriter
from sihd.Tools.pcap import PcapReader

class PcapHandler(AHandler):

    def __init__(self, app=None, name="PcapHandler"):
        super(PcapHandler, self).__init__(app=app, name=name)
        self.set_default_conf({
            "runnable_type": "thread",
            "activate": 0,
            "save_raw": 0,
            "save_type": 'queue',
            "endianness": 'big',
        })
        self.__save = False
        self.add_channel_input("activate", type='bool')
        self.add_channel_input("save", type='queue')
        self.add_channel_input("dump_path", type='queue')

    #
    # Configuration
    #

    def on_setup(self):
        ret = super().on_setup()
        self.__save = bool(int(self.get_conf("save_raw")))
        if self.__save:
            self.add_channel_output("saved", type=self.get_conf('save_type'))
        self.__writer = PcapWriter(self.get_conf("endianness"))
        return True

    def post_setup(self):
        self.set_saving(bool(int(self.get_conf("activate"))))
        return True

    #
    # Channels
    #

    def handle(self, channel):
        if channel == self.save:
            data = channel.read()
            if data is not None:
                self.store_data(data)
        elif channel == self.dump_path:
            path = channel.read()
            if path:
                self.dump_to(path)
        elif channel == self.activate:
            self.set_saving(channel.read())
        return True

    def on_stop(self):
        super().on_stop()
        #Unlock channels
        self.set_saving(True)

    #
    # Handler
    #

    def set_saving(self, activate):
        if activate:
            self.save.unlock()
            self.dump_path.unlock()
            if self.__save:
                self.saved.unlock()
        else:
            self.save.lock()
            self.dump_path.lock()
            if self.__save:
                self.saved.lock()

    def store_data(self, data, capinfo=None):
        writer = self.__writer
        if capinfo is not None:
            writer.add_cap(capinfo, data)
        else:
            capinfo, data = writer.add_data(data)
        if self.__save:
            self.saved.write(data)

    #
    # ADumpable
    #

    def dump_to(self, filename, perm="wb+"):
        try:
            self.__writer.write_pcap(filename, mode=perm)
        except IOError as e:
            self.log_error("Pcap error {}".format(e))
            return False
        return True

    def load_from(self, filename, perm='rb'):
        reader = PcapReader()
        writer = self.__writer
        try:
            lst = reader.read_all(filename, mode=perm)
        except (IOError, ValueError) as e:
            self.log_error("Pcap error {}".format(e))
            return False
        store = self.store_data
        for capinfo, pkt in lst:
            store(pkt, capinfo)
        return True
