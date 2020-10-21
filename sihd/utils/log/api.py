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
        self.file_log_fmt = '%(asctime)s :: %(name)s :: %(threadName)s :: %(levelname)s :: %(message)s'
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
