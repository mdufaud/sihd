#!/usr/bin/python
# coding: utf-8

#
# System
#
import time
import os

serial = None

from sihd.readers.AReader import AReader

class SerialReader(AReader):

    def __init__(self, name="SerialReader", app=None):
        global serial
        if serial is None:
            import serial
        super().__init__(app=app, name=name)
        self.__serial = None
        self.configuration.add_defaults({
            "port": "/dev/some_device",
            "baudrate": 9600,
        })
        self.configuration.add_defaults({
            "bytesize": "EIGHTBITS",
            "stopbits": "STOPBITS_ONE",
            "parity": "PARITY_NONE",
            "xonxoff": False,
            "rtscts": False,
            "dsrdtr": False,
            "exclusive": False,
            "inter_byte_timeout": 1.0,
            "write_timeout": 1.0,
            "timeout": 1.0,
        }, expose=False)
        self.__conf = {}
        self.add_channel_input('input')
        self.add_channel('output')
        self.add_channel('lines', type='counter')
        self.add_channel('wrote', type='counter')

    #
    # Configuration
    #

    def on_setup(self, conf):
        ret = super().on_setup()
        baudrate = conf.get("baudrate")
        if baudrate:
            self.__conf['baudrate'] = int(baudrate)
        timeout = conf.get("timeout")
        if timeout:
            self.__conf['timeout'] = float(timeout)
        port = conf.get("port", default=False)
        if port:
            self.open(port)
        if not self.__serial:
            self.log_error("Bad configuration: no port or could not open")
            return False
        return True

    #
    # Channels
    #

    def handle(self, channel):
        ser = self.__serial
        if not s:
            return
        if channel == self.input:
            rd = channel.read()
            if rd and ser.writable():
                ser.write(rd)
                self.wrote.write()

    #
    # Reader
    #

    def open(self, port):
        self.lines.clear()
        self.wrote.clear()
        self.close()
        try:
            ser = serial.Serial(port, **self.__conf)
            self.__serial = ser
        except IOError as err:
            self.log_error("Cannot open: {}".format(err))
            return False
        self.log_info("Reading on port: {}".format(port))
        return True

    def get_serial(self):
        return self.__serial

    def read_serial(self):
        ser = self.__serial
        if ser.readable() is False:
            return None
        try:
            line = ser.readline()
        except EOFError as e:
            self.log_info("Serial EOF")
            self.stop()
            line = None
        if line is not None:
            self.lines.write()
        return line

    def on_step(self):
        line = self.read_serial()
        if line is not None:
            self.output.write(line)

    #
    # SihdObject
    #

    def close():
        serial = self.__serial
        if serial:
            serial.close()
            self.__serial = None
