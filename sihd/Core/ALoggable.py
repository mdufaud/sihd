#!/usr/bin/python
#coding: utf-8

""" System """
import os
import sys
import datetime
import logging
from logging.handlers import RotatingFileHandler
from logging import StreamHandler

from .ANamedObject import ANamedObject

class ALoggable(ANamedObject):

    logger = logging.getLogger("sihd")
    __file_handler = None
    __level = None
    __original_emit = logging.StreamHandler.emit
    __is_color = False
    __warning_count = 0
    __error_count = 0
    __critical_count = 0

    def __init__(self, name="ALoggable", **kwargs):
        super(ALoggable, self).__init__(name, **kwargs)
        self.__can = True

    def _set_log(self, activate):
        self.__can = activate

    def _log_format(self, *msg):
        return "{0}: {1}".format(self.get_name(), ' '.join(msg))

    def log_debug(self, *msg):
        if self.__can:
            self.logger.debug(self._log_format(*msg))

    def log_info(self, *msg):
        if self.__can:
            self.logger.info(self._log_format(*msg))

    def log_warning(self, *msg):
        if self.__can:
            self.logger.warning(self._log_format(*msg))
        ALoggable.__warning_count += 1

    def log_error(self, *msg):
        if self.__can:
            self.logger.error(self._log_format(*msg))
        ALoggable.__error_count += 1

    def log_critical(self, *msg):
        if self.__can:
            self.logger.critical(self._log_format(*msg))
        ALoggable.__critical_count += 1

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
        raise RuntimeError("Level unknown {}".format(level))

    """ Class methods """

    @staticmethod
    def _get_filename(directory, app_name):
        now = datetime.datetime.now()
        name = now.strftime("%Y-%m-%d_{}.log".format(app_name))
        return os.path.join(directory, name)

    @staticmethod
    def get_format():
        return '%(asctime)s :: %(levelname)s :: %(message)s'

    @staticmethod
    def get_date_fmt():
        return '%Y-%m-%d %H:%M:%S'

    @staticmethod
    def get_formatter():
        return logging.Formatter(ALoggable.get_format(), ALoggable.get_date_fmt())

    @staticmethod
    def remove_stream_handlers():
        for handler in ALoggable.logger.handlers:
            if isinstance(handler, logging.StreamHandler):
                ALoggable.logger.removeHandler(handler)

    @staticmethod
    def add_stream_handler(level="info", stream=sys.stderr):
        ALoggable.remove_stream_handlers()
        handler = StreamHandler(stream)
        level = ALoggable.__level if ALoggable.__level is not None else level
        if ALoggable.__level is None:
            ALoggable.__level = level
        log_level = ALoggable._get_level(level)
        ALoggable.logger.setLevel(log_level)
        handler.setLevel(log_level)
        handler.setFormatter(ALoggable.get_formatter())
        ALoggable.logger.addHandler(handler)
        
    @staticmethod
    def remove_handlers():
        file_handler = ALoggable.__file_handler
        if file_handler:
            file_handler.close()
            ALoggable.__file_handler = None
        ALoggable.logger.handlers = []

    # Setup global logger
    @staticmethod
    def setup_log(app_name, level="info", directory=None):
        ALoggable.remove_handlers()
        ALoggable.__level = level
        log_level = ALoggable._get_level(level)
        ALoggable.logger.setLevel(log_level)
        ALoggable.add_stream_handler()
        if directory is None:
            return
        if not os.path.exists(directory):
            try:
                os.makedirs(directory)
            except Exception as e:
                ALoggable.logger.error("Could not make directories"
                                        " for log directory: {}".format(e))
                return
        file_handler = RotatingFileHandler(ALoggable._get_filename(directory, app_name),
                                            'a+', 1e6, 1, encoding='utf-8')
        file_handler.setLevel(log_level)
        file_handler.setFormatter(ALoggable.get_formatter())
        ALoggable.logger.addHandler(file_handler)
        ALoggable.__file_handler = file_handler
        ALoggable.logger.debug("Logger is setup")

    @staticmethod
    def set_color(activate):
        if activate and ALoggable.__is_color:
            return
        if not activate and not ALoggable.__is_color:
            return
        if activate:
            if platform.system() == 'Windows':
                # Windows does not support ANSI escapes and we are using API calls to set the console color
                logging.StreamHandler.emit = add_coloring_to_emit_windows(logging.StreamHandler.emit)
            else:
                # all non-Windows platforms are supporting ANSI escapes so we use them
                logging.StreamHandler.emit = add_coloring_to_emit_ansi(logging.StreamHandler.emit)
            logging.handlers.RotatingFileHandler.emit = ALoggable.__original_emit
        else:
            logging.StreamHandler.emit = ALoggable.__original_emit
        ALoggable.__is_color = activate

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
