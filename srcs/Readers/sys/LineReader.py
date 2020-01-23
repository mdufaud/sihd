#!/usr/bin/python
#coding: utf-8

""" System """

import time
import os

from sihd.srcs.Readers.IReader import IReader

class LineReader(IReader):
    
    files_read = {}  

    def __init__(self, path=None, app=None, name="LineReader"):
        super(LineReader, self).__init__(app=app, name=name)
        self.set_run_method(self.diffuse_line)
        self._set_default_conf({
            "path": "/path/to/file",
        })
        self._fully_read = False
        self._reader = None
        if path:
            self.set_source(path)


    """ IConfigurable """

    def _setup_impl(self):
        super(LineReader, self)._setup_impl()
        path = self.get_conf("path", default=False)
        if path:
            self.set_source(path)
        if not self._reader:
            return False
        return True

    """ Reader """

    def _can_recover(self):
        if self._path not in LineReader.files_read:
            return
        tupl = LineReader.files_read[self._path]
        if tupl[1] == False:
            s = "File {} already read".format(self._path)
            self.log_info(s)
            return
        self._to_recover = tupl[0]
        s = "To recover {}".format(self._to_recover)
        self.log_debug(s)

    def _recover(self):
        while self.is_active() and self._to_recover > 0:
            l = self._reader.readline()
            if l is None:
                return False
            self._to_recover -= 1
        if self._to_recover > 0:
            return True
        s = "Recovered {}".format(self._path)
        self.log_info(s)

    def set_source(self, path):
        self._path = os.path.abspath(path) if path else None
        self._lines = 0
        self._to_recover = 0
        if self._reader:
            self._reader.close()
            self._reader = None
        try:
            fp = open(path, 'r')
            self._reader = fp
        except IOError as err:
            self.notify_error(err)
            self.log_error("Cannot open: {}".format(err))
            return False
        self._can_recover()
        return True

    def read_line(self):
        reader = self._reader
        if not reader:
            return None
        try:
            line = self._reader.readline()
        except EOFError as e:
            line = None
        return line

    def diffuse_line(self):
        if self._to_recover > 0 and self._recover() == True:
            return True
        line = self.read_line()
        if line is None or line == "":
            self._read_end()
            return False
        self.notify_observers(line)
        self._lines += 1
        return True

    def _read_end(self):
        stop_time = time.time()
        self.log_info("File {0:s} read - {1:d} packets".format(self._path, self._lines))
        self.log_debug("took {0:.3f} seconds to read and process {1:d} lines"\
                .format(stop_time - self.get_thread_start_time(), self._lines))
        self._fully_read = True
        LineReader.files_read[self._path] = (self._lines, False)
        self.stop()

    """ IService """

    def _start_impl(self):
        if self._reader is None:
            self.log_error("No reader has been set")
            return False
        s = "Reading file {name}".format(name=self._path)
        self.log_info(s)
        return super(LineReader, self)._start_impl()

    def _stop_impl(self):
        if self._reader:
            self._reader.close()
            self._reader = None
        LineReader.files_read[self._path] = (self._lines, self._fully_read == False)
        return super(LineReader, self)._stop_impl()
