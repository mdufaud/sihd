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
        self._set_default_conf({
            "path": "/path/to/file",
        })
        self.__fully_read = False
        self.__reader = None
        self.add_channel_input('path', type='queue', simple=True)
        self.add_channel_output('output')
        self.add_channel_output('lines', type='int', default=0)
        self.add_channel_output('eof', type='bool', timeout=0.1)

    """ IConfigurable """

    def on_setup(self):
        """ After setup to have eof channel created """
        ret = super().on_setup()
        path = self.get_conf("path", default=False)
        if path:
            ret = self.set_source(path)
        return ret

    def handle(self, channel):
        path = channel.read()
        if path:
            self.set_source(path)

    """ Reader """

    def __can_recover(self):
        if self.__path not in LineReader.files_read:
            return False
        tupl = LineReader.files_read[self.__path]
        if tupl[1] == False:
            s = "File {} already read".format(self.__path)
            self.log_info(s)
            return True
        self.__to_recover = tupl[0]
        self.log_debug("To recover {}".format(self.__to_recover))
        return True

    def __recover(self):
        reader = self.__reader
        if not reader:
            return False
        rdline = reader.readline
        recover = self.__to_recover
        while self.is_active() and recover > 0:
            l = rdline()
            if l is None:
                return False
            recover -= 1
        self.__to_recover = recover
        if recover > 0:
            return True
        self.log_info("Recovered {}".format(self.__path))

    def set_source(self, path):
        self.__lines = 0
        self.__to_recover = 0
        if self.__reader:
            self.__reader.close()
            self.__reader = None
        try:
            fp = open(path, 'r')
            self.__reader = fp
            self.__path = path
        except IOError as err:
            self.log_error("Cannot open: {}".format(err))
            return False
        self.__can_recover()
        self.eof.write(0)
        return True

    def read_line(self):
        reader = self.__reader
        if not reader:
            return None
        try:
            line = self.__reader.readline()
        except EOFError as e:
            line = None
        return line

    def do_step(self):
        if self.__to_recover > 0 and self.__recover() == True:
            return True
        line = self.read_line()
        if line is None or line == "":
            self.__read_end()
            return False
        line = line.strip()
        if line != "":
            self.output.write(line)
            self.__lines += 1
            self.lines.write(self.__lines)
        return True

    def __read_end(self):
        stop_time = time.time()
        self.log_info("File {0:s} read - {1:d} packets".format(self.__path, self.__lines))
        self.log_debug("took {0:.3f} seconds to read and process {1:d} lines"\
                .format(stop_time - self.get_service_start_time(), self.__lines))
        self.__fully_read = True
        LineReader.files_read[self.__path] = (self.__lines, False)
        self.eof.write(1)

    """ IService """

    def _start_impl(self):
        s = "Reading file {name}".format(name=self.__path)
        self.log_info(s)
        return super(LineReader, self)._start_impl()

    def _stop_impl(self):
        if self.__reader:
            self.__reader.close()
            self.__reader = None
        LineReader.files_read[self.__path] = (self.__lines, self.__fully_read == False)
        return super(LineReader, self)._stop_impl()
