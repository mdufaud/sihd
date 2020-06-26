#!/usr/bin/python
# coding: utf-8

import os
import sys
import datetime
import logging
from logging.handlers import RotatingFileHandler
from logging import StreamHandler

class LoggerApi(object):
        
    __original_emit = logging.StreamHandler.emit
    __colored = False
    __file_handlers = {}

    def __init__(self, ext=""):
        logger = self.get_logger(ext)
        self.debug = logger.debug
        self.info = logger.info
        self.warning = logger.warning
        self.error = logger.error
        self.critical = logger.critical
        self.logger = logger
        self.date_fmt = '%H:%M:%S'
        self.file_date_fmt = '%Y-%m-%d %H:%M:%S'
        self.log_fmt = '%(asctime)s :: %(name)s :: %(levelname)s :: %(message)s'
        self.file_log_fmt = '%(asctime)s :: %(name)s :: %(levelname)s :: %(message)s'
        self.filename_fmt = "%Y-%m-%d_{}.log"
        self.directory = ""
        self.set_level('info')

    def set_level(self, name):
        self.logger.setLevel(self._get_level(name))
        self.level = name
    
    @staticmethod
    def get_logger(ext=""):
        if ext:
            ext = "." + str(ext)
        return logging.getLogger('sihd' + ext)

    @staticmethod
    def _get_level(level):
        if level == "debug":
            return logging.DEBUG
        elif level == "info":
            return logging.INFO
        elif level == "warning":
            return logging.WARNING
        elif level == "error":
            return logging.ERROR
        elif level == "critical":
            return logging.CRITICAL
        raise RuntimeError("Log level unknown: {}".format(level))

    def setup(self, *args, **kwargs):
        self.add_stream_handler(*args, **kwargs)
        self.set_color(True)
        return self.logger

    #
    # Log handlers
    #

    def get_formatter(self):
        return logging.Formatter(self.log_fmt, self.date_fmt)

    def add_stream_handler(self, level=None, stream=sys.stderr, logger=None):
        logger = logger or self.logger
        self.remove_stream_handlers(logger)
        level = level or self.level
        handler = StreamHandler(stream)
        handler.setLevel(self._get_level(level))
        self.set_level(level)
        handler.setFormatter(self.get_formatter())
        logger.addHandler(handler)
        return handler

    def remove_stream_handlers(self, logger=None):
        logger = logger or self.logger
        toremove = []
        for handler in logger.handlers:
            if isinstance(handler, StreamHandler):
                toremove.append(handler)
        for handler in toremove:
            logger.removeHandler(handler)

    #
    # File
    #

    def get_file_formatter(self):
        return logging.Formatter(self.file_log_fmt, self.file_date_fmt)

    def get_log_filename(self, directory, name):
        now = datetime.datetime.now()
        name = now.strftime(self.filename_fmt.format(name))
        return os.path.join(directory, name) if directory else name

    def remove_file_handler(self, logger=None):
        logger = logger or self.logger
        file_handler = self.__file_handlers.get(logger.name, None)
        if file_handler:
            file_handler.close()
            logger.removeHandler(file_handler)
            del self.__file_handlers[logger.name]
        return file_handler is not None

    def add_file_handler(self, name, level=None, logger=None, directory=None):
        logger = logger or self.logger
        self.remove_file_handler(logger)
        level = level or self.level
        directory = directory or self.directory
        if directory:
            if not os.path.exists(directory):
                try:
                    os.makedirs(directory)
                except Exception as e:
                    self.error("Could not make directories"
                                " for log directory: {}".format(e))
                    return
        path = self.get_log_filename(directory, name)
        file_handler = RotatingFileHandler(path, 'a+', 1e6, 1, encoding='utf-8')
        file_handler.setLevel(self._get_level(level))
        file_handler.setFormatter(self.get_file_formatter())
        logger.addHandler(file_handler)
        self.__file_handlers[logger.name] = file_handler
        logger.debug("Logger '{}' file setup: {}".format(logger.name, path))
        return file_handler

    #
    # Colors
    #

    @staticmethod
    def set_color(activate):
        if activate and LoggerApi.__colored:
            return
        if not activate and not LoggerApi.__colored:
            return
        if activate:
            if platform.system() == 'Windows':
                """
                    Windows does not support ANSI escapes and we are using
                    API calls to set the console color
                """
                logging.StreamHandler.emit = \
                    add_coloring_to_emit_windows(logging.StreamHandler.emit)
            else:
                """ all non-Windows platforms are supporting ANSI escapes """
                logging.StreamHandler.emit = \
                    add_coloring_to_emit_ansi(logging.StreamHandler.emit)
            logging.handlers.RotatingFileHandler.emit = LoggerApi.__original_emit
        else:
            logging.StreamHandler.emit = LoggerApi.__original_emit
        LoggerApi.__colored = activate

