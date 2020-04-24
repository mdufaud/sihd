#!/usr/bin/python
#coding: utf-8

""" System """
import time
try:
    import Queue
except ImportError:
    import queue
    Queue = queue

from sihd.Handlers.IHandler import IHandler

from sihd.Readers.ip.IpReader import IpReader

class IpServerHandler(IHandler):

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
        self.add_channel_input("c_packet_data", type='queue')
        self.add_channel_input("c_client_info", type='queue')
        self.add_channel_input("c_server_msg", type='queue')
        self.add_channel_output("c_packet_send")
        self.add_channel_output("c_server_action")
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

    def handle_service(self, service):
        if isinstance(service, IpReader):
            #Observe service output
            service.c_packet_data.add_observer(self.c_packet_data)
            service.c_client_info.add_observer(self.c_client_info)
            service.c_server_msg.add_observer(self.c_server_msg)
            #Add service inputs to our outputs
            self.c_packet_send.add_observer(service.c_packet_send)
            self.c_server_action.add_observer(service.c_server_action)
            self.__tcp = service.is_tcp()
            self.__udp = service.is_udp()
            self.__raw = service.is_raw()
            return True
        return False

    def handle(self, channel):
        if channel == self.c_packet_data:
            msg, infos = channel.read()
            if self.__tcp:
                self.on_client_input(msg, infos)
            else:
                self.on_packet_data(msg, *infos)
        elif channel == self.c_server_msg:
            pass
        elif self.__tcp and channel == self.c_client_info:
            co, action, value = channel.read()
            self._parse_client_info(co, action, value)

    """ Utilities """

    def send_client(self, msg, co):
        self.c_packet_send.write((msg + "\n", co))

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
