#!/usr/bin/python
#coding: utf-8

from sihd.Handlers.AHandler import AHandler

class WordHandler(AHandler):

    def __init__(self, app=None, name="WordHandler"):
        super(WordHandler, self).__init__(app=app, name=name)
        self.set_default_conf({
            "delimiter": "",
            "skip": "#;//",
        })
        #self._stats = {}
        self.__toskip = None
        self.__delimiter = None
        self.__skipped = 0
        self.add_channel_input('input')
        self.add_channel_output('output', type='dict')
        self.add_channel_output('skipped', type='int', default=0)

    #
    # Configuration
    #

    def on_setup(self):
        ret = super().on_setup()
        delimiter = self.get_conf("delimiter")
        if isinstance(delimiter, str) and len(delimiter) >= 1:
            self.__delimiter = delimiter
        toskip = self.get_conf("skip")
        if isinstance(toskip, str) and len(toskip) >= 1:
            self.__toskip = toskip.split(";")
        return ret

    #
    # Channels
    #

    def handle(self, channel):
        if channel != self.input:
            return True
        line = channel.read()
        if not isinstance(line, str):
            return True
        #Skip lines
        toskip = self.__toskip
        if toskip:
            for skip in toskip:
                if line.find(skip) == 0:
                    self.__skipped += 1
                    self.skipped.write(self.__skipped)
                    return True
        #Stat words in line
        line = line.strip()
        lst = line.split(self.__delimiter)
        write = self.output.write
        read = self.output.read
        for word in lst:
            if word == "":
                continue
            write(word, read(word, 0) + 1)
        return True

    #
    # SihdService
    #

    def on_start(self):
        super().on_start()
        self.__skipped = 0
