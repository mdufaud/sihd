#!/usr/bin/python
# coding: utf-8

class IAppContainer(object):

    def __init__(self):
        super().__init__()
        self.__sihd_app = None

    def set_app(self, app):
        self.__sihd_app = app

    def get_app(self):
        return self.__sihd_app
