#!/usr/bin/python
#coding: utf-8

""" System """
import os
import pickle

from .INamedObject import INamedObject

class IDumpable(INamedObject):

    def __init__(self, name="IDumpable"):
        super(IDumpable, self).__init__(name)
        self.__file_magic = None

    def _dump(self):
        """ Del problematic keys to make your dump work """
        state = self.__dict__.copy()
        return state

    def _load(self, state):
        """ You can change here to do specific stuff on loading """
        self.__dict__.update(state.__dict__)

    def __getstate__(self):
        """ Called py pickle.dump(self) """
        to_dump = self._dump()
        return to_dump or {}

    def set_dump_magic(self, magic):
        """ Set a magic number at the end of the file """
        if not isinstance(magic, bytes):
            magic = str(magic).encode("utf-8")
        self.__file_magic = magic

    def __is_magic(self, buf):
        if buf == self.__file_magic:
            return True
        return False

    def dump_to(self, filename, perm="wb+"):
        """ Use pickle to dump object to file """
        with open(filename, perm) as f:
            if self.__file_magic:
                f.write(self.__file_magic)
            pickle.dump(self, f)
        return True

    def load_from(self, filename, perm='rb'):
        """ Use pickle to load object's dump from a file """
        with open(filename, "rb") as f:
            if self.__file_magic:
                magic = f.read(len(self.__file_magic))
                if not self.__is_magic(magic):
                    return False
                data = pickle.load(f)
                self._load(data)
        return False
