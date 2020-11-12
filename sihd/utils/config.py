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

    def __path_assign(self, val, key, value):
        if isinstance(val, dict):
            try:
                val[key] = value
            except KeyError as exc:
                raise KeyError("Conf '{}': Key not found: {}"\
                               .format(self.key, key)) from exc
        elif isinstance(val, (list, tuple)):
            try:
                val[int(key)] = value
            except IndexError as exc:
                raise KeyError("Conf '{}': Index not found: {}"\
                               .format(self.key, key)) from exc
        else:
            raise KeyError("Conf '{}': For key '{}' cannot assign type: {}"
                           .format(self.key, key, type(val)))

    def __path_browse(self, val, key):
        if isinstance(val, dict):
            try:
                return val[key]
            except KeyError as exc:
                raise KeyError("Conf '{}': Key not found: {}".format(self.key, key)) from exc
        elif isinstance(val, (list, tuple)):
            try:
                return val[int(key)]
            except IndexError as exc:
                raise KeyError("Conf '{}': Index not found: {}".format(self.key, key)) from exc
        else:
            raise KeyError("Conf '{}': Cannot go further with key: {}".format(self.key, key))

    def deep_set(self, keylst, value, default=True):
        if not keylst:
            return
        val = self.value
        if val is None and default is True:
            val = self.default
        for key in keylst[:-1]:
            val = self.__path_browse(val, key)
        self.__path_assign(val, keylst[-1], value)
        return val


    def find(self, keylst, default=True):
        if not keylst:
            return
        val = self.value
        if val is None and default is True:
            val = self.default
        for key in keylst:
            val = self.__path_browse(val, key)
        return val

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

    def set(self, path, value, dynamic=False, force=False):
        split = path.split('.')
        if split:
            key = split[0]
            split = split[1:]
        else:
            key = path
        conf = self.dict.get(key, None)
        if conf is None:
            if dynamic is False:
                raise KeyError("{}: key not in configuration: {}".format(self.name, path))
            if split:
                return
            conf = self.add_default(key, value, expose=False)
        if split:
            conf.deep_set(split, value, force)
        else:
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

    def get(self, path, ret=None, dynamic=False, default=True):
        """
            Get value from conf

            :param path: configuration path key
            :param ret: returned value when failed to find desired value
            :param default: returns None if value is still default
            :param dynamic: do not raise key error if config key not found
            :return: value or None if not found
        """
        split = path.split('.')
        if split:
            key = split[0]
            split = split[1:]
        else:
            key = path
        if key not in self.dict:
            if dynamic is True:
                return ret
            else:
                raise KeyError("{}: key {} not found".format(self.name, path))
        conf = self.dict[key]
        if split:
            exceptions = (KeyError) if dynamic is True else ()
            try:
                val = conf.find(split)
            except exceptions:
                val = None
        else:
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
