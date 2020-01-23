#!/usr/bin/python
#coding: utf-8

import base64 as b64
import time
import struct

from sihd.srcs.Tools.network.const import DLT_EN10MB

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

class PcapReader:
    def __init__(self):
        self.endian = None
        self.nano = None
        self.fd = None
        self.capinfo = None

    def is_open(self):
        return self.fd is not None

    def open(self, filename, mode='rb'):
        self.fd = open(filename, mode)
        self.capinfo = None
        self.__header = False
        self.__magic = False

    def check_magic(self):
        if self.fd is None:
            return False
        if self.__magic:
            return True
        magic = self.fd.read(4)
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
            self.close()
            return False
        self.endian = endian
        self.nano = nano
        self.__magic = True
        return True

    def check_header(self):
        fd = self.fd
        if fd is None or not self.__magic:
            return False
        if self.__header:
            return True
        hdr = fd.read(20)
        if len(hdr) < 20:
            self.close()
            return False
        major, minor, tz, sig, max_len, link_type = struct.unpack(self.endian + "HHIIII", hdr)
        self.capinfo = CapFileInfo(ver_maj=major, ver_min=minor, gmt=tz,
                            sigfigs=sig, snaplen=max_len, network=link_type)
        self.__header = True
        return True

    def read_pkt(self):
        fd = self.fd
        pkt = None
        cap_info = None
        if fd and self.__header:
            hdr = fd.read(16)
            if len(hdr) == 16:
                sec, usec, cap_len, wire_len = struct.unpack(self.endian + "IIII", hdr)
                cap_info = CapInfo(sec=sec, usec=usec, cap_len=cap_len, orig_len=wire_len)
                try:
                    pkt = fd.read(cap_len)
                except EOFError:
                    pkt = None
        return cap_info, pkt

    def close(self):
        fd = self.fd
        if fd:
            fd.close()
        self.fd = None

    def read_all(self, filename, mode='rb'):
        lst = None
        self.open(filename, mode)
        ret = self.check_magic()
        ret = ret and self.check_header()
        if ret:
            lst = []
            while True:
                cap_info, pkt = self.read_pkt()
                if pkt is None:
                    break
                lst.apppend((cap_info, pkt))
        self.close()
        return lst

    def get_header(self):
        return self.capinfo

class PcapWriter:
    def __init__(self, endian, linktype=DLT_EN10MB, nano=False):
        endian = endian.lower()
        if endian == "big":
            endian = ">"
        elif endian == "little":
            endian = "<"
        else:
            raise ValueError("Endianness is either 'big' or 'little' not '{}'"\
                                .format(endian))
        self.endian = endian
        self.nano = bool(nano)
        self.linktype = linktype
        self.pkt_lst = []

    def get_pkts(self):
        return self.pkt_lst

    def reset(self):
        self.pkt_lst = []

    @staticmethod
    def encode_data(data, base64=False, encoding='utf-8'):
        s = str(data)
        binary = s.encode(encoding)
        if base64:
            binary = b64.b64encode(binary)
        return binary

    def make_pkt(self, data, sec=None, usec=None, caplen=None, wirelen=None):
        if sec is None or usec is None:
            t = time.time()
            if sec is None:
                sec = int(t)
                usec = int(round(t - sec) * (1e9 if self.nano else 1e6))
            elif usec is None:
                usec = 0
        if not isinstance(data, bytes):
            data = self.encode_data(data)
        if caplen is None:
            caplen = len(data)
        if wirelen is None:
            wirelen = caplen
        hdr = struct.pack(self.endian + "IIII", sec, usec, caplen, wirelen)
        pkt = data
        return hdr, pkt

    def add_pkt(self, data, sec=None, usec=None, caplen=None, wirelen=None):
        hdr, pkt = self.make_pkt(data, sec, usec, caplen, wirelen)
        self.pkt_lst.append((hdr, pkt))

    def get_header(self):
        #E501 ver 2.4
        hdr = struct.pack(self.endian + "IHHIIII",
                        0xa1b23c4d if self.nano else 0xa1b2c3d4,
                        2, 4, 0, 0, 0xffff, self.linktype)
        return hdr

    def write_pcap(self, filename, mode='wb'):
        with open(filename, mode) as fd:
            if mode[0] == 'w':
                hdr = self.get_header()
                fd.write(hdr)
                fd.flush()
            for pktinfo in self.pkt_lst:
                hdr, pkt = pktinfo
                fd.write(hdr)
                fd.write(pkt)
