#!/usr/bin/python
#coding: utf-8

#
# System
#
import socket

from sihd.Interactors.AInteractor import AInteractor

class IpInteractor(AInteractor):

    def __init__(self, name="IpInteractor", app=None):
        super(IpInteractor, self).__init__(app=app, name=name)
        self.set_default_conf({
            "port": 42042,
            "host": "localhost",
            "protocol": "tcp",
        })
        self.__tcp_socket = None
        self.__host = ""
        self.__port = 0
        self.add_channel_input("c_port", type='int')
        self.add_channel_input("c_host", type='queue')
        self.add_channel_input("c_protocol", type='queue')

    #
    # Configuration
    #

    def on_setup(self):
        ret = super().on_setup()
        self.__port = int(self.get_conf("port"))
        self.__host = str(self.get_conf("host"))
        self.__protocol = str(self.get_conf("protocol"))
        return ret

    #
    # Channels
    #

    def handle(self, channel):
        if channel == self.c_port:
            port = channel.read()
            if port:
                self.__port = p
                self.close_tcp()
        elif channel == self.c_host:
            host = channel.read()
            if host:
                self.__host = host
                self.close_tcp()
        elif channel == self.c_type:
            t = channel.read()
            if t:
                self.__protocol = t
                self.close_tcp()

    #
    # Interactor
    #

    def on_new_interaction(self, action):
        return action.encode()

    def do_interaction(self, data, *args, **kwargs):
        p = self.__protocol
        if p == "tcp":
            return self.send_tcp_once(data, *args, **kwargs)
        elif p == "udp":
            return self.send_udp(self.__host, self.__port, data, *args, **kwargs)
        else:
            self.log_error("Protocol {} not recognized".format(p))
        return False

    #
    # UDP
    #

    def send_udp(self, host, port, data):
        sent = False
        s = None
        for res in socket.getaddrinfo(self.__host, self.__port,
                                        socket.AF_UNSPEC,
                                        socket.SOCK_DGRAM):
            af, socktype, proto, canonname, sa = res
            try:
                s = socket.socket(af, socktype, proto)
            except OSError as e:
                s = None
                continue
            break
        if s is None:
            self.log_error("Could not open socket for '{}:{}'".format(
                            self.__host, self.__port))
            return False
        try:
            s.sendto(data, (self.__host, self.__port))
            sent = True
        except Exception as e:
            self.log_error("Could not send data: {}".format(e))
        s.close()
        return sent

    #
    # TCP
    #

    def close_tcp(self):
        if self.is_tcp_connected():
            self.__tcp_socket.close()
            self.__tcp_socket = None

    def is_tcp_connected(self):
        return self.__tcp_socket is not None

    def connect_tcp(self, host, port):
        s = self.__tcp_socket
        if s:
            return s
        for res in socket.getaddrinfo(host, port,
                                        socket.AF_UNSPEC,
                                        socket.SOCK_STREAM):
            af, socktype, proto, canonname, sa = res
            try:
                s = socket.socket(af, socktype, proto)
            except OSError as e:
                s = None
                continue
            try:
                s.connect(sa)
            except Exception as e:
                s.close()
                s = None
                continue
            break
        if s:
            self.__host = host
            self.__port = port
            self.__tcp_socket = s
        else:
            self.log_error("Could not open socket for '{}:{}'".format(host, port))
        return s

    def send_tcp(self, data, *args, **kwargs):
        sent = False
        s = self.__tcp_socket
        if not s:
            self.log_error("No socket opened")
            return False
        try:
            s.sendall(data)
            sent = True
        except Exception as e:
            self.log_error("Could not send data: {}".format(e))
        return sent

    def send_tcp_once(self, data, *args, **kwargs):
        ret = False
        self.close_tcp()
        if self.connect_tcp(self.__host, self.__port) is not None:
            ret = self.send_tcp(data, *args, **kwargs)
            self.close_tcp()
            return ret
        return ret
