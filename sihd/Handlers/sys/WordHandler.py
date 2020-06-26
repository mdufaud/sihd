#!/usr/bin/python
# coding: utf-8

from sihd.Handlers.AHandler import AHandler

class WordHandler(AHandler):

    def __init__(self, name="WordHandler", app=None):
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
        self.add_channel('output', type='dict')
        self.add_channel('skipped', type='counter')
        self.add_channel('processed', type='counter')

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
            return False
        line = channel.read()
        if not isinstance(line, str):
            return
        #Skip lines
        toskip = self.__toskip
        willskip = False
        if toskip:
            for skip in toskip:
                if line.find(skip) == 0:
                    willskip = True
                    break
        if willskip is False:
            #Stat words in line
            line = line.strip()
            lst = line.split(self.__delimiter)
            write = self.output.write
            read = self.output.read
            for word in lst:
                if word == "":
                    continue
                write(word, read(word, 0) + 1)
        else:
            self.skipped.write()
        self.processed.write()

    def on_step(self):
        self.input.wait(0.1)

    #
    # SihdService
    #

    def on_start(self):
        super().on_start()
        self.skipped.clear()
        self.processed.clear()
