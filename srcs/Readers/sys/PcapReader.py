#!/usr/bin/python
#coding: utf-8

""" System """
import struct
import time
import os

from sihd.srcs.Readers.IReader import IReader
from sihd.srcs.Tools.pcap import PcapReader as PcapReaderTool

class PcapReader(IReader):
    
    files_read = {}  

    def __init__(self, app=None, name="PcapReader"):
        super(PcapReader, self).__init__(app=app, name=name)
        self.set_run_method(self.diffuse_pkt)
        self._set_default_conf({
            "path": "/path/to/file",
        })
        self._fully_read = False
        self._reader = None
        self._bytes = 0
        self.__last_pkt_info = None

    """ IConfigurable """

    def _setup_impl(self):
        super(PcapReader, self)._setup_impl()
        path = self.get_conf("path", default=False)
        if path:
            self.set_source(path)
        if not self._reader:
            return False
        return True

    """ Reader """

    def _can_recover(self):
        if self._path not in PcapReader.files_read:
            return
        tupl = PcapReader.files_read[self._path]
        if tupl[1] == False:
            s = "File {} already read".format(self._path)
            self.log_info(s)
            return
        self._to_recover = tupl[0]
        s = "To recover {}".format(self._to_recover)
        self.log_debug(s)

    def _recover(self):
        while self.is_active() and self._to_recover > 0:
            bytes_read = self._reader.fd(self._to_recover)
            self._to_recover -= bytes_read
        if self._to_recover > 0:
            return False
        s = "Recovered {}".format(self._path)
        self.log_info(s)
        return True

    def set_source(self, path):
        self._path = os.path.abspath(path) if path else None
        self._pkts = 0
        self._to_recover = 0
        reader = PcapReaderTool()
        try:
            reader.open(path)
        except (FileNotFoundError, IOError) as err:
            self.log_error("Cannot open: {}".format(err))
        if reader.check_magic() is True:
            if reader.check_header() is True:
                self.__last_pkt_info = None
                if self._reader:
                    self._reader.close()
                self._reader = reader
                self._can_recover()
            else:
                self.log_error("Invalid pcap file {}".format(path))
        else:
            self.log_error("Invalid magic {}".format(path))
        return reader.is_open()

    def get_pcap_header(self):
        rd = self._reader
        if rd:
            return rd.get_header()
        return None

    def get_pkt_info(self):
        return self.__last_pkt_info

    def diffuse_pkt(self):
        if self._to_recover > 0 and self._recover() == False:
            return True
        try:
            info, pkt = self._reader.read_pkt()
        except EOFError as e:
            pkt = None
        if pkt is None:
            self._read_end()
            return False
        self.__last_pkt_info = info
        self.notify_observers(pkt)
        self._pkts += 1
        return True

    def _read_end(self):
        stop_time = time.time()
        self.log_info("File {0:s} read - {1:d} packets".format(self._path, self._pkts))
        self.log_debug("took {0:.3f} seconds to read and process {1:d} lines"\
                .format(stop_time - self.get_thread_start_time(), self._pkts))
        self._fully_read = True
        PcapReader.files_read[self._path] = (self._bytes, False)
        self.stop()

    """ IService """

    def _start_impl(self):
        if self._reader is None:
            self.log_error("No reader has been set")
            return False
        s = "Reading file {name}".format(name=self._path)
        self.log_info(s)
        return super(PcapReader, self)._start_impl()

    def _stop_impl(self):
        if self._reader:
            self._reader.close()
            self._reader = None
        PcapReader.files_read[self._path] = (self._pkts, self._fully_read == False)
        return super(PcapReader, self)._stop_impl()
