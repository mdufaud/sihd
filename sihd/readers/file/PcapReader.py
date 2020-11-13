#!/usr/bin/python
# coding: utf-8

#
# System
#
import struct
import time
import os

from sihd.utils.pcap import PcapReader as PcapReaderTool
from .AFileReader import AFileReader

class PcapReader(AFileReader):

    def __init__(self, name="PcapReader", app=None, **kwargs):
        super().__init__(app=app, name=name, **kwargs)
        self.reader = None
        self.add_channel("linktype", type='int')
        self.add_channel("pcap_header")
        self.add_channel("packet")
        self.add_channel("packet_info")

    def get_pcap_header(self):
        rd = self.reader
        if rd:
            return rd.get_header()
        return None

    # override
    def open_file(self, path):
        path = os.path.abspath(path) if path else None
        reader = PcapReaderTool()
        try:
            reader.open(path)
        except (FileNotFoundError, IOError) as err:
            self.log_error("Cannot open: {}".format(err))
        if reader.check_magic() is True:
            if reader.check_header() is True:
                self.reader = reader
                #Channel update
                hdr = reader.get_header()
                self.pcap_header.write(hdr)
                self.linktype.write(hdr.network)
                self.log_info("Reading file {name}".format(name=path))
            else:
                self.log_error("Invalid pcap file {}".format(path))
        else:
            self.log_error("Invalid magic {}".format(path))
        return reader.is_open()

    def write(self, info, pkt):
        # Override because multiple channels are involved
        if self.packet_info.write(info) and self.packet.write(pkt):
            self.count.write()
        else:
            self.log_error("Could not write packet")

    # override
    def read_once(self):
        info = None
        pkt = None
        try:
            info, pkt = self.reader.read_pkt()
        except EOFError as e:
            pass
        if pkt:
            self.write(info, pkt)
        else:
            self.read_end()

    # override
    def close_file(self):
        if self.reader:
            self.reader.close()
            self.reader = None
