#!/usr/bin/python
#coding: utf-8

#
# System
#
import time
import os
import socket
import select

try:
    import Queue
except ImportError:
    import queue
    Queue = queue

from sihd.Readers.AReader import AReader

class IpReader(AReader):
    
    def __init__(self, name="IpReader", app=None):
        super(IpReader, self).__init__(app=app, name=name)
        self._clients = {}
        self._socket = None
        self._inputs = []
        self._outputs = []
        self._listening = False
        self.set_default_conf({
            "port": 42042,
            "max_connexions": 5,
            "protocol": "tcp",
            "sock_type": "ipv4",
            "rcv_buf": 4096,
            "poll_timeout": 0.1,
        })
        self.add_channel_input("server_action", type='queue')
        self.add_channel_input("packet_send", type='queue')
        self.add_channel_output("packet_data", type='queue')
        self.add_channel_output("client_info", type='queue')
        self.add_channel_output("server_msg", type='queue')

    #
    # Configuration
    #

    def on_setup(self):
        ret = super().on_setup()
        self._max_co = int(self.get_conf("max_connexions"))
        self._protocol = self.get_socket_type(self.get_conf("protocol"))
        self._sock_type = self.get_protocol(self.get_conf("sock_type"))
        self._rcv_buf = int(self.get_conf("rcv_buf"))
        self._to = float(self.get_conf("poll_timeout"))
        self.set_source(int(self.get_conf("port")))
        return True

    #
    # Channels
    #

    def handle(self, channel):
        if self.is_tcp() and channel == self.packet_send:
            i = 0
            max_iter = 10
            while channel.is_readable() or i >= max_iter:
                msg, co = channel.read()
                self.buff_send(msg, co)
                i += 1

    #
    # Getters
    #

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
        return self._protocol == socket.SOCK_RAW

    def is_tcp(self):
        return self._protocol == socket.SOCK_STREAM

    def is_udp(self):
        return self._protocol == socket.SOCK_DGRAM

    def get_serv_addr(self):
        return ("localhost", self._port)

    def get_server(self):
        return self._socket

    #
    # Select utilities
    #

    def add_server_input(self, co):
        self._inputs.append(co)

    def add_server_output(self, co):
        self._outputs.append(co)

    def remove_server_input(self, co):
        self._inputs.remove(co)

    def remove_server_output(self, co):
        if co in self._outputs:
            self._outputs.remove(co)

    #
    # Reader
    #

    def is_up(self):
        return self._socket is not None

    def set_source(self, port):
        if self.is_up():
            return True
        self._port = int(port)
        sock = socket.socket(self._sock_type, self._protocol)
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
        except (OSError, TypeError, socket.error) as e:
            self.log_error("Error bind server ({}): {}".format(addr, e))
            self.stop_server(False)
            return False
        self._listening = False
        if self._protocol == socket.SOCK_STREAM:
            try:
                sock.listen(self._max_co)
                self._listening = True
            except (OSError, socket.error) as e:
                self.log_error("Error listen server: {}".format(e))
                self.stop_server(False)
                return False
        self._socket = sock
        self._inputs = [sock]
        self._outputs = []
        return True

    def stop_server(self, do_time=False):
        if self.is_up():
            self._clients = {}
            self._inputs = []
            self._outputs = []
            self._socket.close()
            self._socket = None
        if do_time:
            stop_time = time.time()
            self.log_info("Server was up for {0:.3f} seconds"\
                    .format(stop_time - self.get_service_start_time()))

    #
    # Select
    #

    def on_step(self):
        server = self.get_server()
        inputs = self._inputs
        # readable, writable, exceptionnal
        r, w, ex = select.select(inputs, self._outputs,
                                inputs, self._to)
        if ex:
            self.log_error("Select exceptional: {}".format(ex))
            return False
        if self.is_active():
            if r:
                self._do_read(r)
            if w:
                self._do_write(w)
        return True

    #
    # Read part
    #

    def get_client(self, co):
        if isinstance(co, int):
            return self._clients.get(co, None)
        return self._clients.get(co.fileno(), None)

    def _accept_client(self):
        co, addr = self.get_server().accept()
        self.log_debug("New connexion from {}:{}".format(addr[0], addr[1]))
        co.setblocking(0)
        fileno = co.fileno()
        self.add_server_input(co)
        self._clients[fileno] = {
            "addr": addr,
            "socket": co,
            "msg": 0,
            "queue": Queue.Queue(),
        }
        self.client_info.write((fileno, "co", True))
        self.client_info.write((fileno, "addr", True))

    def _remove_client(self, co):
        self.remove_server_input(co)
        self.remove_server_output(co)
        client = self.get_client(co)
        self.log_debug("Client {} has left".format(client["addr"]))
        self._clients[co.fileno()] = None
        co.close()
        self.client_info.write((co.fileno(), "co", False))

    def _read_tcp_client(self, co):
        client = self.get_client(co)
        data = co.recv(self._rcv_buf)
        if data:
            client["msg"] += 1
            self.packet_data.write((data.decode(), co.fileno()))
        else:
            self._remove_client(co)

    def _read_udp_packet(self, co):
        data, server = co.recvfrom(self._rcv_buf)
        if data:
            self.packet_data.write((data.decode(), server))

    def _do_read(self, readable):
        ret = True
        server = self.get_server()
        for sock in readable:
            if sock is server:
                if self.is_tcp():
                    ret = self._accept_client()
                elif self.is_udp():
                    ret = self._read_udp_packet(sock)
            else:
                ret = self._read_tcp_client(sock)
            if ret == False:
                break

    #
    # Sender part
    #

    def buff_send(self, msg, co):
        client = self.get_client(co)
        if client is None:
            self.log_debug("No such client {} to send to".format(co))
            return False
        #TODO set actions like kick
        client['queue'].put(msg)
        if isinstance(co, int):
            co = client['socket']
        if co not in self._outputs:
            self.add_server_output(co)
        return True

    def _do_write(self, writable):
        server = self.get_server()
        for sock in writable:
            client = self.get_client(sock)
            if client is None:
                continue
            try:
                next_msg = client['queue'].get_nowait()
            except Queue.Empty:
                self.remove_server_output(sock)
                continue
            #TODO execute actions like kick
            try:
                sock.sendall(next_msg.encode())
            except Exception as e:
                self.log_error("Error sending to client {}".format(client['addr']))
                continue
        return True

    #
    # SihdService
    #

    def on_start(self):
        super().on_start()
        if self._socket is None:
            self.log_error("Server is not up")
            return False
        if self._listening is True:
            self._clients = {}
            s = "Accepting connections ({} max)".format(self._max_co)
            self.log_info(s)

    def close(self):
        self.stop_server(True)
