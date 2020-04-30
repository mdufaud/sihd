#!/usr/bin/python
#coding: utf-8

""" System """
import time
import os

serial = None

from sihd.Readers.IReader import IReader

class SerialReader(IReader):
    
    def __init__(self, app=None, name="SerialReader"):
        global serial
        if serial is None:
            import serial
        super(SerialReader, self).__init__(app=app, name=name)
        self.__serial = None
        self._set_default_conf({
            "port": "/dev/some_device",
            "baudrate": 9600,
            "timeout": 1,
        })

    """ IConfigurable """

    def on_setup(self):
        ret = super().on_setup()
        baudrate = self.get_conf("baudrate")
        if baudrate:
            self.__baudrate = int(baudrate)
        timeout = self.get_conf("timeout")
        if timeout:
            self.__timeout = int(timeout)
        port = self.get_conf("port", default=False)
        if port:
            self.set_source(port)
        if not self.__serial:
            self.log_error("Bad configuration: no port or could not open")
            return False
        return True

    """ Reader """

    def set_source(self, port):
        self._lines = 0
        self.close()
        try:
            ser = serial.Serial(port, baudrate=self.__baudrate,
                                timeout=self.__timeout)
            self.__serial = ser
        except IOError as err:
            self.log_error("Cannot open: {}".format(err))
            return False
        self.__path = port
        self.log_info("Reading on port: {}".format(port))
        return True

    def get_serial(self):
        return self.__serial

    def read_serial(self):
        serial = self.__serial
        if not serial:
            return None
        try:
            line = serial.readline()
        except EOFError as e:
            self.log_info("Serial EOF")
            self.stop()
            line = None
        if line is not None:
            self._lines += 1
        return line

    def on_step(self):
        line = self.read_serial()
        if line is not None:
            self.output.write(line)
        return True

    """ IService """

    def close():
        serial = self.__serial
        if serial:
            serial.close()
            self.__serial = None
