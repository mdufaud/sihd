#!/usr/bin/python
# coding: utf-8

from sihd.utils.config import ConfigApi
from .ANamedObject import ANamedObject

class AConfigurable(ANamedObject):

    def __init__(self, name="AConfigurable", **kwargs):
        super().__init__(name, **kwargs)
        self.__is_configured = False
        self.configuration = ConfigApi(self.get_path())
        self.save_conf = self.configuration.save

    def get_conf(self):
        return self.__conf

    def setup(self, obj=None):
        ret = True
        conf = self.configuration
        if obj is not None:
            conf.set_configuration_object(obj)
        if conf.obj and conf.has_section() is False:
            ret = conf.save()
        if ret and self._setup_impl(conf) is True:
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

"""
class AConfigurable(ANamedObject):

    def __init__(self, name="AConfigurable", **kwargs):
        super().__init__(name, **kwargs)
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
        return self.__is_configured

    #
    # Main getter
    #

    def get_dyn_conf(self, key, default=True):
        return self.get_conf(key, dynamic=True, default=default)

    def get_conf(self, key, dynamic=False, default=True):
        val = None
        conf = self.get_conf_obj()
        #Gets conf from either obj or setted configuration
        if conf:
            #From obj
            path = self.get_path()
            if conf.has_option(path, key):
                val = conf.get(path, key)
            elif not dynamic:
                raise KeyError("{}: key {} not found".format(self, key))
        else:
            #From manual set/default
            val = self.__conf.get(key, None)
        if not dynamic:
            if default is False:
                #Checks if val == default
                #Also if dynamic -> not in default
                val_default = self.get_default_conf(key)
                if val_default == val:
                    val = self.__conf.get(key, None)
                    if val_default == val:
                        val = None
            elif val is None:
                #If config has not been found elsewhere
                val = self.get_default_conf(key)
        return val

    #
    # Main setters
    #

    def set_conf_from_dict(self, dic, dynamic=False, force=False):
        if not dynamic:
            bad_keys = [k for k in dic.keys() if k not in self.__default_conf]
            if bad_keys:
                raise KeyError("{} keys not in configuration: {}"\
                                .format(self, bad_keys))
        self.__conf.update(dic)
        if force is True:
            for k, v in dic.items():
                self.set_conf_file(k, v, force=force)
        return True

    def set_dyn_conf(self, key, value=None, force=False):
        return self.set_conf(key, value, dynamic=True, force=force)

    def set_conf(self, key, value=None, dynamic=False, force=False):
        if isinstance(key, dict):
            return self.set_conf_from_dict(key, force=force, dynamic=dynamic)
        if value is None:
            raise RuntimeError("{}: cannot configure None (key: {})"\
                                .format(self, key))
        if not dynamic:
            if key not in self.__default_conf:
                raise KeyError("{}: key not in configuration: {}"\
                                .format(self, key))
        self.__conf[key] = value
        if force is True:
            return self.set_conf_file(key, value, force=force)
        return True

    def set_conf_file(self, key, value, force=False, obj=None):
        if obj is None:
            obj = self.get_conf_obj()
        path = self.get_path()
        if not obj:
            return False
        if not self.__has_section:
            self.__setup_section(obj)
        elif obj.has_option(path, key):
            base_val = obj.get(path, key)
            if not force and base_val != value:
                return False
        obj.set(path, key, value)
        return True

    #
    # Special getters
    #

    def get_default_conf_dict(self):
        return self.__default_conf

    def get_default_conf(self, key):
        return self.__default_conf.get(key)

    def set_conf_obj(self, config_obj):
        self.__conf_obj = config_obj

    def get_conf_obj(self):
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
        if isinstance(dic, dict):
            self.__default_conf.update(dic)

    def _setup_impl(self):
        return True

    #
    # Private
    #

    def __setup_section(self, obj=None):
        conf = obj if obj is not None else self.get_conf_obj()
        added = False
        path = self.get_path()
        if conf and conf.has_section(self.get_path()) is False:
            conf.add_section(self.get_path())
            added = True
        self.__has_section = True
        return added

    def __write_dict_conf(self, dic, obj=None):
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
        obj = obj if obj is not None else self.get_conf_obj()
        return self.__write_dict_conf(self.__default_conf, obj)

    def _set_unconfigured(self):
        self.__is_configured = False
"""
