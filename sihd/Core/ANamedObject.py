#!/usr/bin/python
#coding: utf-8

#
# System
#
import sys

class ANamedObject(object):

    __namedobjects = {}

    def __init__(self, name, parent=None):
        self.__name = None
        self.__parent = None
        self.__children = dict()
        self.set_name(name)
        self.set_namedobject_parent(parent)

    def __delete__(self):
        self.__namedobjects.pop(self.get_name())

    @staticmethod
    def find_namedobject(path):
        split = path.split('.')
        get_nobj = ANamedObject.__namedobjects.get
        obj = get_nobj(split[0], None)
        for child in split[1:]:
            if obj is None:
                break
            obj = obj.get_namedobject_children(child)
        return obj

    @staticmethod
    def get_namedobject_path(no):
        lst = [no.get_name()]
        parent = no.get_namedobject_parent()
        while parent is not None:
            lst.append(parent)
            parent = parent.get_namedobject_parent()
        lst.reverse()
        return '.'.join(lst)

    #
    # Parent
    #

    def get_namedobject_root(self):
        parent = self.get_namedobject_parent()
        while parent is not None:
            parent = parent.get_namedobject_parent()
        return parent

    def set_namedobject_parent(self, parent):
        oldparent = self.get_namedobject_parent()
        if oldparent is not None:
            if parent == oldparent:
                return False
            oldparent.remove_namedobject_children(self)
        self.__parent = parent
        if parent:
            parent.set_namedobject_children(self)

    def get_namedobject_parent(self):
        return self.__parent
    
    #
    # Children
    #

    def get_namedobject_children(self, children_name):
        return self.__children.get(children_name, None)

    def set_namedobject_children(self, children):
        self.__children[children.get_name()] = children

    def remove_namedobject_children(self, children):
        self.__children.pop(children.get_name())

    #
    # Name
    #

    def set_name(self, name):
        #Cleanup links
        parent = self.get_namedobject_parent()
        nos = self.__namedobjects
        if parent and parent.get_namedobject_children(name) is not None\
            or parent is None and nos.get(name, None) is not None:
                raise ValueError("Namedobject {} already exists".format(name))
        #Set
        self.__name = name
        #Link
        nos[name] = self
        if parent:
            parent.set_namedobject_children(self)

    def get_name(self):
        return self.__name

    def _get_attributes(self):
        attr_lst = ['name=' + self.get_name()]
        return attr_lst

    def get_description(self):
        l = self._get_attributes()
        return "{}({})".format(self.__class__.__name__, ", ".join(l))

    def __str__(self):
        s = self.get_name()
        parent = self.get_namedobject_parent()
        if parent:
            s = "{}.{}".format(parent, s)
        return s
