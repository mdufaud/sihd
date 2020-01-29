#!/usr/bin/python
#coding: utf-8

""" System """

import time
import os

serial = None

from sihd.srcs.Readers.IReader import IReader

class SerialReader(IReader):
    
    def __init__(self, app=None, name="SerialReader"):
        global serial
        if serial is None:
            import serial
        super(SerialReader, self).__init__(app=app, name=name)
        self._serial = None
        self._set_default_conf({
            "port": "/dev/some_device",
            "baudrate": 9600,
            "timeout": 1,
        })
        self.set_run_method(self._read_serial)

    """ IConfigurable """

    def _setup_impl(self):
        super(SerialReader, self)._setup_impl()
        baudrate = self.get_conf("baudrate")
        if baudrate:
            self._baudrate = int(baudrate)
        timeout = self.get_conf("timeout")
        if timeout:
            self._timeout = int(timeout)
        port = self.get_conf("port", default=False)
        if port:
            self.set_source(port)
        if not self._serial:
            return False
        return True

    """ Reader """

    def set_source(self, port):
        self._lines = 0
        if self._serial:
            self._serial.close()
            self._serial = None
        try:
            ser = serial.Serial(port, baudrate=self._baudrate, timeout=self._timeout)
            self._serial = ser
        except IOError as err:
            self.log_error("Cannot open: {}".format(err))
            return False
        self._path = port
        s = "Reading on port: {}".format(port)
        self.log_info(s)
        return True

    def get_serial(self):
        return self._serial

    def _read_serial(self):
        try:
            line = self._serial.readline()
        except EOFError as e:
            line = None
        if line is None:
            self.stop()
            return False
        self.notify_observers(line)
        self._lines += 1
        return True

    """ IService """

    def _start_impl(self):
        if self._serial is None:
            self.log_error("No serial reader has been set")
            return False
        return super(SerialReader, self)._start_impl()

    def _stop_impl(self):
        if self._serial:
            self._serial.close()
            self._serial = None
        return super(SerialReader, self)._stop_impl()
