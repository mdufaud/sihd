#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

class IAppContainer(object):

    def set_app(self, app):
        self.__wm_app = app

    def get_app(self):
        return self.__wm_app
