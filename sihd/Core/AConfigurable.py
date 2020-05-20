#!/usr/bin/python
#coding: utf-8

#
# System
#
import os
import datetime
import logging
from logging.handlers import RotatingFileHandler

from .ANamedObject import ANamedObject

class AConfigurable(ANamedObject):

    def __init__(self, name="AConfigurable", **kwargs):
        super(AConfigurable, self).__init__(name, **kwargs)
        self.__default_conf = {}
        self.__conf = {}
        self.__conf_obj = None
        self.__has_section = False
        self.__is_configured = False
        self.__dynamic_conf = False

    #
    # Entry point for object configuration
    #

    def setup(self, config_obj=None):
        """
            Load configuration from either file or default/setted conf
            Calls setup implementation from children (_setup_impl)
        """
        if self.__is_configured is True:
            return True
        if config_obj is not None:
            self.set_conf_obj(config_obj)
        if self.__setup_section():
            self.__write_default_conf()
        if self._setup_impl() is True:
            self.__is_configured = True
        return self.__is_configured

    def is_configured(self):
        """ Returns true if obj is setup """
        return self.__is_configured

    #
    # Main getter
    #

    def get_conf(self, key, default=True):
        """
            Get value from conf - Either from file or dict

            @param key configuration key
            @param not_default returns None if value is still default
            @return value or None if not found
        """
        val = None
        conf = self.get_conf_obj()
        #Gets conf from either obj or setted configuration
        if conf:
            #From obj
            if conf.has_option(self.get_name(), key):
                val = conf.get(self.get_name(), key)
            else:
                raise ValueError("Key {} not found".format(key))
        else:
            #From manual set/default
            val = self.__conf.get(key, None)
        if default is False:
            #Checks if val is equal to default
            val_default = self.get_default_conf(key)
            if val_default == val:
                val = self.__conf.get(key, None)
                if val_default == val:
                    val = None
        elif val is None:
            #If config has not been found elsewhere
            val = self.get_default_conf(key)
        return val

    def allow_dynamic_conf(self, active):
        self.__dynamic_conf = active

    #
    # Main setters
    #

    def set_conf_from_dict(self, dic, force=False):
        if not self.__dynamic_conf:
            bad_keys = [k for k in dic.keys() if k not in self.__default_conf]
            if bad_keys:
                raise KeyError("{} keys not in configuration: {}"\
                                .format(self.get_name(), bad_keys))
        self.__conf.update(dic)
        if force is True:
            for k, v in dic.items():
                self.set_conf_file(k, v, force=force)
        return True

    def set_conf(self, key, value=None, force=False):
        """
            Set value in dic
            @param dic dictionnary to complete
            @param key can be a string or a dictionnary
            @param value can be anything - default: None
        """
        if isinstance(key, dict):
            return self.set_conf_from_dict(key, force=force)
        if not self.__dynamic_conf:
            if key not in self.__default_conf:
                raise KeyError("{} key not in configuration: {}"\
                                .format(self.get_name(), key))
        self.__conf[key] = value
        if force is True:
            return self.set_conf_file(key, value, force=force)
        return True

    def set_conf_file(self, key, value, force=False, obj=None):
        """ Set a value in the obj configuration directly """
        if obj is None:
            obj = self.get_conf_obj()
        name = self.get_name()
        if not obj:
            return False
        if not self.__has_section:
            self.__setup_section(obj)
        elif obj.has_option(name, key):
            base_val = obj.get(name, key)
            if not force and base_val != value:
                return False
        obj.set(name, key, value)
        return True

    #
    # Special getters
    #

    def get_default_conf_dict(self):
        return self.__default_conf

    def get_default_conf(self, key):
        """ Get the value from default dict """
        return self.__default_conf.get(key)

    def set_conf_obj(self, config_obj):
        """ Set the configparser obj """
        self.__conf_obj = config_obj

    def get_conf_obj(self):
        """ Returns the configparser obj """
        return self.__conf_obj

    def get_conf_dict(self, defaults=True):
        if not defaults:
            return self.__conf
        dic = {}
        for key in self.__default_conf.keys():
            dic[key] = self.get_conf(key)
        return dic

    #
    # Save
    #

    def save_conf(self, obj=None):
        """ Writes configured obj to file """
        dic = dict(self.__default_conf)
        dic.update(self.__conf)
        obj = obj if obj is not None else self.get_conf_obj()
        if obj:
            return self.__write_dict_conf(dic, obj)
        return False

    #
    # Define obj configuration
    #

    def set_default_conf(self, dic):
        """
            MUST be used before setup
            Set a default object configuration
        """
        if isinstance(dic, dict):
            self.__default_conf.update(dic)

    def _setup_impl(self):
        """
            MUST be implemented to use configuration
        """
        return True

    #
    # Private
    #

    def __setup_section(self, obj=None):
        """ Sets section for obj in configparser """
        conf = obj if obj is not None else self.get_conf_obj()
        added = False
        if conf and conf.has_section(self.get_name()) is False:
            conf.add_section(self.get_name())
            added = True
        self.__has_section = True
        return added

    def __write_dict_conf(self, dic, obj=None):
        """ Writes dictionnary in configparser """
        if obj is None:
            return False
        if not self.__has_section:
            self.__setup_section(obj)
        fun = self.set_conf_file
        keys = sorted(dic.keys())
        for key in keys:
            fun(key, str(dic[key]), force=False, obj=obj)
        return True

    def __write_default_conf(self, obj=None):
        """ Called when no section exists for obj in file """
        obj = obj if obj is not None else self.get_conf_obj()
        return self.__write_dict_conf(self.__default_conf, obj)

    def _set_unconfigured(self):
        """ Be sure to know what you are doing """
        self.__is_configured = False