log = LoggerApi()

#
# Colors patch
#

import ctypes
import platform

# now we patch Python code to add color support to logging.StreamHandler
def add_coloring_to_emit_windows(fn):

    # add methods we need to the class
    def _out_handle(self):
        return ctypes.windll.kernel32.GetStdHandle(self.STD_OUTPUT_HANDLE)

    out_handle = property(_out_handle)

    def _set_color(self, code):
        # Constants from the Windows API
        self.STD_OUTPUT_HANDLE = -11
        hdl = ctypes.windll.kernel32.GetStdHandle(self.STD_OUTPUT_HANDLE)
        ctypes.windll.kernel32.SetConsoleTextAttribute(hdl, code)

    setattr(logging.StreamHandler, '_set_color', _set_color)

    def new(*args):
        FOREGROUND_BLUE      = 0x0001 # text color contains blue.
        FOREGROUND_GREEN     = 0x0002 # text color contains green.
        FOREGROUND_RED       = 0x0004 # text color contains red.
        FOREGROUND_INTENSITY = 0x0008 # text color is intensified.
        FOREGROUND_WHITE     = FOREGROUND_BLUE|FOREGROUND_GREEN |FOREGROUND_RED

       # winbase.h
        STD_INPUT_HANDLE = -10
        STD_OUTPUT_HANDLE = -11
        STD_ERROR_HANDLE = -12

        # wincon.h
        FOREGROUND_BLACK     = 0x0000
        FOREGROUND_BLUE      = 0x0001
        FOREGROUND_GREEN     = 0x0002
        FOREGROUND_CYAN      = 0x0003
        FOREGROUND_RED       = 0x0004
        FOREGROUND_MAGENTA   = 0x0005
        FOREGROUND_YELLOW    = 0x0006
        FOREGROUND_GREY      = 0x0007
        FOREGROUND_INTENSITY = 0x0008 # foreground color is intensified.

        BACKGROUND_BLACK     = 0x0000
        BACKGROUND_BLUE      = 0x0010
        BACKGROUND_GREEN     = 0x0020
        BACKGROUND_CYAN      = 0x0030
        BACKGROUND_RED       = 0x0040
        BACKGROUND_MAGENTA   = 0x0050
        BACKGROUND_YELLOW    = 0x0060
        BACKGROUND_GREY      = 0x0070
        BACKGROUND_INTENSITY = 0x0080 # background color is intensified.     

        levelno = args[1].levelno
        if (levelno >= 50):
            color = BACKGROUND_YELLOW | FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY 
        elif (levelno >= 40):
            color = FOREGROUND_RED | FOREGROUND_INTENSITY
        elif (levelno >= 30):
            color = FOREGROUND_YELLOW | FOREGROUND_INTENSITY
        elif (levelno >= 20):
            color = FOREGROUND_GREEN
        elif (levelno >= 10):
            color = FOREGROUND_MAGENTA
        else:
            color = FOREGROUND_WHITE
        args[0]._set_color(color)
        ret = fn(*args)
        args[0]._set_color(FOREGROUND_WHITE)
        return ret
    return new

def add_coloring_to_emit_ansi(fn):
    # add methods we need to the class
    def new(*args):
        levelno = args[1].levelno
        if (levelno >= 50):
            color = '\x1b[31m' # red
        elif (levelno >= 40):
            color = '\x1b[31m' # red
        elif (levelno >= 30):
            color = '\x1b[33m' # yellow
        elif (levelno >= 20):
            color = '\x1b[32m' # green 
        elif (levelno >= 10):
            color = '\x1b[35m' # pink
        else:
            color = '\x1b[0m' # normal
        #args[1].msg = color + args[1].msg +  '\x1b[0m'  # normal
        old = args[1].levelname
        args[1].levelname = color + args[1].levelname +  '\x1b[0m'  # normal
        ret = fn(*args)
        args[1].levelname = old
        return ret
    return new
