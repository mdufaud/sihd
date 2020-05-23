#!/usr/bin/python
#coding: utf-8

#
# System
#
import sys

class ANamedObject(object):

    def __init__(self, name, parent=None):
        self.__name = None
        self.__parent = None
        if parent and not isinstance(parent, ANamedObjectContainer):
            raise TypeError("For object {}: parent {} is not a NamedObjectContainer".format(name, parent))
        self.set_name(name)
        self.set_parent(parent)

    def __del__(self):
        self.set_parent(None)

    #
    # Browse hierarchy
    #

    @staticmethod
    def get_namedobject_path(no):
        lst = [no.get_name()]
        parent = no.get_parent()
        while parent is not None:
            lst.append(parent.get_name())
            parent = parent.get_parent()
        lst.reverse()
        return '.'.join(lst)

    def get_path(self):
        return self.get_namedobject_path(self)

    #
    # Parent
    #

    def get_root(self):
        parent = self.get_parent()
        while parent is not None:
            parent = parent.get_parent()
        return parent

    def set_parent(self, parent, remove=True):
        oldparent = self.get_parent()
        if oldparent is not None:
            if parent == oldparent:
                return False
            if remove:
                oldparent.remove_child(self)
        if parent:
            parent.add_child(self)
        self.__parent = parent

    def get_parent(self):
        return self.__parent

    #
    # Name
    #

    def set_name(self, name):
        parent = self.get_parent()
        if parent:
            parent.remove_child(self)
        self.__name = name
        if parent:
            parent.add_child(self)

    def get_name(self):
        return self.__name

    def _get_attributes(self):
        attr_lst = []
        return attr_lst

    def get_description(self):
        l = self._get_attributes()
        return "{}({})".format(self.__class__.__name__, ", ".join(l))

    def __str__(self):
        s = self.get_name()
        parent = self.get_parent()
        if parent:
            s = "{}.{}".format(parent, s)
        return s

from .ANamedObjectContainer import ANamedObjectContainer
