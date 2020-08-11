#!/usr/bin/python
# coding: utf-8

import socket
import select

#
# Utils
#

def get_family(family):
    if family == "ipv4":
        return socket.AF_INET
    elif family == "ipv6":
        return socket.AF_INET6
    elif family == "unix":
        return socket.AF_UNIX
    elif family == "unspec":
        return socket.AF_UNSPEC
    elif isinstance(family, str):
        raise ValueError("Socket family '%s' unknown" % family)
    return family

def get_socket_type(type):
    if type == "datagram":
        return socket.SOCK_DGRAM
    elif type == "stream":
        return socket.SOCK_STREAM
    elif type == "raw":
        return socket.SOCK_RAW
    elif isinstance(type, str):
        raise ValueError("Socket type '%s' unknown" % type)
    return type

def get_proto(proto):
    return socket.getprotobyname(proto)

def is_internet(host='google.com'):
    try:
        socket.gethostbyname(host)
        return True
    except socket.gaierror as err:
        pass
    return False

def get_host(host):
    try:
        host = socket.gethostbyname(host)
    except socket.gaierror as err:
        pass
    return host

#
# Socket
#

def make_simple_socket(socktype="datagram", proto="udp", family=socket.AF_INET):
    family = get_family(family)
    socktype = get_socktype(socktype)
    proto = get_proto(proto)
    sock = None
    try:
        sock = socket.socket(family, socktype, proto)
    except OSError as e:
        pass
    return sock

def make_socket(host, port, socket_type, family=socket.AF_UNSPEC, connect=False):
    if host is None or port is None or socket_type is None:
        return
    family = get_family(family)
    socket_type = get_socket_type(socket_type)
    host = get_host(host)
    s = None
    for res in socket.getaddrinfo(host, port, family, socket_type):
        af, socktype, proto, canonname, sa = res
        try:
            sock = socket.socket(af, socktype, proto)
        except OSError as e:
            sock = None
            continue
        if connect is True:
            try:
                sock.connect(sa)
            except Exception as e:
                sock.close()
                sock = None
                continue
        break
    return sock

#
# ICMP
#

import os
import sys
import random
import struct
import time
import array

# From https://github.com/secdev/scapy/blob/master/scapy/utils.py - modified
if sys.byteorder == "little":
    def calculate_checksum(pkt):
        if len(pkt) % 2 == 1:
            pkt += b"\0"
        s = sum(array.array("H", pkt))
        s = (s >> 16) + (s & 0xffff)
        s += s >> 16
        s = ~s
        return s & 0xffff
else:
    def calculate_checksum(pkt):
        if len(pkt) % 2 == 1:
            pkt += b"\0"
        s = sum(array.array("H", pkt))
        s = (s >> 16) + (s & 0xffff)
        s += s >> 16
        s = ~s
        return (((s >> 8) & 0xff) | s << 8) & 0xffff

def ping(host, timeout=1, packet_id=None, seq_id=None):
    sock = send_icmp(host, 8, 0, timeout, packet_id, seq_id)
    delay = rcv_icmp(sock, timeout, packet_id)
    sock.close()
    return delay

def send_icmp(host, type, code, timeout=1, packet_id=None, seq_id=None, data=None):
    # From https://gist.github.com/pklaus/ping.py - modified
    host = get_host(host)
    sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.getprotobyname('icmp'))
    packet_id = packet_id or os.getpid() & 0xFFFF
    seq_id = seq_id or 1
    # Type - Code - Checksum - Id - Seq
    header = struct.pack('!BBHHH', type, code, 0, packet_id, seq_id)
    data = data or b"\x42\xde\xad\xc0\xfe"
    # Dummy header
    my_checksum = calculate_checksum(header + data)
    # Repack with checksum
    header = struct.pack('!BBHHH', type, code, socket.htons(my_checksum), packet_id, seq_id)
    packet = header + data
    while packet:
        # Dummy port for ICMP
        sent = sock.sendto(packet, (host, 42))
        packet = packet[sent:]
    return sock

