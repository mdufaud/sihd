#!/usr/bin/python
# coding: utf-8

import json
import urllib.request as urllib_request
import urllib.error as urllib_error
import urllib.parse as urllib_parse

#
# Server
#

import os
import threading
from http.server import HTTPServer, CGIHTTPRequestHandler

httpserver = None

def stop_server():
    global httpserver
    if httpserver:
        httpserver.shutdown()
        httpserver.server_close()
        httpserver = None

def start_server(path, port=8080):
    stop_server()
    os.chdir(path)
    global httpserver
    httpserver = HTTPServer(('', port), CGIHTTPRequestHandler)
    httpserver.serve_forever()

def server(path, port=8080, name='httpserver'):
    daemon = threading.Thread(name=name, target=start_server, args=(path, port))
    daemon.setDaemon(True)
    setattr(daemon, 'stop', stop_server)
    return daemon

#
# Requests
#

def build_url(url, query=None):
    if query is None:
        return url
    return "{:s}?{:s}".format(url, query)

def build_post(data):
    if data is None:
        return
    if isinstance(data, dict):
        return urllib_parse.urlencode(data).encode()
    return data.encode()

def build_query(data):
    if data is None:
        return
    if isinstance(data, str):
        return data
    return urllib_parse.urlencode(data)

def build_request(url, query=None, **kwargs):
    query = build_query(query)
    url = build_url(url, query)
    return urllib_request.Request(url, **kwargs)

def send(req, data=None):
    ret = None
    if req:
        post = build_post(data)
        try:
            ret = urllib_request.urlopen(req, post)
        except urllib_error.URLError as e:
            raise RuntimeError(e)
        if ret:
            ret = ret.read()
    return ret

def get(url, query=None, **kwargs):
    req = build_request(url, query, **kwargs)
    return send(req)

def post(url, data=None, **kwargs):
    req = build_request(url, **kwargs)
    return send(req, data)
