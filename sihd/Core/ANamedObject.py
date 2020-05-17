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
        self.__links = dict()
        self.__parent = parent
        self.set_name(name)
        if parent:
            parent.set_child(self)

    def __lt__(self, other):
        if isinstance(other, ANamedObject):
            return self.get_name() < other.get_name()
        return super().__lt__(other)

    def __del__(self):
        if self.__namedobjects.pop(self.get_name(), None) is None:
            return
        parent = self.get_parent()
        if parent:
            parent.remove_child(self)
        for children in self.__children.values():
            children.set_parent(None, remove=False)

    #
    # Utility
    #

    @staticmethod
    def delete_from_tree(path):
        no = ANamedObject.find(path)
        if no is None:
            raise KeyError("No child in path {}".format(path))
        if no.get_parent() is None:
            ANamedObject.__namedobjects.pop(no.get_name())
        else:
            no.set_parent(None)

    @staticmethod
    def clear_tree():
        ANamedObject.__namedobjects.clear()

    #
    # Links
    #

    def on_link(self, name, obj):
        self.set_child(obj, name=name)

    def link(self, name, path_or_no):
        """ Add a link """
        if isinstance(path_or_no, str):
            self.__links[name] = path_or_no
        elif isinstance(path_or_no, ANamedObject):
            self.on_link(name, path_or_no)
        else:
            raise ValueError("Unrecognized link type: " + str(path_or_no))

    def process_links(self):
        """ Process all links """
        for name, path in self.__links.items():
            item = self._get_link(name)
            if isinstance(item, ANamedObject):
                self.on_link(name, item)
            elif item is not None:
                raise ValueError("Linked object is not a ANamedObject: {}"\
                                    .format(item))
            else:
                raise ValueError("Linked object does not exists: {}"\
                                    .format(path))
        self.__links.clear()

    def _get_links(self) -> dict:
        return self.__links

    def _get_link(self, name):
        path = self.__links.get(name, None)
        if path is not None:
            item = self.find(path)
            if item is not None:
                return item
        return None

    def is_linked(self, name):
        return name in self.__links

    #
    # Browse hierarchy
    #

    @staticmethod
    def find(path):
        split = path.split('.')
        get_nobj = ANamedObject.__namedobjects.get
        obj = get_nobj(split[0], None)
        for child in split[1:]:
            if obj is None:
                break
            obj = obj.get_child(child)
        return obj

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
        self.__parent = parent
        if parent:
            parent.set_child(self)

    def get_parent(self):
        return self.__parent

    def dump_tree(self, desc=False, ident=1, name=None, parent=None):
        children = self.get_children()
        children_lst = list(children.keys())
        children_lst.sort()
        if parent is None:
            parent = self.get_parent()
        if name is None:
            name = self.get_name()
        s = name
        if desc:
            s += " - {}".format(self.get_description())
        else:
            s += " - {}".format(self.__class__.__name__)
        if parent and name != self.get_name():
            s += " -> " + self.get_path()
        s += "{}\n".format(":" if children else "")
        for child_name in children_lst:
            child = children[child_name]
            s += "  " * ident + child.dump_tree(desc=desc, ident=ident+1, 
                                                name=child_name, parent=self)
        return s
    #
    # Children
    #

    def get_children(self):
        return self.__children

    def get_child(self, name):
        return self.__children.get(name, None)

    def set_child(self, child, name=None):
        """ No parent replacement because of links """
        if name is None:
            name = child.get_name()
        self.__children[name] = child

    def remove_child(self, children):
        self.__children.pop(children.get_name())

    #
    # Name
    #

    def set_name(self, name):
        parent = self.get_parent()
        nos = self.__namedobjects
        if parent and parent.get_child(name) is not None\
            or parent is None and nos.get(name, None) is not None:
                raise ValueError("Namedobject {} already exists".format(name))
        self.__name = name
        if parent:
            parent.set_child(self)
        else:
            nos[name] = self

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
        parent = self.get_parent()
        if parent:
            s = "{}.{}".format(parent, s)
        return s
