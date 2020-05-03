#!/usr/bin/python
#coding: utf-8

""" System """
import time
try:
    import Queue
except ImportError:
    import queue
    Queue = queue

from sihd.Handlers.AHandler import AHandler

from sihd.Readers.ip.IpReader import IpReader

class IpServerHandler(AHandler):

    def __init__(self, app=None, name="IpServerHandler"):
        super(IpServerHandler, self).__init__(app=app, name=name)
        self._clients = {}
        self._set_default_conf({
            "service_type": "thread",
            "server_protocol": "tcp",
        })
        self.__server_action = {
            "co": self._set_client_connected,
            "addr": self._set_client_addr,
        }
        self.add_channel_input("packet_data", type='queue')
        self.add_channel_input("client_info", type='queue')
        self.add_channel_input("server_msg", type='queue')
        self.add_channel_output("packet_send", type='queue')
        self.add_channel_output("server_action", type='queue')
        self.__udp = False
        self.__tcp = False
        self.__raw = False

    def on_setup(self):
        ret = super().on_setup()
        self.set_server_protocol(self.get_conf("server_protocol"))
        return ret

    def set_server_protocol(self, proto):
        if proto == 'raw':
            self.__raw = True
        elif proto == 'udp':
            self.__udp = True
        elif proto == "tcp":
            self.__tcp = True

    def handle_service_ipreader(self, service):
        #IpReader outputs: Read directly from outputs
        self.link_channel("packet_data", service.packet_data)
        self.link_channel("client_info", service.client_info)
        self.link_channel("server_msg", service.server_msg)
        #IpReader inputs: Post directly to inputs
        self.link_channel("server_action", service.server_action)
        self.link_channel("packet_send", service.packet_send)

        self.__tcp = service.is_tcp()
        self.__udp = service.is_udp()
        self.__raw = service.is_raw()
        return False

    def handle(self, channel):
        if channel == self.packet_data:
            msg, infos = channel.read()
            if self.__tcp:
                self.on_client_input(msg, infos)
            else:
                self.on_packet_data(msg, *infos)
        elif channel == self.server_msg:
            pass
        elif self.__tcp and channel == self.client_info:
            co, action, value = channel.read()
            self._parse_client_info(co, action, value)

    """ Utilities """

    def send_client(self, msg, co):
        self.packet_send.write((msg + "\n", co))

    """ Packet data  """

    def on_packet_data(self, msg, host=None, port=0):
        self.log_info("Packet from {}:{} said : {}".format(host, port, msg))

    """ Client input """

    def on_client_input(self, msg, co):
        self.log_info("Client {} said : {}".format(co, msg))

    """ Client info """

    def get_client(self, co):
        return self._clients.get(co, None)

    def _parse_client_info(self, co, action, value):
        method = self.__server_action.get(action, None)
        if method is None:
            self.log_error("Client info action {} not recognized".format(action))
        else:
            method(co, value)

    def _set_client_addr(self, co, addr):
        client = self._clients.get(co, None)
        if client:
            client['addr'] = addr
        else:
            self.log_error("No such client {}".format(co))

    def _set_client_connected(self, co, active):
        if active:
            self._clients[co] = {
                "name": "Client_" + str(co),
                "addr": "",
                "time_co": time.time(),
                "socket": co,
            }
        else:
            self._clients[co] = None
