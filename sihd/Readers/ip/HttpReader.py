#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

from sihd.Readers.IReader import IReader
from sihd.Interactors.ip.HttpInteractor import HttpInteractor

class HttpReader(IReader):
    
    def __init__(self, app=None, name="HttpReader"):
        super(HttpReader, self).__init__(app=app, name=name)
        self._set_default_conf({
            "url": "",
            "query": "",
            "json_post_file": "/path/to/json/post",
            "json_header_file": "/path/to/json/header",
        })
        self.__interactor = HttpInteractor(name="{}_interactor".format(name))
        self.set_step_method(self.diffuse_request)

    """ IConfigurable """

    def _setup_impl(self):
        super(HttpReader, self)._setup_impl()
        for key in ("url", "query", "json_post_file", "json_header_file"):
            self.__interactor.set_conf(key, self.get_conf(key))
        ret = self.__interactor.setup()
        if ret:
            self.__interactor.make_request()
        return ret

    def set_source(self, url, **kwargs):
        self.__interactor.make_request(url, **kwargs)
        return True

    def get_interactor(self):
        return self.__interactor

    def diffuse_request(self):
        ret = self.__interactor.send()
        if not ret:
            self.stop()
            return False
        self.deliver(ret)
        return True

    """ IService """

    def _start_impl(self):
        return super(HttpReader, self)._start_impl()

    def _stop_impl(self):
        return super(HttpReader, self)._stop_impl()
