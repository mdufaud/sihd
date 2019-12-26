#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import time
import os

serial = None

from sihd.srcs.Readers.IReader import IReader

class SerialReader(IReader):
    
    def __init__(self, port=None, app=None, name="SerialReader"):
        global serial
        if serial is None:
            import serial
        super(SerialReader, self).__init__(app=app, name=name)
        self._serial = None
        if port:
            self.set_source(port)
        self._set_default_conf({
            "port": "/dev/some_device",
            "baudrate": 9600,
            "timeout": 1,
        })
        self.set_run_method(self._read_line)

    """ IConfigurable """

    def _load_conf_impl(self):
        super(SerialReader, self)._load_conf_impl()
        baudrate = self.get_conf_val("baudrate")
        if baudrate:
            self._baudrate = int(baudrate)
        timeout = self.get_conf_val("timeout")
        if timeout:
            self._timeout = int(timeout)
        port = self.get_conf_val("port", not_default=True)
        if port:
            self.set_source(port)
        if not self._serial:
            return False
        return True

    """ Reader """

    def set_source(self, port):
        self._path = os.path.abspath(port) if port else None
        self._lines = 0
        if self._serial:
            self._serial.close()
            self._serial = None
        try:
            serial = serial.Serial(port, baudrate=self._baudrate, timeout=self._timeout)
            self._serial = serial
        except IOError as err:
            self.notify_error(err)
            self.log_error("Cannot open: {}".format(err))
            return False
        s = "Reading nmea packets on port: {}".format(port)
        self.log_info(s)
        self.notify_info(s)
        return True

    def _read_line(self):
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
