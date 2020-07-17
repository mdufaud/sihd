#!/usr/bin/python
# coding: utf-8

#
# System
#
import struct
import time
import os

from sihd.Readers.AReader import AReader
from sihd.Tools.pcap import PcapReader as PcapReaderTool

class PcapReader(AReader):
    
    files_read = {}  

    def __init__(self, name="PcapReader", app=None, **kwargs):
        super().__init__(app=app, name=name, **kwargs)
        self.configuration.add_defaults({
            "path": "/path/to/file",
        })
        self.__pcap_reader = None
        self.add_channel_input("path")
        self.add_channel("linktype", type='int')
        self.add_channel("pcap_header")
        self.add_channel("packet")
        self.add_channel("packet_info")
        self.add_channel('packets', type='counter')
        self.add_channel('eof', type='bool', default=True)

    #
    # Configuration
    #

    def on_init(self):
        """ After setup to have eof channel created """
        path = self.configuration.get('path', default=False)
        if path:
            self.path.write(path)
        return super().on_init()

    #
    # Channels
    #

    def handle(self, channel):
        if channel == self.path:
            if self.__pcap_reader is None:
                path = channel.read()
                if path:
                    self.set_source(path)

    #
    # Reader
    #

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
                self.__pcap_reader = reader
                #Channel update
                hdr = reader.get_header()
                self.pcap_header.write(hdr)
                self.linktype.write(hdr.network)
                self.packets.clear()
                self.eof.write(0)
                self.log_info("Reading file {name}".format(name=self.__path))
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
        info, pkt = self.read_packet()
        if pkt is None:
            self._read_end()
            return True
        if self.packet_info.write(info) and self.packet.write(pkt):
            self.packets.write()
        else:
            self.log_error("Could not write packet")
        return True

    def _read_end(self):
        stop_time = time.time()
        self.log_info("File {0:s} read - {1:d} packets".format(self.__path, self.packets.read()))
        self.log_debug("took {0:.3f} seconds to read and process {1:d} lines"\
                .format(stop_time - self.get_service_start_time(), self.__pkts))
        self.eof.write(1)
        self.close()
        self.pause()

    def close(self):
        if self.__pcap_reader:
            self.__pcap_reader.close()
            self.__pcap_reader = None
            PcapReader.files_read[self.__path] = (self.packets.read(), self.eof.read() == False)
            self.log_debug("File {} closed".format(self.__path))
