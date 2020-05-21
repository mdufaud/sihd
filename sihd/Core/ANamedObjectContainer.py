#!/usr/bin/python
#coding: utf-8

#
# System
#
import sys

from .ANamedObject import ANamedObject

class ANamedObjectContainer(ANamedObject):

    __nocs = {}

    def __init__(self, name, parent=None, **kwargs):
        self.__children = dict()
        self.__links = dict()
        self.__alias = dict()
        super().__init__(name, parent=parent, **kwargs)
        if parent is None:
            nos = self.__nocs
            name = self.get_name()
            if name in nos:
                raise KeyError("Container '{}' key already used".format(name))
            nos[name] = self

    def __del__(self):
        if self.__nocs.pop(self.get_name(), None) is None:
            return
        self.set_parent(None)
        self.remove_children()

    #
    # Utility
    #

    @staticmethod
    def delete_from_tree(path):
        no = ANamedObjectContainer.root_find(path)
        if no is None:
            raise KeyError("No child in path: {}".format(path))
        if no.get_parent() is None:
            ANamedObjectContainer.__nocs.pop(no.get_name())
        else:
            no.set_parent(None)

    @staticmethod
    def clear_tree():
        ANamedObjectContainer.__nocs.clear()

    #
    # Links
    #

    def get_alias(self, obj):
        return self.__alias.get(obj.get_name(), None)

    def remove_alias(self, obj):
        return self.__alias.pop(obj.get_name())

    def on_link(self, name, obj):
        self.add_child(obj, name=name, replace=True)
        self.__alias[obj.get_name()] = name

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
            item = self.root_find(path)
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
            item = self.root_find(path)
            if item is not None:
                return item
        return None

    def is_linked(self, name):
        return name in self.__links

    #
    # Browse hierarchy
    #

    @staticmethod
    def root_find(path):
        split = path.split('.')
        get_nobj = ANamedObjectContainer.__nocs.get
        obj = get_nobj(split[0], None)
        for child in split[1:]:
            if obj is None:
                break
            obj = obj.get_child(child)
        return obj

    def find(self, path):
        split = path.split('.')
        obj = self
        if split[0] == '':
            # For case of starting on dots
            split = split[1:]
        if split[-1] == '':
            # For case of finishing on dots
            split = split[:-1]
        for name in split:
            if obj is None:
                break
            if name == '':
                obj = obj.get_parent()
            else:
                obj = obj.get_child(name)
        if obj == self:
            return None
        return obj

    #
    # Parent
    #

    def get_tree_children(self, ident=1):
        children = self.get_children()
        children_lst = list(children.keys())
        children_lst.sort()
        s = ""
        for child_name in children_lst:
            child = children[child_name]
            s += "{}{}: {}".format("  " * ident,
                                    child_name,
                                    child.__class__.__name__)
            if child.get_name() != child_name:
                s += " -> {}".format(child.get_path())
            s += "\n"
            if isinstance(child, ANamedObjectContainer):
                s += child.get_tree_children(ident=ident+1)
        return s


    def get_tree(self, ident=1):
        s = "{}: {}\n".format(self.get_name(),
                            self.__class__.__name__)
        s += self.get_tree_children(ident=ident)
        return s

    def print_tree(self):
        print(self.get_tree())

    #
    # Children
    #

    def get_children(self):
        return self.__children

    def get_child(self, name):
        return self.__children.get(name, None)

    def add_child(self, child, name=None, replace=False):
        """ No parent replacement because of links """
        if name is None:
            name = child.get_name()
        children = self.__children
        if replace is False and name in children:
            raise KeyError("{}: child '{}' key already used".format(self, name))
        elif replace is True:
            old_child = children.get(name, None)
            if old_child is None:
                raise KeyError("{}: no such child {}".format(self, name))
            old_child.set_parent(None)
        children[name] = child

    def remove_child(self, child):
        alias = self.get_alias(child)
        name = child.get_name() if alias is None else alias
        self.__children.pop(name)
        if alias:
            self.remove_alias(child)

    def remove_children(self):
        for children in self.__children.values():
            children.set_parent(None, remove=False)
        self.__children.clear()

    #
    # Name
    #

    def set_parent(self, parent, remove=True):
        super().set_parent(parent, remove=remove)
        nos = self.__nocs
        name = self.get_name()
        if parent and name in nos:
            nos.pop(name)
