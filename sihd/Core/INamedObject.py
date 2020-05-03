#!/usr/bin/python
#coding: utf-8

""" System """
import sys

class INamedObject(object):

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
        get_nobj = INamedObject.__namedobjects.get
        obj = get_nobj(split[0], None)
        for child in split[1:]:
            if obj is None:
                break
            obj = obj.get_namedobject_children(child)
        return obj

    """ Parent """

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
            parent.add_namedobject_children(self)

    def get_namedobject_parent(self):
        return self.__parent

    """ Children """

    def get_namedobject_children(self, children_name):
        return self.__children.get(children_name, None)

    def add_namedobject_children(self, children):
        self.__children[children.get_name()] = children

    def remove_namedobject_children(self, children):
        self.__children.pop(children.get_name())

    """ Name """

    def set_name(self, name):
        #Cleanup links
        parent = self.get_namedobject_parent()
        no = self.__namedobjects
        no.pop(self.get_name(), None)
        if parent:
            parent.remove_namedobject_children(self)
        #Set
        self.__name = name
        #Relink
        no[name] = self
        if parent:
            parent.add_namedobject_children(self)

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

    """ Utils """

    def say(self, s, prefix="", **kwargs):
        print("{0}: {1}{2}".format(self.get_name(), prefix, s), **kwargs)

    def error(self, err, **kwargs):
        self.say(err, prefix="Error: ", file=sys.stderr)
        return False

    """

    def get_namedobject_parent_classes(self):
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

    def get_namedobject_parent_classes_str(self):
        lst = self.get_namedobject_parent_classes()
        s = ""
        for kls in reversed(lst[:-1]):
            if s != "":
                s += "."
            if len(kls) > 1:
                s += "(" + " | ".join(kls) + ")"
            else:
                s += "".join(kls)
        return s

    """
