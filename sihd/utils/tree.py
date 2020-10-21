#!/usr/bin/python
# coding: utf-8

from sihd.core.ANamedObject import ANamedObject
from sihd.core.ANamedObjectContainer import ANamedObjectContainer

def add_leaf(name, parent=None):
    return ANamedObject(name, parent)

def add_node(name, parent=None):
    return ANamedObjectContainer(name, parent)

def find(path):
    return ANamedObjectContainer.root_find(path)

def clear():
    ANamedObjectContainer.clear_tree()

def remove(path):
    ANamedObjectContainer.delete_from_tree(path)

def dump(*args, **kwargs):
    keys = ANamedObjectContainer.get_root_containers()
    for key in keys:
        root = find(key)
        if root:
            root.print_tree(*args, **kwargs)
