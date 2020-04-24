#!/usr/bin/python
#coding: utf-8

class IAppContainer(object):

    def __init__(self):
        super().__init__()
        self.__wm_app = None

    def set_app(self, app):
        self.__wm_app = app

    def get_app(self):
        return self.__wm_app
