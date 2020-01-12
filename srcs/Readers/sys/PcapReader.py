#!/usr/bin/python
#coding: utf-8

""" System """
import struct
import time
import os

from sihd.srcs.Readers.IReader import IReader

""" using pcap format ver 2.4 """

class CapInfo:
    def __init__(self, sec=0, usec=0, cap_len=0, orig_len=0):
        self.sec = sec
        self.usec = usec
        self.cap_len = cap_len
        self.orig_len = orig_len

    def __str__(self):
        s = "timestamp: {}.{} - len: {}o (orig: {}o)"\
                .format(self.sec, self.usec, self.cap_len, self.orig_len)
        return s

class CapFileInfo:
    def __init__(self, ver_maj=2, ver_min=4, gmt=0,
                    sigfigs=0, snaplen=65535, network=None):
        self.ver_maj = ver_maj
        self.ver_min = ver_min
        self.gmt = gmt
        self.sigfigs = sigfigs
        self.snaplen = snaplen
        self.network = network

    def __str__(self):
        s = str("Version Major: {}\nVersion Minor: {}\nGMT Correction: {}\n"
            "Timestamp accuracy: {}\nMax length: {}\nData Link: {}").format(
            self.ver_maj, self.ver_min, self.gmt, self.sigfigs, self.snaplen,
            self.network)
        return s

class PcapReader(IReader):
    
    files_read = {}  

    def __init__(self, path=None, app=None, name="PcapReader"):
        super(PcapReader, self).__init__(app=app, name=name)
        self.set_run_method(self.diffuse_pkt)
        self._set_default_conf({
            "path": "/path/to/file",
        })
        self._fully_read = False
        self._reader = None
        self._bytes = 0
        if path:
            self.set_source(path)

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
            bytes_read = self._reader.read(self._to_recover)
            self._to_recover -= bytes_read
        if self._to_recover > 0:
            return False
        s = "Recovered {}".format(self._path)
        self.log_info(s)
        return True

    def __setup_pcap(self, fd):
        magic = fd.read(4)
        if magic == b"\xa1\xb2\xc3\xd4":  # big endian
            endian = ">"
            nano = False
        elif magic == b"\xd4\xc3\xb2\xa1":  # little endian
            endian = "<"
            nano = False
        elif magic == b"\xa1\xb2\x3c\x4d":  # big endian, nanosecond-precision
            endian = ">"
            nano = True
        elif magic == b"\x4d\x3c\xb2\xa1":  # little endian, nanosecond-precision  # noqa: E501
            endian = "<"
            nano = True
        else:
            self.log_error("Bad magic: {}".format(magic))
            return False
        hdr = fd.read(20)
        if len(hdr) < 20:
            self.log_error("Invalid pcap file - hdr size: {}".format(hdr))
            return False
        self._endian = endian
        self._nano = nano
        self._reader = fd
        major, minor, tz, sig, max_len, link_type = struct.unpack(endian + "HHIIII", hdr)
        self._capinfo = CapFileInfo(ver_maj=major, ver_min=minor, gmt=tz,
                            sigfigs=sig, snaplen=max_len, network=link_type)
        self.log_debug(self._capinfo)
        return True

    def set_source(self, path):
        self._path = os.path.abspath(path) if path else None
        self._pkts = 0
        self._to_recover = 0
        if self._reader:
            self._reader.close()
            self._reader = None
        try:
            fd = open(path, 'rb')
        except IOError as err:
            self.notify_error(err)
            self.log_error("Cannot open: {}".format(err))
            return False
        if self.__setup_pcap(fd) is True:
            self._can_recover()
        return True

    def get_cap_info(self):
        return self._capinfo

    def read_pkt(self):
        if not self._reader:
            return None
        read = self._reader.read
        hdr = read(16)
        if len(hdr) < 16:
            return None
        sec, usec, cap_len, wire_len = struct.unpack(self._endian + "IIII", hdr)
        try:
            pkt = read(cap_len)
        except EOFError:
            pkt = None
        self._bytes += cap_len + 16
        cap_info = CapInfo(sec=sec, usec=usec, cap_len=cap_len, orig_len=wire_len)
        return (cap_info, pkt)

    def diffuse_pkt(self):
        if self._to_recover > 0 and self._recover() == False:
            return True
        try:
            info, pkt = self.read_pkt()
        except EOFError as e:
            pkt = None
        if pkt is None:
            self._read_end()
            return False
        self.notify_observers(info, pkt)
        self._pkts += 1
        return True

    def _read_end(self):
        stop_time = time.time()
        self.log_info("File {0:s} read - {1:d} packets".format(self._path, self._pkts))
        self.log_debug("took {0:.3f} seconds to read and process {1:d} lines"\
                .format(stop_time - self._start_time, self._pkts))
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
