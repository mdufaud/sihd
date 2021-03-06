#!/usr/bin/python
#coding: utf-8

""" System """
import sys

class INamedObject(object):

    def __init__(self, name):
        self.set_name(name)

    def set_name(self, name):
        self.__name = name

    def get_name(self):
        return self.__name

    def get_description(self):
        return None

    def say(self, s, prefix="", **kwargs):
        print("{0}: {1}{2}".format(self.get_name(), prefix, s), **kwargs)

    def error(self, err, **kwargs):
        self.say(err, prefix="Error: ", file=sys.stderr)
        return False

    def get_parent_classes(self):
        kls = self.__class__
        lst = [[kls.__name__]]
        while kls is not None:
            kls = kls.__bases__
            if len(kls) > 0:
                lst.append([el.__name__ for el in kls])
                kls = kls[0]
            else:
                kls = None
        return lst

    def get_parent_classes_str(self):
        lst = self.get_parent_classes()
        s = ""
        for kls in reversed(lst[:-1]):
            if s != "":
                s += "."
            if len(kls) > 1:
                s += "(" + " | ".join(kls) + ")"
            else:
                s += "".join(kls)
        return s

    def __str__(self):
        desc = self.get_description()
        s = "{0}(name={1}".format(self.__class__.__name__, self.get_name())
        if desc:
            s += "{0}, {1})".format(s, desc)
        else:
            s += ")"
        return s
