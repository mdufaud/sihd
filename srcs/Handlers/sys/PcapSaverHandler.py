#!/usr/bin/python
#coding: utf-8

from sihd.srcs.Handlers.IHandler import IHandler
from sihd.srcs.Core.IDumpable import IDumpable

from sihd.srcs.Tools.pcap import PcapWriter
from sihd.srcs.Tools.pcap import PcapReader

class PcapSaverHandler(IHandler):

    def __init__(self, app=None, name="PcapSaverHandler"):
        global base64
        if base64 is None:
            import base64
        super(PcapSaverHandler, self).__init__(app=app, name=name)
        self._set_default_conf({})
        self.__writer = PcapWriter()

    def _setup_impl(self):
        return True

    """ IObservable """

    def handle(self, observable, *args):
        self.__writer.add_pkt(args)
        return True

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
        for capinfo, pkt in lst:
            writer.add_pkt(pkt, capinfo.sec, capinfo.usec,
                            capinfo.cap_len, capinfo.orig_len)
        return len(lst) > 0
