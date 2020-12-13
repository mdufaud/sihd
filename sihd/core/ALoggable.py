#!/usr/bin/python
# coding: utf-8

import os
import sys
import datetime
import logging
from logging.handlers import RotatingFileHandler
from logging import StreamHandler

import sihd
from .ANamedObject import ANamedObject

class ALoggable(ANamedObject):

    __warning_count = 0
    __error_count = 0
    __critical_count = 0

    def __init__(self, name="ALoggable", **kwargs):
        super().__init__(name, **kwargs)
        self.logger = sihd.log.get_logger(self.get_path())
        self.__can = True

    def set_log_active(self, activate):
        self.__can = activate

    def _log_format(self, *msg):
        return '\t'.join((str(m) for m in msg))

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
