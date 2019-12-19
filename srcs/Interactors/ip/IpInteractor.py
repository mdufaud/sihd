#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import socket

from sihd.srcs.Interactors.IInteractor import IInteractor

class IpInteractor(IInteractor):

    def __init__(self, app=None, name="IpInteractor"):
        super(IpInteractor, self).__init__(app=app, name=name)
        self._set_default_conf({
            "port": 42042,
            "host": "localhost",
            "type": "tcp",
            "response_buff": 4096,
        })

    """ IConfigurable """

    def _load_conf_impl(self):
        port = self.get_conf_val("port")
        if port:
            self._port = int(port)
        host = self.get_conf_val("host")
        if host:
            self._host = host
        t = self.get_conf_val("type")
        if type:
            self._type = t
        reponse_buff = self.get_conf_val("reponse_buff")
        if reponse_buff:
            self._reponse_buff = reponse_buff
        return True

    """ Interactor """

    def send_udp(self, host, port, data, response=False):
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
            if response is True:
                data, server = s.recvfrom(self._response_buff)
                return data
            sent = True
        except Exception as e:
            self.log_error("Could not send data: {}".format(e))
        s.close()
        return sent


    def send_tcp(self, host, port, data, response=False):
        sent = False
        s = None
        for res in socket.getaddrinfo(self._host, self._port,
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
        if s is None:
            self.log_error("Could not open socket for '{}:{}'".format(
                            self._host, self._port))
            return False
        try:
            s.sendall(data)
            if response is True:
                return s.recv(self._response_buff)
            sent = True
        except Exception as e:
            self.log_error("Could not send data: {}".format(e))
        s.close()
        return sent

    def interact(self, data, *args, **kwargs):
        t = self._type
        if t == "tcp":
            return self.send_tcp(self._host, self._port, data, *args, **kwargs)
        elif t == "udp":
            return self.send_udp(self._host, self._port, data, *args, **kwargs)
        return False
