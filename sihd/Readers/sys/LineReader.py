#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import os

from sihd.Readers.AReader import AReader

class LineReader(AReader):

    files_read = {}

    def __init__(self, name="LineReader", app=None):
        super(LineReader, self).__init__(app=app, name=name)
        self.configuration.add_defaults({
            "path": "/path/to/file",
        })
        self.__reader = None
        self.add_channel_input('path')
        self.add_channel('output')
        self.add_channel('lines', type='counter')
        self.add_channel('eof', type='bool', default=True)

    #
    # Configuration
    #

    def on_init(self):
        """ After setup to have eof channel created """
        path = self.configuration.get('path', default=False)
        if path:
            self.path.write(path)
        return super().on_init()

    #
    # Channels
    #

    def handle(self, channel):
        if channel == self.path and self.__reader is None:
            path = channel.read()
            if path:
                self.set_source(path)

    #
    # Reader
    #

    def set_source(self, path):
        self.lines.clear()
        self.close()
        try:
            fp = open(path, 'r')
            self.__reader = fp
            self.__path = path
        except IOError as err:
            self.log_error("Cannot open: {}".format(err))
            return False
        self.log_info("Reading file {name}".format(name=self.__path))
        self.eof.write(0)
        return True

    def read_line(self):
        try:
            line = self.__reader.readline()
        except EOFError as e:
            line = None
        return line

    def on_step(self):
        if not self.__reader:
            return
        line = self.read_line()
        if line is None or line == "":
            self.__read_end()
            return
        line = line.strip()
        if line != "":
            self.output.write(line)
            self.lines.write()

    def __read_end(self):
        stop_time = time.time()
        self.log_info("File {0:s} read - {1:d} lines".format(self.__path, self.lines.read()))
        self.log_debug("took {0:.3f} seconds to read and process {1:d} lines"\
                .format(stop_time - self.get_service_start_time(), self.lines.read()))
        self.eof.write(1)
        self.close()
        self.pause()

    def close(self):
        if self.__reader:
            self.__reader.close()
            self.__reader = None
            LineReader.files_read[self.__path] = (self.lines.read(), self.eof.read() == False)
            self.log_debug("File {} closed".format(self.__path))
