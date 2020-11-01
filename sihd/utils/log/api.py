#!/usr/bin/python
# coding: utf-8

import os
import sys
import datetime
import logging
import platform
from logging.handlers import RotatingFileHandler
from logging import StreamHandler

from .colors import add_coloring_to_emit_windows, add_coloring_to_emit_ansi

class LoggerApi(object):

    __original_emit = logging.StreamHandler.emit
    __colored = False
    __stream_handlers = {}
    __file_handlers = {}

    def __init__(self, ext=""):
        self.change_logger(ext)
        self.set_level('info')
        self.date_fmt = '%H:%M:%S'
        self.file_date_fmt = '%Y-%m-%d %H:%M:%S'
        self.log_fmt = '%(asctime)s :: %(name)s :: %(levelname)s :: %(message)s'
        self.file_log_fmt = '%(asctime)s :: %(name)s :: %(threadName)s :: %(levelname)s :: %(message)s'
        self.filename_fmt = "%Y-%m-%d_{}.log"
        self.directory = ""

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

    @staticmethod
    def get_logger(ext=""):
        if ext:
            ext = "." + str(ext)
        return logging.getLogger('sihd' + ext)

    def change_logger(self, ext):
        logger = self.get_logger(ext)
        logger.setLevel(logging.DEBUG)
        self.debug = logger.debug
        self.info = logger.info
        self.warning = logger.warning
        self.error = logger.error
        self.critical = logger.critical
        self.logger = logger
        return self

    def set_stream_level(self, name):
        handlers = self.__stream_handlers.get(self.logger, None)
        if handlers is not None:
            for stream, handler in handlers.items():
                handler.setLevel(self._get_level(name))
        self.stream_level = name
        return self

    def set_file_level(self, name):
        handler = self.__file_handlers.get(self.logger, None)
        if handler is not None:
            handler.setLevel(self._get_level(name))
        self.file_level = name
        return self

    def set_level(self, name):
        self.set_stream_level(name)
        self.set_file_level(name)
        return self

    def setup(self, *args, **kwargs):
        self.add_stream_handler(*args, **kwargs)
        self.set_color(True)
        return self

    #
    # Stream handler
    #

    def get_stream_formatter(self):
        return logging.Formatter(self.log_fmt, self.date_fmt)

    def add_stream_handler(self, level=None, stream=sys.stderr, logger=None):
        logger = logger or self.logger
        self.remove_stream_handlers(logger)
        level = level or self.stream_level
        handler = StreamHandler(stream)
        handler.setLevel(self._get_level(level))
        self.set_level(level)
        handler.setFormatter(self.get_stream_formatter())
        logger.addHandler(handler)
        if logger not in self.__stream_handlers:
            self.__stream_handlers[logger] = {stream: handler}
        else:
            self.__stream_handlers[logger][stream] = handler
        return handler

    def remove_stream_handlers(self, logger=None, stream=None):
        logger = logger or self.logger
        sh = self.__stream_handlers.get(logger, None)
        ret = sh is not None
        if sh is not None:
            if stream is None:
                for s, handler in sh.items():
                    logger.removeHandler(handler)
                del self.__stream_handlers[logger]
            else:
                handler = sh.get(stream, None)
                if handler is not None:
                    logger.removeHandler(handler)
                    del sh[stream]
                else:
                    ret = False
        return ret

    #
    # File handler
    #

    def get_file_formatter(self):
        return logging.Formatter(self.file_log_fmt, self.file_date_fmt)

    def get_log_filename(self, directory, name):
        now = datetime.datetime.now()
        name = now.strftime(self.filename_fmt.format(name))
        return os.path.join(directory, name) if directory else name

    def remove_file_handler(self, logger=None):
        logger = logger or self.logger
        fh = self.__file_handlers.get(logger, None)
        if fh is not None:
            fh.close()
            logger.removeHandler(fh)
            del self.__file_handlers[logger]
        return fh is not None

    def add_file_handler(self, name, level=None, logger=None, directory=None):
        logger = logger or self.logger
        self.remove_file_handler(logger)
        level = level or self.file_level
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
        logger.debug("Logger '{}' file setup: {}".format(logger.name, path))
        self.__file_handlers[logger] = file_handler
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
