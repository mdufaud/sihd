#!/usr/bin/python
#coding: utf-8

#
# System
#
import os
import pickle

from .ANamedObject import ANamedObject

class ADumpable(ANamedObject):

    def __init__(self, name="ADumpable"):
        super(ADumpable, self).__init__(name)
        self.__file_magic = None

    def on_dump(self) -> dict:
        """ You can add or remove stuff from dumping """
        return {}

    def on_load(self, dic: dict):
        """ You can change here to do specific stuff on loading """
        pass

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
        dump = self._dump()
        if dump is None:
            return True
        with open(filename, perm) as f:
            if self.__file_magic:
                f.write(self.__file_magic)
            pickle.dump(dump, f)
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
        return True
