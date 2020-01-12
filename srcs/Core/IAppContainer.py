#!/usr/bin/python
#coding: utf-8

""" System """


class IAppContainer(object):

    def __init__(self, *args, **kwargs):
        self.__wm_app = None
        super(IAppContainer, self).__init__(*args, **kwargs)

    def set_app(self, app):
        self.__wm_app = app

    def get_app(self):
        return self.__wm_app
