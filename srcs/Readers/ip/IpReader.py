#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import time
import os
import socket
import select
import Queue

from sihd.srcs.Readers.IReader import IReader

class IpReader(IReader):
    
    def __init__(self, port=None, app=None, name="IpReader"):
        super(IpReader, self).__init__(app=app, name=name)
        self._socket = None
        if port >= 0:
            self.setup_server(port)
        self._inputs = []
        self._outputs = []
        self._clients = {}
        self._set_default_conf({
            "port": 42042,
            "max_co": 5,
            "rcv_buf": 4096,
        })
        self.set_run_method(self._do_select)

    """ IConfigurable """

    def _load_conf_impl(self):
        max_co = self.get_conf_val("max_co")
        if max_co:
            self._max_co = int(max_co)
        rcv_buf = self.get_conf_val("rcv_buf")
        if rcv_buf:
            self._rcv_buf = rcv_buf
        if self._socket is None:
            port = self.get_conf_val("port")
            self.setup_server(port)
        return True

    """ Reader """

    def is_up(self):
        return self._socket is not None

    def send(self, co, msg):
        client = self._clients.get(co, None)
        if client is None:
            self.log_error("No such client {}".format(co))
            return False
        client.queue.put(msg)
        if co not in self._outputs:
            self._outputs.append(co)
        return True

    def setup_server(self, port):
        if self.is_up():
            return True
        socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if not socket:
            return False
        try:
            socket.setblocking(0)
            socket.bind((socket.gethostname(), port))
            socket.listen(self._max_co)
        except OSError as e:
            self.log_error("Error setting up server: {}".format(e))
            self.stop_server(False)
            return False
        self._start_time = time.time()
        self._socket = socket
        self._port = port
        self._inputs = [socket]
        return True

    def get_server(self):
        return self._socket

    def _accept_client(self):
        co, client_addr = self._socket.accept()
        self.log_debug("New connection from {}".format(client_addr))
        con.setblocking(0)
        self._inputs.append(co)
        self._clients[co] = {
            "addr": client_addr,
            "socket": co,
            "msg": 0,
            "queue": Queue.Queue(),
        }

    def _remove_client(self, co):
        self._inputs.remove(co)
        if co in self._outputs:
            self._outputs.remove(co)
        client = self._clients[co]
        self.log_debug("Client {} has left".format(client.addr))
        self._clients[co] = {}
        co.close()

    def _read_client(self, co):
        client = self._clients[co]
        serv = self._socket
        data = serv.recv(self._rcv_buf)
        if data:
            client.msg += 1
            self.notify_observers((data, co))
        else:
            self._remove_client(co)

    def __do_write(self, writable):
        for sock in writable:
            client = self._clients.get(co, None)
            if client is None:
                continue
            try:
                next_msg = client.queue.get_nowait()
            except Queue.Empty:
                outputs.remove(co)
                continue
            total = len(next_msg)
            sent = 0
            while total >= sent:
                ret = server.send(next_msg)
                if ret < 0:
                    err = "Error sending to client {}".format(client.addr)
                    self.log_error(err)
                    self.notify_error(err)
                    break
                sent += ret

    def _do_select(self):
        server = self._socket
        outputs = self._outputs
        inputs = self._inputs
        readable, writable, exceptional = select.select(inputs,
                                                        outputs,
                                                        inputs)
        if exceptional:
            err = "Exceptional: {}".format(exceptional)
            self.log_error(err)
            self.notify_error(err)
            return False
        for sock in readable:
            if sock is server:
                self._accept_client()
            else:
                self._read_client()
        self.__do_write(writable)
        return True

    def stop_server(self, do_time=False):
        if self.is_up():
            self._socket.close()
            self._socket = None
        if do_time:
            stop_time = time.time()
            self.notify_info("Server has shut down")
            self.log_info("Server was up for {0:.3f} seconds"\
                    .format(stop_time - self._start_time))

    """ IService """

    def _start_impl(self):
        if self._socket is None:
            self.log_error("Server is not up")
            return False
        self.setup_thread()
        self._start_time = time.time()
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
