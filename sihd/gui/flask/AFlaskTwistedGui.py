#!/usr/bin/python
# coding: utf-8

#
# System
#
import os
import sys

import sihd
logger = sihd.log.setup('debug')
from .AFlaskGui import AFlaskGui

from autobahn.twisted.resource import WebSocketResource, WSGIRootResource
from autobahn.twisted.websocket import WebSocketServerFactory, WebSocketServerProtocol
from twisted.web.static import File
from twisted.internet import reactor
from twisted.web.proxy import ReverseProxyResource
from twisted.web.resource import Resource
from twisted.web.server import Site
from twisted.web.wsgi import WSGIResource

idx = 1
clients = {}

class AFlaskTwistedGui(AFlaskGui):

    def __init__(self, name="AFlaskTwistedGui", *args, **kwargs):
        super().__init__(name=name, *args, **kwargs)
        self.configuration.add_defaults({
            'static_resources_path': {},
        })
        self.configuration.set('runnable_type', 'default')

    #
    # Create twisted server
    #

    def put_websocket_resource(self, root):
        # create a Twisted Web resource for our WebSocket server
        port = self.configuration.get('port')
        host = self.configuration.get('host')
        ws_factory = WebSocketServerFactory("ws://%s:%d" % (host, port))
        gui = self

        class EchoServerProtocol(WebSocketServerProtocol):
            def onConnect(self, request):
                gui.log_info("On Connect !")

            def onOpen(self):
                gui.log_info("On Open !")

            def onMessage(self, payload, isBinary):
                gui.log_info("On Message !")

            def onClose(self, wasClean, code, reason):
                gui.log_info("On Close !")

        ws_factory.protocol = EchoServerProtocol
        ws_resource = WebSocketResource(ws_factory)
        root.putChild(b'ws', ws_resource)

    def put_static_resources(self, root):
        dic = self.configuration.get('static_resources_path')
        for route, path in dic.items():
            static_resource = File(sihd.resources.get(path))
            root.putChild(route.encode(), static_resource)

    def create_root_resource(self, app):
        # create a Twisted Web WSGI resource for our Flask server
        wsgi_resource = WSGIResource(reactor, reactor.getThreadPool(), app)
        # create a root resource serving everything via WSGI/Flask, but
        # the path "/ws" served by our WebSocket stuff
        root_resource = WSGIRootResource(wsgi_resource, {})
        self.put_websocket_resource(root_resource)
        self.put_static_resources(root_resource)
        #root_resource = WSGIRootResource(wsgi_resource, {b'assets': static_resource, b'ws': ws_resource})
        return root_resource

    def create_server(self, app):
        root_resource = self.create_root_resource(app)
        # create a Twisted Web Site
        site = Site(root_resource)
        reactor.listenTCP(self.configuration.get('port'), site)

    def run_server(self):
        reactor.run()

    def stop_server(self):
        pass
