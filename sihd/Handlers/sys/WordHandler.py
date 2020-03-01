#!/usr/bin/python
#coding: utf-8

from sihd.Handlers.IHandler import IHandler

class WordHandler(IHandler):

    def __init__(self, app=None, name="WordHandler"):
        super(WordHandler, self).__init__(app=app, name=name)
        self._set_default_conf({
            "delimiter": "",
            "skip": "#;//",
        })
        self._stats = {}
        self._toskip = None
        self._skipped = 0

    """ IConfigurable """

    def _setup_impl(self):
        delimiter = self.get_conf("delimiter")
        if isinstance(delimiter, str) and len(delimiter) >= 1:
            self._delimiter = delimiter
        else:
            self._delimiter = None
        toskip = self.get_conf("skip")
        if isinstance(toskip, str):
            self._toskip = toskip.split(";")
        return True

    """ IObservable """

    def handle(self, observable, line):
        if not isinstance(line, str):
            return True
        toskip = self._toskip
        if toskip:
            for skip in toskip:
                if line.find(skip) == 0:
                    self._skipped += 1
                    return True
        line = line.strip()
        lst = line.split(self._delimiter)
        d = self._stats
        get = d.get
        for word in lst:
            if word == "":
                continue
            d[word] = get(word, 0) + 1
        return True

    """ IService """

    def _start_impl(self):
        self._skipped = 0
        return super()._start_impl()
