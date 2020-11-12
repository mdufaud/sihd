#!/usr/bin/python
# coding: utf-8

__all__ = ('ANamedObjectContainer')

import sys
from typing import Union, Tuple

from .ANamedObject import ANamedObject

import sihd
bcolors = sihd.term.colors

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
    def delete_from_tree(path: str):
        no = ANamedObjectContainer.root_find(path)
        if no is None:
            raise KeyError("No container in tree for path: {}".format(path))
        if no.get_parent() is None:
            ANamedObjectContainer.__nocs.pop(no.get_name())
        else:
            no.set_parent(None)

    @staticmethod
    def clear_tree():
        ANamedObjectContainer.__nocs.clear()

    @staticmethod
    def get_root_containers():
        return ANamedObjectContainer.__nocs.keys()

    #
    # Links
    #

    def get_base_child_name(self, obj: object):
        return self.__alias.get(obj.get_path(), None) or obj.get_name()

    def get_alias(self, obj: object):
        return self.__alias.get(obj.get_path(), None)

    def remove_alias(self, obj: object):
        return self.__alias.pop(obj.get_path())

    def on_link(self, name: str, obj: object):
        """ Callback when link resolution is done """
        self.add_child(obj, name=name, replace=True)
        path = obj.get_path()
        alias = self.__alias
        if path not in alias:
            alias[path] = name
        else:
            name = alias[path]
            raise KeyError("{}: Alias '{}' already in path for: {}"\
                            .format(self, path, name))

    def link(self, name: str, path_or_no: Union[str, object]):
        """ Add a link """
        if isinstance(path_or_no, str):
            self.__links[name] = path_or_no
        elif isinstance(path_or_no, ANamedObject):
            self.on_link(name, path_or_no)
        else:
            raise ValueError("{}: Unrecognized link type: {}"\
                                .format(self, path_or_no))

    def __resolve_parent_link(self, name: str, path: str):
        """ Get parent of link and resolve it """
        idx = path.rfind('.')
        if idx == -1:
            raise ValueError("{}.{} link does not exist: {}"\
                                .format(self, name, path))
        parent_path = path[0:idx]
        child_name = path[idx + 1:]
        parent = self.find(parent_path)
        if parent is None:
            raise ValueError("{}.{} link parent not found: {}"\
                                .format(self, name, parent_path))
        err_str = ""
        try:
            return parent._resolve_link(child_name)
        except (KeyError, ValueError) as err:
            err_str = str(err)
        raise ValueError("{}.{} link resolution failed -> {}.{} (reason: {})"\
                            .format(self, name, parent_path, child_name, err_str))

    def _resolve_link(self, name: str, path: str = None):
        """ Process a link and removes it """
        path = path or self.__links.pop(name, None)
        if path is None:
            raise KeyError("{}.{} link does not exist".format(self, name))
        item = self.find(path)
        if isinstance(item, ANamedObject):
            self.on_link(name, item)
        elif item is not None:
            raise ValueError("{}.{} link is not ANamedObject: {}"\
                                .format(self, name, path))
        else:
            self.__resolve_parent_link(name, path)
            self._resolve_link(name, path)

    def process_links(self):
        """ Process all links """
        links = list(self.__links.keys())
        for name in links:
            self._resolve_link(name)

    def _get_links(self) -> dict:
        return self.__links

    def _get_link(self, name: str) -> str:
        """ Returns linked item """
        path = self.__links.get(name, None)
        if path is not None:
            item = self.root_find(path)
            if item is not None:
                return item
        return None

    def is_linked(self, name):
        """ Check if link exist """
        return name in self.__links

    def unlink(self, name):
        """ Removes link """
        self.__links.pop(name, None)

    #
    # Browse tree
    #

    @staticmethod
    def root_find(path):
        """ Returns object in path from root """
        split = path.split('.')
        get_nobj = ANamedObjectContainer.__nocs.get
        obj = get_nobj(split[0], None)
        for child in split[1:]:
            if obj is None:
                break
            obj = obj.get_child(child)
        return obj

    def find(self, path):
        """
            Returns object in path from root or relatively
            
            For hierarchy:
                root:
                    container1:
                        item1
                        item2
                        item3
                    container2:
                        item1
                root2:
                    item

            Absolute:
                Must start from any root container
                :examples:
                    item3.find('root.container2.item1)
                    item3.find('root2.item)

            Relative:
                Starts with a '.', every next '.' means getting parent
                :examples:
                    root.find('.container1.item1')
                    container1.find('..container2')
                    container2.find('.item1')
                    item3.find('..') -> container1
                    item3.find('...') -> root

        """
        if path and path[0] != '.':
            return self.root_find(path)
        split = path.split('.')
        obj = self
        if split[0] == '':
            # For case of starting on dots
            split = split[1:]
        if split and split[-1] == '':
            # For case of finishing on dots
            split = split[:-1]
        if not split:
            return None
        for name in split:
            if obj is None:
                break
            if name == '':
                obj = obj.get_parent()
            else:
                obj = obj.get_child(name)
        return obj

    #
    # Tree information
    #

    def __sort_child(self, child: object) -> tuple:
        return (child.__class__.__name__, self.get_base_child_name(child))

    def get_tree_children(self, lst, desc=False, ident=1, max_rec=0) -> dict:
        """ Returns a dictionnary containing the actual children hierarchy """
        if max_rec > 0 and max_rec == ident:
            return
        children = self.get_children()
        children_lst = sorted(children.values(),
                        key=self.__sort_child)
        for child in children_lst:
            alias = self.get_alias(child)
            child_name = alias if alias else child.get_name()
            child_dict = {
                'name': child_name,
                'class': child.__class__,
                'alias': child.get_path() if alias else None,
                'children': []
            }
            if desc:
                child_dict['attributes'] = child._get_attributes()
            lst.append(child_dict)
            if isinstance(child, ANamedObjectContainer):
                child.get_tree_children(child_dict['children'],
                                        desc=desc,
                                        ident=ident+1,
                                        max_rec=max_rec)

    def get_tree(self, desc=False, ident=1, max_rec=0) -> dict:
        """
            Returns a dictionnary containing the actual
                hierarchy from this container
        """
        dic = {
            'name': self.get_name(),
            'class': self.__class__,
            'children': []
        }
        if desc:
            dic['attributes'] = self._get_attributes()
        self.get_tree_children(dic['children'], desc=desc,
                                ident=ident, max_rec=max_rec)
        return dic

    #
    # Tree printing
    #

    def __build_tree_children(self, lst, desc=False, colored=True, ident=1) -> str:
        if not bcolors:
            colored = False
        s = ""
        for child in lst:
            children = child['children']
            name = child['name']
            cls = child['class'].__name__
            attr = child.get('attributes', None)
            attributes = "\n\t({})".format(', '.join(attr)) \
                    if attr and desc else ""
            if colored:
                if children:
                    name = bcolors.gold(name)
                    cls = bcolors.gold(cls)
                else:
                    name = bcolors.blue(name)
            alias = child['alias']
            link = " --> " + str(alias) if alias else ""
            if link and colored:
                link = bcolors.red(link)
            s += "{}{}: {}{}{}\n".format("  " * ident, name, cls,
                                        link, attributes)
            s += self.__build_tree_children(children, desc=desc,
                                            colored=colored,
                                            ident=ident+1)
        return s

    def print_tree(self, desc=False, colored=True, ident=1, max_rec=0):
        dic = self.get_tree(desc=desc, ident=ident, max_rec=max_rec)
        if not bcolors:
            colored = False
        s = "{}: {}\n".format(dic['name'], dic['class'].__name__)
        children = dic['children']
        if colored and children:
            s = bcolors.gold(s)
        s += self.__build_tree_children(dic['children'], desc=desc,
                                        colored=colored, ident=ident)
        print(s)

    #
    # Children management
    #

    def get_children(self) -> dict:
        return self.__children

    def get_child(self, name: str) -> object:
        return self.__children.get(name, None)

    def add_child(self, child: object, name=None, replace=False):
        """ Add a child to container's hierarchy - Name must be unique """
        if name is None:
            name = child.get_name()
        children = self.__children
        if replace is False and name in children:
            raise KeyError("{}: child '{}' key already used".format(self, name))
        elif replace is True:
            old_child = children.get(name, None)
            if old_child is not None:
                old_child.set_parent(None)
        children[name] = child

    def remove_child(self, child: object):
        """
            Remove a child from container hierarchy
            Having an alias mean we can't rely on child's name
                an alias might have a different name
        """
        alias = self.get_alias(child)
        name = child.get_name() if alias is None else alias
        self.__children.pop(name)
        if alias:
            self.remove_alias(child)
        child.set_parent(None, remove=False)

    def remove_children(self):
        for children in self.__children.values():
            children.set_parent(None, remove=False)
        self.__children.clear()
        self.__alias.clear()

    def is_child(self, child):
        alias = self.get_alias(child)
        name = child.get_name() if alias is None else alias
        return name in self.__children

    #
    # Name
    #

    def set_parent(self, parent, remove=True):
        super().set_parent(parent, remove=remove)
        nos = self.__nocs
        name = self.get_name()
        if parent and name in nos:
            nos.pop(name)
