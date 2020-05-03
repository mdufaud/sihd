#!/usr/bin/python
#coding: utf-8

""" System """
import struct
import time
import os

from sihd.Readers.AReader import AReader
from sihd.Tools.pcap import PcapReader as PcapReaderTool

class PcapReader(AReader):
    
    files_read = {}  

    def __init__(self, app=None, name="PcapReader"):
        super(PcapReader, self).__init__(app=app, name=name)
        self._set_default_conf({
            "path": "/path/to/file",
        })
        self.__to_recover = 0
        self.__pcap_reader = None
        self.add_channel_input("path", type='queue', simple=True)
        self.add_channel_output("pcap_header", type='pickle', size=200)
        self.add_channel_output("packet", type='queue')
        self.add_channel_output("packet_info", type='queue')
        self.add_channel_output('packets', type='int', default=0)
        self.add_channel_output('eof', type='bool',
                                default=True, timeout=0.1)

    """ AConfigurable """

    def post_setup(self):
        ret = super().post_setup()
        if ret:
            path = self.get_conf("path", default=False)
            if path:
                self.path.write(path)
        return ret

    """ SihdService """

    def handle(self, channel):
        if channel == self.path:
            if self.__pcap_reader is None:
                path = channel.read()
                if path:
                    self.set_source(path)

    """ Reader """

    def __can_recover(self):
        if self.__path not in PcapReader.files_read:
            return False
        tupl = PcapReader.files_read[self.__path]
        if tupl[1] == False:
            s = "File {} already read".format(self.__path)
            self.log_info(s)
            return True
        self.__to_recover = tupl[0]
        self.log_debug("To recover {}".format(self.__to_recover))
        return True

    def __recover(self):
        reader = self.__pcap_reader
        recover = self.__to_recover
        while self.is_active() and recover > 0:
            info, pkt = self.__pcap_reader.read_pkt()
            recover -= 1
        self.__to_recover = recover
        if recover > 0:
            return False
        self.log_info("Recovered {}".format(self.__path))
        return True

    def set_source(self, path):
        path = os.path.abspath(path) if path else None
        reader = PcapReaderTool()
        try:
            reader.open(path)
        except (FileNotFoundError, IOError) as err:
            self.log_error("Cannot open: {}".format(err))
        if reader.check_magic() is True:
            if reader.check_header() is True:
                if self.__pcap_reader:
                    self.__pcap_reader.close()
                self.__path = path
                self.__pkts = 0
                self.__to_recover = 0
                self.__pcap_reader = reader
                #Channel update
                self.pcap_header.write(reader.get_header())
                self.packets.write(0)
                self.eof.write(0)
                self.log_info("Reading file {name}".format(name=self.__path))
                self.__can_recover()
            else:
                self.log_error("Invalid pcap file {}".format(path))
        else:
            self.log_error("Invalid magic {}".format(path))
        return reader.is_open()

    def get_pcap_header(self):
        rd = self.__pcap_reader
        if rd:
            return rd.get_header()
        return None

    def read_packet(self):
        info = None
        pkt = None
        try:
            info, pkt = self.__pcap_reader.read_pkt()
        except EOFError as e:
            pass
        return info, pkt

    def on_step(self):
        if not self.__pcap_reader:
            return True
        if self.__to_recover > 0 and self.__recover() == False:
            return True
        info, pkt = self.read_packet()
        if pkt is None:
            self._read_end()
            return False
        self.packet_info.write(info)
        self.packet.write(pkt)
        self.__pkts += 1
        self.packets.write(self.__pkts)
        return True

    def _read_end(self):
        stop_time = time.time()
        self.log_info("File {0:s} read - {1:d} packets".format(self.__path, self.__pkts))
        self.log_debug("took {0:.3f} seconds to read and process {1:d} lines"\
                .format(stop_time - self.get_service_start_time(), self.__pkts))
        self.eof.write(1)
        self.close()

    def close(self):
        if self.__pcap_reader:
            self.__pcap_reader.close()
            self.__pcap_reader = None
            PcapReader.files_read[self.__path] = (self.__pkts, self.eof.read() == False)
            self.log_debug("File {} closed".format(self.__path))
