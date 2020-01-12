#!/usr/bin/python
#coding: utf-8

from sihd.srcs.Handlers.IHandler import IHandler
from sihd.srcs.Core.IDumpable import IDumpable

base64 = None

from sihd.srcs.Tools.pcap import PcapWriter

class PcapSaverHandler(IHandler, IDumpable):

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

    def _load(self, buf):
        binary = base64.b64decode(buf)
        s = binary.decode('utf-8')
        datas = s.split('\n')
        lst = self.__data_saved
        for data in datas:
            pyvar = eval(data)
            lst.append(pyvar)
        return True

    def __encode_data(self, data):
        s = str(data) + "\n"
        binary = s.encode('utf-8')
        b64 = base64.b64encode(binary)
        return b64

    def _dump(self):
        ret = None
        for source, lst in self.__sources_data.items():
            for datas in lst:
                b64 = self.__encode_data(datas)
                ret = b64 if ret is None else ret + b64
        return ret
