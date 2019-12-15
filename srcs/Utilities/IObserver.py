#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .INamedObject import INamedObject

class IObserver(INamedObject):

    def __init__(self, name="IObserver"):
        super(IObserver, self).__init__(name)

    def handle(self, observable, data):
        raise NotImplementedError("Handle not implemented")

    def on_error(self, observable, error):
        return

    def on_info(self, observable, info):
        return