def rcv_icmp(sock, timeout, packet_id=None, time_sent=None, check_pkt=None):
    time_sent = time_sent or time.time()
    packet_id = packet_id or os.getpid() & 0xFFFF
    # Receive the ping from the socket.
    time_left = timeout
    delay = 0.0
    while True:
        rd, wr, ex = select.select([sock], [], [], time_left)
        if not rd: # Timeout
            break
        time_received = time.time()
        rec_packet, addr = sock.recvfrom(1024)
        icmp_header = rec_packet[20:28]
        type, code, checksum, p_id, s_id = struct.unpack('!BBHHH', icmp_header)
        if (check_pkt is None and p_id == packet_id) or check_pkt(type, code, checksum, p_id, s_id):
            delay = time_received - time_sent
            break
        time_left -= time_received - time_sent
        if time_left <= 0:
            break
    return delay * 1E3

#
# UDP
#

def make_udp_socket(host, port):
    return make_socket(host, port, socket.SOCK_DGRAM, socket.AF_UNSPEC)

def send_udp(host, port, data):
    host = get_host(host)
    sock = make_udp_socket(host, port)
    if sock is None:
        return False
    if not isinstance(data, bytes):
        data = data.encode()
    while data:
        sent = sock.sendto(data, (host, port))
        data = data[sent:]
    sock.close()
    return True

#
# TCP
#

def make_tcp_socket(host, port):
    return make_socket(host, port, socket.SOCK_STREAM, socket.AF_UNSPEC, connect=True)

def send_tcp(host,port, data):
    sock = make_tcp_socket(host, port)
    if sock is None:
        return False
    if not isinstance(data, bytes):
        data = data.encode()
    sock.sendall(data)
    sock.close()
    return True

#
# Server
#

def build_server(port, socktype="datagram", family="ipv4", host=None, max_co=5):
    host = host or 'localhost'
    socktype = get_socket_type(socktype)
    family = get_family(family)
    sock = socket.socket(family, socktype)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setblocking(0)
    except (OSError, socket.error) as e:
        sock.close()
        raise e
    try:
        addr = (host, port)
        sock.bind(addr)
    except (OSError, TypeError, socket.error) as e:
        sock.close()
        raise e
    if socktype == socket.SOCK_STREAM:
        try:
            sock.listen(max_co)
        except (OSError, socket.error) as e:
            sock.close()
            raise e
    return sock

class Server(object):

    def __init__(self, port, *args, on_read=None, on_write=None, on_exceptional=None, **kwargs):
        self.socket = build_server(port, *args, **kwargs)
        self.inputs = [self.socket]
        self.outputs = []
        self.exceptional = []
        self.port = port
        if on_read:
            self.on_read = on_read
        if on_write:
            self.on_write = on_write
        if on_exceptional:
            self.on_exceptional = on_exceptional

    def accept(self):
        sock = self.socket
        return sock and sock.accept() or None

    def close(self):
        sock = self.socket
        if sock:
            sock.close()

    def select(self, timeout=None, loop=1, on_read=None, on_write=None, on_exceptional=None, stop=None):
        on_read = on_read or self.on_read
        on_write = on_write or self.on_write
        on_exceptional = on_exceptional or self.on_exceptional
        start_time = time.time()
        remaining = timeout
        while True:
            rd, wr, ex = select.select(self.inputs, self.outputs, self.exceptional, remaining)
            if rd:
                on_read(rd)
            if wr:
                on_write(wr)
            if ex:
                on_exceptional(ex)
            if stop is not None and stop() is True:
                break
            if timeout is not None:
                remaining -= start_time - time.time()
                if remaining <= 0:
                    break
            loop = loop - 1
            if loop == 0:
                break

    def on_read(self, readable):
        pass

    def on_write(self, writable):
        pass

    def on_exceptional(self, exceptionnal):
        pass
