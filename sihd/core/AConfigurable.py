#!/usr/bin/python
# coding: utf-8

from sihd.utils.config import ConfigApi
from .ANamedObject import ANamedObject

class AConfigurable(ANamedObject):

    def __init__(self, name="AConfigurable", **kwargs):
        super().__init__(name, **kwargs)
        self.__is_configured = False
        self.configuration = ConfigApi(self.get_name())

    def load_conf(self, conf, **kwargs):
        return self.configuration.load(conf, **kwargs)

    def get_conf(self):
        return self.configuration.to_dict()

    def setup(self, conf=None):
        ret = True
        conf_obj = self.configuration
        if conf is not None:
            ret = self.load_conf(conf)
        if ret and self._setup_impl(conf_obj) is True:
            self.__is_configured = True
            ret = True
        return ret

    def _setup_impl(self, conf):
        # MUST be implemented to use configuration
        return True

    def is_configured(self):
        return self.__is_configured

    def _set_unconfigured(self):
        """ Be sure to know what you are doing """
        self.__is_configured = False
