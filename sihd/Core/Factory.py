#!/usr/bin/python
# coding: utf-8

#TODO or erase

#
# System
#
import sys

from .ANamedObject import ANamedObject

class Factory(object):

    def __init__(self):
        return

    @staticmethod
    def __rget(name, cls, depth, max_depth,
                 visited, debug=False):
        if depth == max_depth:
            return None
        for child in cls.__subclasses__():
            if child in visited:
                continue
            if child.__name__ == name:
                return child
            visited.append(child)
            if debug is True:
                for i in range(depth):
                    print("\t", end="")
                print(child.__name__)
            ret = Factory.__rget(name, child, depth + 1,
                                max_depth, visited, debug=debug)
            if ret is not None:
                return ret

    @staticmethod
    def get(name, depth=0, max_depth=7, *args, **kwargs):
        return Factory.__rget(name, ANamedObject, depth, max_depth,
                                [], *args, **kwargs)

    @staticmethod
    def __rfind(name, cls, depth, max_depth,
                visited, results,
                debug=False, interfaces=False):
        if depth == max_depth:
            return None
        for child in cls.__subclasses__():
            if child in visited:
                continue
            ret = False
            if interfaces is False and child.__name__[0] == "I":
                pass
            elif child.__name__.upper().find(name.upper()) >= 0:
                results.append(child)
                ret = True
            visited.append(child)
            if debug is True:
                for i in range(depth):
                    print("\t", end="")
                print(child.__name__ + (' (matched)' if ret is True else ''))
            Factory.__rfind(name, child, depth + 1, max_depth,
                            visited, results, debug=debug)

    @staticmethod
    def find(name, depth=0, max_depth=7, *args, **kwargs):
        lst = []
        Factory.__rfind(name, ANamedObject, depth, max_depth,
                        visited=[], results=lst, *args, **kwargs)
        return lst
