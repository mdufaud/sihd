#!/usr/bin/python
# coding: utf-8

json = None
urllib_request = None
urllib_error = None
urllib_parse = None

import sihd
from sihd.Interactors.AInteractor import AInteractor

class HttpInteractor(AInteractor):

    def __init__(self, name="HttpInteractor", **kwargs):
        super().__init__(name=name, **kwargs)
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
        self.configuration.add_defaults({
            "url": "",
            "query": "",
            "json_post_file": "/path/to/json/post",
            "json_header_file": "/path/to/json/header",
        })
        self.configuration.set('runnable_frequency', 1)
        self.__url = ""
        self.__args = {}
        self.__post = None
        self.__query = None
        self.__req = None
        self.__post_file_path = None
        self.add_channel_input("query")
        self.add_channel_input("headers")
        self.add_channel_input("post")

    #
    # Configuration
    #

    def on_setup(self, conf):
        ret = super().on_setup(conf)
        path_post = sihd.resources.get(conf.get("json_post_file", default=False))
        if path_post:
            post = self.get_json_from_file(path_post)
            ret = ret and self.set_post(post)
        path_header = sihd.resources.get(conf.get("json_header_file", default=False))
        if path_header:
            headers = self.get_json_from_file(path_header)
            ret = ret and self.set_headers(headers)
        query = conf.get("query")
        if query:
            ret = ret and self.set_query(query)
        self.__url = conf.get("url")
        if self.__url:
            self.make_request()
        return ret

    #
    # Channels
    #

    def handle(self, channel):
        if channel == self.query:
            query = channel.read()
            if query:
                self.set_query(query)
        elif channel == self.headers:
            hdr_json = channel.read()
            if hdr_json:
                hdr = json.loads(hdr_json)
                self.set_headers(hdr)
        elif channel == self.post:
            post_json = channel.read()
            if post_json:
                post = json.loads(post_json)
                self.set_post(post)

    #
    # Interactor
    #

    def on_new_interaction(self, url):
        self.make_request(url)
        return url

    def on_interaction(self, url, *args, **kwargs):
        if self.__req is None:
            #Waiting for interaction
            return True
        resp = self.send(*args, **kwargs)
        self.set_result(resp)
        return resp is not None

    #
    # Request
    #

    def make_request(self, url=None, *args, query=None, headers=None, post=None):
        if url is None:
            url = self.__url
        else:
            self.__url = url
        if post is not None:
            self.set_post(post)
        if query is not None:
            self.set_query(query)
        if headers is not None:
            self.set_headers(headers)
        url = self._get_url(url, self.__query)
        self.__req = urllib_request.Request(url, *self.__args)
        return self

    def _get_url(self, url, query):
        if query is None:
            return url
        url = "{:s}?{:s}".format(url, query)
        return url

    def add_header(self, dic):
        """ @params dic dictionnary """
        for key, value in dic.items():
            self.__req.add_header(key, value)

    def send(self):
        ret = None
        req = self.__req
        if req:
            try:
                ret = urllib_request.urlopen(req, self.__post)
            except urllib_error.URLError as e:
                url = self._get_url(self.__url, self.__query)
                self.log_error("URL error: {}\n"
                                "(url={},args={})".format(e.reason, url, self.__args))
            except TypeError as e:
                self.log_error("Type error: {}".format(e))
            if ret:
                ret = ret.read()
        else:
            self.log_error("No request built")
        return ret

    #
    # JSON
    #

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
            self.__query = urllib_parse.urlencode(data)
        elif isinstance(data, str):
            self.__query = data
        else:
            self.log_error("Type error for query: {}".format(data))
        return False

    def set_post(self, data):
        """ @param dic python dictionnary """
        if isinstance(data, dict):
            self.__post = urllib_parse.urlencode(data).encode()
        else:
            try:
                self.__post = data.encode()
            except AttributeError:
                self.log_error("Type error for post: {}".format(data))
                return False
        return True

    def set_headers(self, dic):
        """ @parm dic python dictionnary """
        if isinstance(dic, dict):
            self.__args['headers'] = dic
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
