#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import time
import os
import socket
import select

from sihd.srcs.Readers.IReader import IReader

class IpReader(IReader):
    
    def __init__(self, app=None, name="IpReader"):
        super(IpReader, self).__init__(app=app, name=name)
        self._socket = None
        self._inputs = []
        self._outputs = []
        self._listening = False
        self._set_default_conf({
            "port": 42042,
            "max_co": 5,
            "type": "tcp",
            "protocol": "ipv4",
        })
        self.set_run_method(self._do_select)

    """ IConfigurable """

    def _load_conf_impl(self):
        max_co = self.get_conf_val("max_co")
        if max_co:
            self._max_co = int(max_co)
        socket = self.get_conf_val("type")
        if socket:
            self._socket_type = IpReader.get_socket_type(socket)
        protocol = self.get_conf_val("protocol")
        if protocol:
            self._protocol = IpReader.get_protocol(protocol)
        if self._socket is None:
            port = self.get_conf_val("port")
            self.set_source(port)
        return True

    """ Getters """

    @staticmethod
    def get_protocol(type):
        if type == "ipv4":
            return socket.AF_INET
        elif type == "ipv6":
            return socket.AF_INET6
        elif type == "unix":
            return socket.AF_UNIX
        return None

    @staticmethod
    def get_socket_type(type):
        if type == "udp":
            return socket.SOCK_DGRAM
        elif type == "tcp":
            return socket.SOCK_STREAM
        elif type == "raw":
            return socket.SOCK_RAW
        return None

    def is_raw(self):
        return self._socket_type == socket.SOCK_RAW

    def is_tcp(self):
        return self._socket_type == socket.SOCK_STREAM

    def is_udp(self):
        return self._socket_type == socket.SOCK_DGRAM

    def get_serv_addr(self):
        return ("localhost", self._port)

    def get_server(self):
        return self._socket

    """ Select utilities """

    def add_input(self, co):
        self._inputs.append(co)

    def add_output(self, co):
        self._outputs.append(co)

    def remove_input(self, co):
        self._inputs.remove(co)

    def remove_output(self, co):
        if co in self._outputs:
            self._outputs.remove(co)

    """ Reader """

    def is_up(self):
        return self._socket is not None

    def set_source(self, port):
        if self.is_up():
            return True
        self._port = port
        sock = socket.socket(self._protocol, self._socket_type)
        if not sock:
            self.log_error("Could not open socket")
            return False
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setblocking(0)
        except (OSError, socket.error) as e:
            self.log_error("Error sock opt server: {}".format(e))
            self.stop_server(False)
            return False
        try:
            addr = self.get_serv_addr()
            self.log_debug("Binding on {}:{}".format(addr[0], addr[1]))
            sock.bind(addr)
        except (OSError, socket.error) as e:
            self.log_error("Error bind server: {}".format(e))
            self.stop_server(False)
            return False
        self._listening = False
        if self._socket_type == socket.SOCK_STREAM:
            try:
                sock.listen(self._max_co)
                self._listening = True
            except (OSError, socket.error) as e:
                self.log_error("Error listen server: {}".format(e))
                self.stop_server(False)
                return False
        self._start_time = time.time()
        self._socket = sock
        self._inputs = [sock]
        self._outputs = []
        return True

    def stop_server(self, do_time=False):
        if self.is_up():
            self._inputs = []
            self._outputs = []
            self._socket.close()
            self._socket = None
        if do_time:
            stop_time = time.time()
            self.notify_info("Server has shut down")
            self.log_info("Server was up for {0:.3f} seconds"\
                    .format(stop_time - self._start_time))

    """ Select """

    def _do_select(self):
        server = self._socket
        outputs = self._outputs
        inputs = self._inputs
        readable, writable, exceptional = select.select(inputs,
                                                        outputs,
                                                        inputs,
                                                        1)
        if exceptional:
            err = "Exceptional: {}".format(exceptional)
            self.log_error(err)
            self.notify_error(err)
            return False
        if self.is_active() and (readable or writable):
            try:
                self.notify_observers(readable, writable)
            except Exception as e:
                self.stop()
                raise
        return True

    """ IService """

    def _start_impl(self):
        if self._socket is None:
            self.log_error("Server is not up")
            return False
        self.setup_thread()
        self._start_time = time.time()
        if self._listening is True:
            s = "Accepting connections ({} max)".format(self._max_co)
            self.log_info(s)
            self.notify_info(s)
        self.start_thread()
        return True

    def _stop_impl(self):
        self.stop_server(True)
        self.stop_thread()
        return True

    def _pause_impl(self):
        self.pause_thread()
        return True

    def _resume_impl(self):
        self.resume_thread()
        return True
