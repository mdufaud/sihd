#!/usr/bin/python
#coding: utf-8

from sihd.Handlers.IHandler import IHandler
from sihd.Core.IDumpable import IDumpable

from sihd.Tools.pcap import PcapWriter
from sihd.Tools.pcap import PcapReader

class PcapSaverHandler(IHandler):

    def __init__(self, app=None, name="PcapSaverHandler"):
        super(PcapSaverHandler, self).__init__(app=app, name=name)
        self._set_default_conf({
            "service_type": "thread",
            "activate": 0,
            "save_raw": 0,
            "save_type": 'queue',
        })
        self.__save = False
        self.__writer = PcapWriter()
        self.add_channel_input("activate", type='bool')
        self.add_channel_input("save", type='queue')
        self.add_channel_input("dump_path", type='queue')

    def do_setup(self):
        ret = super().do_setup()
        self.__active = bool(int(self.get_conf("activate")))
        self.__save = bool(int(self.get_conf("save_raw")))
        if self.__save:
            self.add_channel_output("saved", type=self.get_conf('save_type'))
        return True

    """ IObservable """

    def handle(self, channel):
        if channel == self.save:
            data = channel.read()
            if self.__decode:
                data = self.decode_data(data)
            self.store_data(data)
        elif channel == self.dump_path:
            path = channel.read()
            if path:
                self.dump_to(path)
        elif channel == self.activate:
            self.set_saving(channel.read())
        return True

    """ PcapSaverHandler """

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
            self.saved.write((capinfo, data))

    """ IDumpable """

    def dump_to(self, filename, perm="wb+"):
        try:
            self.__writer.write_pcap(filename, mode=perm)
            return True
        except IOError as e:
            self.log_error(e)
        return False

    def load_from(self, filename, perm='rb'):
        reader = PcapReader()
        writer = self.__writer
        lst = reader.read_all(filename, perm)
        store = self.store_data
        for capinfo, pkt in lst:
            store(pkt, capinfo)
        return len(lst) > 0
