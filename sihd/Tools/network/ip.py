#!/usr/bin/python
# coding: utf-8

import socket
import select

#
# ICMP
#

import os
import sys
import random
import struct
import time
import array

def make_socket(host, port, socket_type, family=socket.AF_UNSPEC, connect=False):
    if host is None or port is None or socket_type is None:
        return
    host = socket.gethostbyname(host)
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
    host = socket.gethostbyname(host)
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

def send_udp(host, port, data):
    host = socket.gethostbyname(host)
    sock = make_socket(host, port, socket.SOCK_DGRAM, socket.AF_UNSPEC)
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

def send_tcp(data, sock=None, host=None, port=None):
    sock = sock or make_tcp_socket(host, port)
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

def get_protocol(type):
    if type == "ipv4":
        return socket.AF_INET
    elif type == "ipv6":
        return socket.AF_INET6
    elif type == "unix":
        return socket.AF_UNIX
    elif type == "unspec":
        return socket.AF_UNSPEC
    return None

def make_socket_type(type):
    if type == "udp":
        return socket.SOCK_DGRAM
    elif type == "tcp":
        return socket.SOCK_STREAM
    elif type == "raw":
        return socket.SOCK_RAW
    return None

def build_server(port, socktype="udp", protocol="ipv4", host=None, max_co=5):
    host = host or 'localhost'
    socktype = make_socket_type(socktype)
    protocol = get_protocol(protocol)
    sock = socket.socket(protocol, socktype)
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

    def select(self, timeout=None, loop=1, on_read=None, on_write=None, on_exceptional=None):
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
