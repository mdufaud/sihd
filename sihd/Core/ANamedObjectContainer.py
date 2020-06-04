#!/usr/bin/python
#coding: utf-8

#
# System
#
import sys

from .ANamedObject import ANamedObject

try:
    from sihd.Tools.shell.term_colors import bcolors
except ImportError:
    bcolors = None

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
            raise KeyError("No container in path: {}".format(path))
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

    def get_base_child_name(self, obj):
        return self.__alias.get(obj.get_path(), None) or obj.get_name()

    def get_alias(self, obj):
        return self.__alias.get(obj.get_path(), None)

    def remove_alias(self, obj):
        return self.__alias.pop(obj.get_path())

    def on_link(self, name, obj):
        """ Callback when link resolution is done """
        self.add_child(obj, name=name, replace=True)
        self.__alias[obj.get_path()] = name

    def link(self, name, path_or_no):
        """ Add a link """
        if isinstance(path_or_no, str):
            self.__links[name] = path_or_no
        elif isinstance(path_or_no, ANamedObject):
            self.on_link(name, path_or_no)
        else:
            raise ValueError("{}: Unrecognized link type: {}"\
                                .format(self, path_or_no))

    def process_links(self):
        """ Process all links """
        for name, path in self.__links.items():
            item = self.find(path)
            if isinstance(item, ANamedObject):
                self.on_link(name, item)
            elif item is not None:
                raise ValueError("{}: Linked object is not a ANamedObject: {}"\
                                    .format(self, item))
            else:
                raise ValueError("{}: Linked object does not exists: {}"\
                                    .format(self, path))
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
    # Parent
    #

    def __sort_child(self, child):
        return (child.__class__.__name__, self.get_base_child_name(child))

    def get_tree_children(self, lst, desc=False, ident=1, max_rec=0):
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

    def get_tree(self, desc=False, ident=1, max_rec=0):
        dic = {
            'name': self.get_name(),
            'class': self.__class__,
            'children': []
        }
        if desc:
            dic['attributes'] = self._get_attributes()
        self.get_tree_children(dic['children'],
                                desc=desc,
                                ident=ident,
                                max_rec=max_rec)
        return dic

    def __build_tree_children(self, lst, desc=False, colored=True, ident=1):
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
            s += "{}{}: {}{}{}\n".format("  " * ident, name, cls, link, attributes)
            s += self.__build_tree_children(children,
                                            desc=desc,
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
            if old_child is not None:
                old_child.set_parent(None)
        children[name] = child

    def remove_child(self, child):
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

    #
    # Name
    #

    def set_parent(self, parent, remove=True):
        super().set_parent(parent, remove=remove)
        nos = self.__nocs
        name = self.get_name()
        if parent and name in nos:
            nos.pop(name)
