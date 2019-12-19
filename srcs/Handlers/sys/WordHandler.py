#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from sihd.srcs.Handlers.IHandler import IHandler

class WordHandler(IHandler):

    def __init__(self, app=None, name="WordHandler"):
        super(WordHandler, self).__init__(app=app, name=name)
        self._set_default_conf({
            "delimiter": " ",
            "skip": "#;//",
        })
        self._stats = {}
        self._toskip = None
        self._skipped = 0

    def _handle_result(self, line):
        if not isinstance(line, str):
            return
        toskip = self._toskip
        if toskip:
            for skip in toskip:
                if line.find(skip) == 0:
                    self._skipped += 1
                    return
        if line[-1] == "\n":
            line = line[0:-1]
        lst = line.split(self._delimiter)
        d = self._stats
        for word in lst:
            if word == "":
                continue
            if d.get(word, None) is None:
                d[word] = 0
            d[word] += 1

    """ IConfigurable """

    def _load_conf_impl(self):
        self._delimiter = self.get_conf_val("delimiter")
        toskip = self.get_conf_val("skip")
        if isinstance(toskip, str):
            self._toskip = toskip.split(";")
        return True

    """ IObservable """

    def handle(self, observable, line):
        if self.is_active():
            self._handle_result(line)
        return True

    """ IService """

    def _start_impl(self):
        self._skipped = 0
        return True

    def _stop_impl(self):
        return True

    def _pause_impl(self):
        return True

    def _resume_imp(self):
        return True
