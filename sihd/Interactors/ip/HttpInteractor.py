#!/usr/bin/python
#coding: utf-8

""" System """


import socket

json = None
urllib_request = None
urllib_error = None
urllib_parse = None

from sihd.Interactors.IInteractor import IInteractor

class HttpInteractor(IInteractor):

    def __init__(self, app=None, name="HttpInteractor"):
        super(HttpInteractor, self).__init__(app=app, name=name)
        global json
        if json is None:
            import json
        global urllib_request
        if urllib_request is None:
            import urllib.request as urllib_request
        global urllib_error
        if urllib_error is None:
            import urllib.error as urllib_error
        global urllib_parse
        if urllib_parse is None:
            import urllib.parse as urllib_parse
        self._set_default_conf({
            "url": "",
            "query": "",
            "json_post_file": "/path/to/json/post",
            "json_header_file": "/path/to/json/header",
        })
        self._url = ""
        self._args = {}
        self._post = None
        self._query = None
        self._req = None
        self._post_file_path = None

    """ IConfigurable """

    def _setup_impl(self):
        super(HttpInteractor, self)._setup_impl()
        url = self.get_conf("url")
        if url:
            self._url = url
        path_post = self.get_conf("json_post_file", default=False)
        if path_post:
            post = self.get_json_from_file(path_post)
            self.set_post(post)
        path_header = self.get_conf("json_header_file", default=False)
        if path_header:
            headers = self.get_json_from_file(path_header)
            self.set_headers(headers)
        query = self.get_conf("query")
        if query:
            self.set_query(query)
        self.make_request()
        return True

    """ IInteractor """

    def _interact_impl(self, data, *args, **kwargs):
        resp = self.send(*args, **kwargs)
        return resp is not None

    """ Request """

    def make_request(self, url=None, *args, query=None, headers=None, post=None):
        if url is None:
            url = self._url
        else:
            self._url = url
        if post is not None:
            self.set_post(post)
        if query is not None:
            self.set_query(query)
        if headers is not None:
            self.set_headers(headers)
        url = self._get_url(url, self._query)
        self._req = urllib_request.Request(url, *self._args)
        return self

    def _get_url(self, url, query):
        if query is None:
            return url
        url = "{:s}?{:s}".format(url, query)
        return url

    def add_header(self, dic):
        """ @params dic dictionnary """
        for key, value in dic.items():
            self._req.add_header(key, value)

    def send(self):
        req = self._req
        ret = None
        if req:
            try:
                ret = urllib_request.urlopen(req, self._post)
            except urllib_error.URLError as e:
                url = self._get_url(self._url, self._query)
                self.log_error("URL error: {}\n"
                                "(url={},args={})".format(e.reason, url, self._args))
            except TypeError as e:
                self.log_error("Type error: {}".format(e))
            if ret:
                ret = ret.read()
        else:
            self.log_error("No request built")
        return ret

    """ Json """

    def set_json_file(self, dic, path):
        """
            @param dic python dictionnary
            @param path /path/to/file
        """
        with open(path, 'w') as fd:
            json.dump(dic, fd)

    def set_query(self, data):
        """ @param data python dictionnary or str """
        if isinstance(data, dict):
            self._query = urllib_parse.urlencode(data)
        elif isinstance(data, str):
            self._query = data
        else:
            self.log_error("Type error for query: {}".format(data))
        return False

    def set_post(self, data):
        """ @param dic python dictionnary """
        if isinstance(data, dict):
            self._post = urllib_parse.urlencode(data).encode()
        else:
            try:
                self._post = data.encode()
            except AttributeError:
                self.log_error("Type error for post: {}".format(data))
                return False
        return True

    def set_headers(self, dic):
        """ @parm dic python dictionnary """
        if isinstance(dic, dict):
            self._args['headers'] = dic
            return True
        else:
            self.log_error("Not a dictionnary for headers: {}".format(dic))
        return False

    def get_json_from_file(self, path):
        """ Accept only json format in file """
        ret = {}
        with open(path, 'r') as json_file:
            ret = json.load(json_file)
        return ret
