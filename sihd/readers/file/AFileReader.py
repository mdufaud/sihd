#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import os

from sihd.readers.AReader import AReader

class AFileReader(AReader):

    def __init__(self, name="LineReader", app=None):
        super().__init__(app=app, name=name)
        self.files_read = {}
        self.configuration.add_defaults({
            "path": None,
        })
        self.opened = False
        self.current_path = None
        self.add_channel_input('path')
        self.add_channel('output')
        self.add_channel('count', type='counter')
        self.add_channel('eof', type='bool', default=True)

    #
    # Configuration
    #

    def on_init(self):
        """ After setup to have eof channel created """
        path = self.configuration.get('path')
        if path is not None:
            self.path.write(path)
        return super().on_init()

    #
    # Channels
    #

    def handle(self, channel):
        if channel == self.path:
            path = channel.read()
            if path:
                self.open(path)

    #
    # Runnable
    #

    def on_step(self):
        if not self.opened:
            return
        return self.read_once()

    #
    # File Reading
    #

    def open_file(self, path):
        raise NotImplementedError("open_file")

    def close_file(self):
        raise NotImplementedError("close_file")

    def read_once(self):
        raise NotImplementedError("read_once")

    # override
    def open(self, path):
        self.close()
        ret = self.open_file(path)
        if ret is True:
            self.current_path = path
            self.count.clear()
            self.eof.write(0)
            self.opened = True
        return True

    # override
    def close(self):
        self.opened = False
        path = self.current_path
        self.close_file()
        self.files_read[path] = (self.count.read(), self.eof.read() == False)
        self.log_debug("File {} closed".format(path))
        self.current_path = None

    def write(self, data):
        self.output.write(data)
        self.count.write()

    def read_end(self):
        self.log_info("File {0:s} read - {1:d} times"\
                      .format(self.current_path, self.count.read()))
        read_time = time.time() - self.get_service_start_time()
        self.log_debug("took {0:.3f} seconds to read and process {1:d} lines"\
                        .format(read_time, self.count.read()))
        self.eof.write(1)
        self.close()
        self.pause()
