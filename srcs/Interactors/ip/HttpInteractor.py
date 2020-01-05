#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import socket

json = None
requests = None

from sihd.srcs.Interactors.IInteractor import IInteractor

class HttpInteractor(IInteractor):

    def __init__(self, app=None, name="HttpInteractor"):
        super(HttpInteractor, self).__init__(app=app, name=name)
        global json
        if json is None:
            import json
        global requests
        if requests is None:
            import requests
        self._set_default_conf({
            "type": "get",
            "url": "www.set_the_url.com/arguments/x",
            "json_post_file": "/path/to/json/post_req",
        })
        self._url = ""
        self._post_data = {}
        self._post_file_path = None

    """ IConfigurable """

    def _setup_impl(self):
        super(HttpInteractor, self)._setup_impl()
        url = self.get_conf("url", default=False)
        if url:
            self._url = url
        path = self.get_conf("json_post_file", default=False)
        if path:
            self._post_data = self.get_post_from_file(path)
        t = self.get_conf("type")
        if t:
            self._type = t
        return True

    """ IInteractor """

    def _interact_impl(self, data, *args, **kwargs):
        t = self._type
        resp = None
        if t == "get":
            resp = self.get(*args, **kwargs)
        elif t == "post":
            resp = self.post(*args, **kwargs)
        return resp is not None

    """ Get """

    def get(self, url=None, *args, **kwargs):
        if url is None:
            url = self._url
        response = None
        try:
            response = requests.get(url)
        except Exception as e:
            self.log_error("GET error: {}".format(e))
        return response

    """ Post """

    def post(self, url, data=None, *args, **kwargs):
        if url is None:
            url = self._url
        if data is None:
            data = self._post_data
        response = None
        try:
            response = requests.post(url, data)
        except Exception as e:
            self.log_error("POST error: {}".format(e))
        return response

    """ Json """

    def set_post_to_file(self, dic, path=None):
        """
            @dic: python dictionnary
            @path: /path/to/file - default: json_post_file configuration
        """
        if path is None:
            path = self._post_file_path
        with open(path, 'w') as fd:
            json.dump(dic, fd)

    def set_post(self, dic):
        """ @dic = python dictionnary """
        self._post_data = dic

    def get_post_from_file(self, path=None):
        """ Accept only json format in file """
        ret = {}
        with open(path, 'r') as json_file:
            ret = json.load(json_file)
            self._post_file_path = path
        return ret
