#!/usr/bin/python
#coding: utf-8

""" System """


import socket

from sihd.Interactors.IInteractor import IInteractor

class IpInteractor(IInteractor):

    def __init__(self, app=None, name="IpInteractor"):
        super(IpInteractor, self).__init__(app=app, name=name)
        self._set_default_conf({
            "port": 42042,
            "host": "localhost",
            "type": "tcp",
        })
        self._tcp_socket = None
        self._host = ""
        self._port = 0

    """ IConfigurable """

    def do_setup(self):
        ret = super().do_setup()
        self._port = int(self.get_conf("port"))
        self._host = str(self.get_conf("host"))
        self._type = str(self.get_conf("type"))
        return ret

    """ IInteractor """

    def on_new_interaction(self, action):
        return action.encode()

    def on_interaction(self, data, *args, **kwargs):
        t = self._type
        if t == "tcp":
            return self.send_tcp_once(data, *args, **kwargs)
        elif t == "udp":
            return self.send_udp(self._host, self._port, data, *args, **kwargs)
        return False

    """ UDP """

    def send_udp(self, host, port, data):
        sent = False
        s = None
        for res in socket.getaddrinfo(self._host, self._port,
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
                            self._host, self._port))
            return False
        try:
            s.sendto(data, (self._host, self._port))
            sent = True
        except Exception as e:
            self.log_error("Could not send data: {}".format(e))
        s.close()
        return sent

    """ TCP """

    def close_tcp(self):
        if self.is_tcp_connected():
            self._tcp_socket.close()
            self._tcp_socket = None

    def is_tcp_connected(self):
        return self._tcp_socket is not None

    def connect_tcp(self, host, port):
        s = self._tcp_socket
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
            self._host = host
            self._port = port
            self._tcp_socket = s
        else:
            self.log_error("Could not open socket for '{}:{}'".format(host, port))
        return s

    def send_tcp(self, data, *args, **kwargs):
        sent = False
        s = self._tcp_socket
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
        if self.connect_tcp(self._host, self._port) is not None:
            ret = self.send_tcp(data, *args, **kwargs)
            self.close_tcp()
            return ret
        return ret
