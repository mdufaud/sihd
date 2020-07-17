#!/usr/bin/python
# coding: utf-8

from collections import OrderedDict

class ConfigObject(object):

    def __init__(self, key, value=None, default=None, forced=False, infile=True):
        self.key = key
        self.forced = False
        self.value = None
        default = str(default)
        self.default = str(default)
        if value is None:
            self.set(default , forced)
            self.setted = False
        else:
            self.set(value , forced)
        self.infile = infile

    def set(self, value, force=False):
        if self.forced:
            return
        if isinstance(value, bool):
            value = int(value)
        self.forced = force
        self.value = str(value)
        self.setted = True

    def __str__(self):
        return "{}: {} - default={} forced={} infile={}".format(
                self.key, self.value, self.default, self.forced, self.infile)

class ConfigApi(object):

    def __init__(self, section_name):
        self.name = section_name
        self.conf = OrderedDict()
        self.obj = None
        self.configured = False

    def set_configuration_object(self, obj):
        self.obj = obj

    def dump(self):
        print(self.name + ": ")
        for key, conf in self.conf.items():
            print("\t" + str(conf))

    #
    # Default
    #

    def add_default(self, key, default, **kwargs):
        if key in self.conf:
            raise KeyError("{}: key for default already exists: {}".format(self.name, key))
        self.conf[key] = ConfigObject(key, default=default, **kwargs)

    def add_defaults(self, dic, **kwargs):
        for key, conf in dic.items():
            if isinstance(conf, (list, tuple)):
                default, dic = conf
                dic.update(kwargs)
                self.add_default(key, default, **dic)
            else:
                self.add_default(key, conf, **kwargs)

    #
    # Set
    #

    def setall(self, dic, **kwargs):
        for key, value in dic.items():
            self.set(key, value, **kwargs)

    def set_force(self, key, value, **kwargs):
        kwargs['force'] = True
        self.set(key, value, **kwargs)

    def set_dynamic(self, key, value, **kwargs):
        kwargs['dynamic'] = True
        self.set(key, value, **kwargs)

    def set(self, key, value, dynamic=False, force=False):
        if value is None:
            raise RuntimeError("{}: for key '{}' cannot configure None".format(self.name, key))
        conf = self.conf.get(key, None)
        if conf is None:
            if not dynamic:
                raise KeyError("{}: key not in configuration: {}".format(self.name, key))
            self.add_default(key, value, infile=False)
            conf = self.conf.get(key, None)
        conf.set(value, force)

    def is_set(self, key):
        return self.conf[key].setted

    #
    # Get
    #

    def get_dynamic(self, key):
        return self.get(key, dynamic=True)

    def get_default(self, key):
        conf = self.conf.get(key, None)
        return conf and conf.default or None

    def get(self, key, dynamic=False, default=True):
        """
            Get value from conf - Either from file or dict

            :param key: configuration key
            :param default: returns None if value is still default
            :param dynamic: do not raise key error if config key not found
            :return: value or None if not found
        """
        val = None
        name = self.name
        obj = self.obj
        conf = self.conf.get(key, None)
        if obj is not None:
            if obj.has_option(name, key):
                val = obj.get(name, key)
            # no key error because it can be either dynamic or not in file
        if conf is not None and val is None:
            val = conf.value
        # if dynamic and not set do not raise key error
        if dynamic:
            return val
        elif conf is None:
            raise KeyError("{}: key {} not found".format(name, key))
        # if not default, must return None if setted value is default
        if default is False:
            valdefault = conf.default
            if obj is not None and valdefault == val:
                # if value from config file is default, switch to config value
                val = conf.value
            if valdefault == val:
                val = None
        elif val is None:
            # else get default value
            val = conf.default
        return val

    #
    # ConfigParser
    #

    def has_section(self):
        obj = self.obj
        if obj is None:
            return False
        return obj.has_section(self.name)

    def save(self, obj=None):
        obj = obj or self.obj
        if obj is None:
            return False
        name = self.name
        if not obj.has_section(name):
            obj.add_section(name)
        for key, conf in self.conf.items():
            if conf.infile is True:
                obj.set(name, key, str(conf.value))
        return True
