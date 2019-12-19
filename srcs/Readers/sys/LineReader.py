#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import time
import os

from sihd.srcs.Readers.IReader import IReader

class LineReader(IReader):
    
    files_read = {}  

    def __init__(self, path=None, app=None, name="LineReader"):
        super(LineReader, self).__init__(app=app, name=name)
        self._fully_read = False
        self._reader = None
        if path:
            self.set_source(path)
        self._set_default_conf({
            "path": "/path/to/file",
        })
        self.set_run_method(self._read_line)

    """ IConfigurable """

    def _load_conf_impl(self):
        path = self.get_conf_val("path", not_default=True)
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
            s = "File already read"
            self.say(s)
            self.notify_info(s)
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
        self.notify_info(s)

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

    def _read_line(self):
        if self._to_recover > 0 and self._recover() == True:
            return True
        try:
            line = self._reader.readline()
        except EOFError as e:
            line = None
        if line is None or line == "":
            self._read_end()
            return False
        self.notify_observers(line)
        self._lines += 1
        return True

    def _read_end(self):
        stop_time = time.time()
        self.notify_info("File {0:s} read - {1:d} packets".format(self._path, self._lines))
        self.log_info("took {0:.3f} seconds to read and process {1:d} lines"\
                .format(stop_time - self._start_time, self._lines))
        self._fully_read = True
        LineReader.files_read[self._path] = (self._lines, False)
        self.stop()

    """ IService """

    def _start_impl(self):
        if self._reader is None:
            self.log_error("No reader has been set")
            return False
        self.setup_thread()
        self._start_time = time.time()
        s = "Reading file {name}".format(name=self._path)
        self.log_info(s)
        self.notify_info(s)
        self.start_thread()
        return True

    def _stop_impl(self):
        if self._reader:
            self._reader.close()
            self._reader = None
        self.stop_thread()
        LineReader.files_read[self._path] = (self._lines, self._fully_read == False)
        return True

    def _pause_impl(self):
        self.pause_thread()
        return True

    def _resume_impl(self):
        self.resume_thread()
        return True
