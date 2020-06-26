from sihd.Core.ANamedObject import ANamedObject
from sihd.Core.ANamedObjectContainer import ANamedObjectContainer

def leaf(name, parent=None):
    return ANamedObject(name, parent)

def node(name, parent=None):
    return ANamedObjectContainer(name, parent)

def find(path):
    return ANamedObjectContainer.root_find(path)

def clear():
    ANamedObjectContainer.clear_tree()

def delete(path):
    ANamedObjectContainer.delete_from_tree(path)

def dump(*args, **kwargs):
    keys = ANamedObjectContainer.get_root_containers()
    for key in keys:
        root = find(key)
        if root:
            root.print_tree(*args, **kwargs)
