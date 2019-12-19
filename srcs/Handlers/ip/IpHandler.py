#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

try:
    import Queue
except ImportError:
    import queue
    Queue = queue

from sihd.srcs.Handlers.IHandler import IHandler

class IpHandler(IHandler):

    def __init__(self, app=None, name="IpHandler"):
        super(IpHandler, self).__init__(app=app, name=name)
        self._reader = None
        self._clients = {}
        self._set_default_conf({
            "rcv_buf": 4096,
        })


    """ IConfigurable """

    def _load_conf_impl(self):
        rcv_buf = self.get_conf_val("rcv_buf")
        if rcv_buf:
            self._rcv_buf = int(rcv_buf)
        return True

    """ IObservable """

    def handle(self, reader, readable, writable):
        ret = self.__do_read(reader, readable)
        ret = ret and self.__do_write(reader, writable)
        return ret

    """ Read part """

    def _accept_client(self, reader):
        co, addr = reader.get_server().accept()
        self.log_debug("New connexion from {}:{}".format(addr[0], addr[1]))
        co.setblocking(0)
        reader.add_input(co)
        self._clients[co] = {
            "addr": addr,
            "socket": co,
            "msg": 0,
            "queue": Queue.Queue(),
        }

    def _remove_client(self, reader, co):
        reader.remove_input(co)
        reader.remove_output(co)
        client = self._clients[co]
        self.log_debug("Client {} has left".format(client["addr"]))
        self._clients[co] = {}
        co.close()

    def _read_tcp_client(self, reader, co):
        client = self._clients[co]
        data = co.recv(self._rcv_buf)
        if data:
            client["msg"] += 1
            self._reader = reader
            self.notify_observers(data, co)
            self._reader = None
        else:
            self._remove_client(reader, co)

    def _read_client(self, reader, co):
        data, server = co.recvfrom(self._rcv_buf)
        if data:
            self._reader = reader
            self.notify_observers(data, co)
            self._reader = None
        else:
            self._remove_client(reader, co)

    def __do_read(self, reader, readable):
        ret = True
        server = reader.get_server()
        for sock in readable:
            if sock is server:
                if reader.is_tcp():
                    ret = self._accept_client(reader)
                elif reader.is_udp():
                    ret = self._read_client(reader, sock)
            else:
                ret = self._read_tcp_client(reader, sock)
            if ret == False:
                break
        return ret

    """ Sender part """

    def send(self, co, msg):
        client = self._clients.get(co, None)
        if client is None:
            self.log_error("No such client {}".format(co))
            return False
        client.queue.put(msg)
        if co not in self._outputs:
            self._reader.add_output(co)
        return True

    def __do_write(self, reader, writable):
        server = self._socket
        for sock in writable:
            client = self._clients.get(co, None)
            if client is None:
                continue
            try:
                next_msg = client.queue.get_nowait()
            except Queue.Empty:
                outputs.remove(co)
                continue
            try:
                server.sendall(next_msg)
            except Exception as e:
                err = "Error sending to client {}".format(client.addr)
                self.log_error(err)
                self.notify_error(err)
                continue
        return True

    """ IService """

    def _start_impl(self):
        self._clients = {}
        return True

    def _stop_impl(self):
        self._clients = {}
        return True

    def _pause_impl(self):
        return True

    def _resume_imp(self):
        return True
