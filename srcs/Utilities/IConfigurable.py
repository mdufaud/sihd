#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function
import os
import datetime
import logging
from logging.handlers import RotatingFileHandler

from .INamedObject import INamedObject

class IConfigurable(INamedObject):

    def __init__(self, name="IConfigurable"):
        super(IConfigurable, self).__init__(name)
        self.__default_conf = {}
        self.__config = None
        self.__has_section = False
        self.__is_configured = False

    def set_conf_obj(self, config_obj):
        self.__config = config_obj

    def get_conf_obj(self):
        return self.__config

    def get_conf_val(self, key, not_default=False):
        """
            Get value from conf - Either from file or dict

            @param key configuration key
            @param not_default returns None if value is still default
            @return value or None if not found
        """
        conf = self.get_conf_obj()
        if not conf and not_default is False:
            return self.get_default_conf(key)
        if not conf.has_option(self.get_name(), key):
            return None
        val = conf.get(self.get_name(), key)
        if not_default is True and val == self.get_default_conf(key):
            return None
        return val

    def load_conf(self, config_obj=None):
        """
            Load configuration from either file or default conf
            Calls load conf implementation
            Can only be called once
        """
        if self.__is_configured is True:
            return
        if config_obj is not None:
            self.set_conf_obj(config_obj)
        self.__setup_default_conf()
        if self._load_conf_impl():
            self.__is_configured = True
        return self.__is_configured

    def set_conf(self, key, value):
        self.__default_conf[key] = value

    def set_conf_file(self, key, value):
        """ Set a value in the obj configuration """
        if self.get_conf_obj():
            if not self.__has_section:
                self.__setup_section()
            self.__config.set(self.get_name(), key, value)

    def get_default_conf(self, key):
        return self.__default_conf.get(key, None)

    def _set_default_conf(self, dic):
        """
            MUST be called at obj init time
            Used to set a default configuration for obj to be put in file
        """
        self.__default_conf = dic

    def _load_conf_impl(self):
        """
            MUST be implemented by children
            Used for loading configuration from default conf or file
        """
        return True

    """ Private """

    def __setup_section(self):
        conf = self.__config
        if conf and not conf.has_section(self.get_name()):
                conf.add_section(self.get_name())
        self.__has_section = True

    def __setup_default_conf(self):
        """ Called when no section exists for obj in file """
        if self.__default_conf is None:
            return
        if not self.__has_section:
            self.__setup_section()
        fun = None
        if self.get_conf_obj():
            fun = self.set_conf_file
        else:
            fun = self.set_conf
        for key, value in self.__default_conf.items():
            fun(key, value)
