#!/usr/bin/python
# coding: utf-8

import json
from collections import OrderedDict

class ConfigObject(object):

    def __init__(self, key, value=None, default=None, forced=False, expose=True):
        self.key = key
        self.forced = False
        self.value = None
        self.default = default
        if value is None:
            self.set(default , forced)
            self.setted = False
        else:
            self.set(value , forced)
        self.expose = expose

    def set(self, value, force=False):
        if self.forced:
            return
        self.forced = force
        self.value = value
        self.setted = True

    def reset(self):
        self.forced = False
        self.setted = False
        self.value = None

    def __str__(self):
        return "{}: {} (default={}, forced={}, expose={})".format(
                self.key, self.value, self.default, self.forced, self.expose)

class ConfigApi(object):

    def __init__(self, section_name):
        self.name = section_name
        self.dict = OrderedDict()
        self.configured = False

    def dump(self):
        print(self.name + ": ")
        for key, conf in self.dict.items():
            print("\t" + str(conf))

    def to_dict(self):
        ret = {}
        for key, conf in self.dict.items():
            if conf.expose is True:
                val = conf.value
                if val is None:
                    val = conf.default
                ret[key] = val
        return ret

    def has(self, key):
        return key in self.dict

    #
    # Default
    #

    def add_default(self, key, default, **kwargs):
        if key in self.dict:
            raise KeyError("{}: key for default already exists: {}".format(self.name, key))
        obj = ConfigObject(key, default=default, **kwargs)
        self.dict[key] = obj
        return obj

    def add_defaults(self, dic, **kwargs):
        for key, conf in dic.items():
            self.add_default(key, conf, **kwargs)

    #
    # Set
    #

    def set_force(self, key, value, **kwargs):
        kwargs['force'] = True
        self.set(key, value, **kwargs)

    def set_dynamic(self, key, value, **kwargs):
        kwargs['dynamic'] = True
        self.set(key, value, **kwargs)

    def set(self, key, value, dynamic=False, force=False):
        conf = self.dict.get(key, None)
        if conf is None:
            if not dynamic:
                raise KeyError("{}: key not in configuration: {}".format(self.name, key))
            conf = self.add_default(key, value, expose=False)
        conf.set(value, force)

    def is_set(self, key):
        return self.dict[key].setted

    def reset(self, key):
        self.dict[key].reset()

    #
    # Get
    #

    def get_dynamic(self, key, **kwargs):
        return self.get(key, dynamic=True, **kwargs)

    def get_default(self, key, **kwargs):
        conf = self.dict.get(key, None, **kwargs)
        return conf and conf.default or None

    def get(self, key, ret=None, dynamic=False, default=True):
        """
            Get value from conf

            :param key: configuration key
            :param ret: returned value when failed to find desired value
            :param default: returns None if value is still default
            :param dynamic: do not raise key error if config key not found
            :return: value or None if not found
        """
        if key not in self.dict:
            if dynamic is True:
                return ret
            else:
                raise KeyError("{}: key {} not found".format(self.name, key))
        conf = self.dict[key]
        val = conf.value
        if default is False and conf.default == val:
            val = ret
        elif val is None:
            val = conf.default
        return val

    #
    # Load
    #

    def load_obj(self, obj, **kwargs):
        keys = self.dict.keys()
        for key in keys:
            self.set(key, obj.get(key, None), **kwargs)

    def load_json(self, conf, **kwargs):
        self.load_dict(json.loads(conf), **kwargs)

    def load_dict(self, dic, **kwargs):
        for key, value in dic.items():
            self.set(key, value, **kwargs)
        self.configured = True

    def load(self, conf, **kwargs):
        ret = True
        if isinstance(conf, str):
            self.load_json(conf, **kwargs)
        elif isinstance(conf, dict):
            self.load_dict(conf, **kwargs)
        elif isinstance(conf, object):
            self.load_object(conf, **kwargs)
        else:
            ret = False
        return ret
