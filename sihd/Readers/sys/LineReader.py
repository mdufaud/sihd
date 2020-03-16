#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

from sihd.Readers.IReader import IReader

class LineReader(IReader):
    
    files_read = {}  

    def __init__(self, app=None, name="LineReader"):
        super(LineReader, self).__init__(app=app, name=name)
        self.set_step_method(self.diffuse_line)
        self._set_default_conf({
            "path": "/path/to/file",
        })
        self._fully_read = False
        self._reader = None

    """ IConfigurable """

    def do_setup(self):
        ret = super().do_setup()
        path = self.get_conf("path", default=False)
        if path:
            self.set_source(path)
        if not self._reader:
            ret = False
        return ret

    def do_channels(self):
        ret = super().do_channels()
        return ret and self.create_output('output') is not None

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
        self.log_debug("To recover {}".format(self._to_recover))

    def _recover(self):
        while self.is_active() and self._to_recover > 0:
            l = self._reader.readline()
            if l is None:
                return False
            self._to_recover -= 1
        if self._to_recover > 0:
            return True
        self.log_info("Recovered {}".format(self._path))

    def set_source(self, path):
        self._lines = 0
        self._to_recover = 0
        if self._reader:
            self._reader.close()
            self._reader = None
        try:
            fp = open(path, 'r')
            self._reader = fp
            self._path = path
        except IOError as err:
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
        line = line.strip()
        if line != "":
            self.output.write(line)
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
