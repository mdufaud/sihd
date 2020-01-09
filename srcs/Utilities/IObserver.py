#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

from .INamedObject import INamedObject

class IObserver(INamedObject):

    def __init__(self, name="IObserver"):
        super(IObserver, self).__init__(name)

    def on_notify(self, observable, *args, **kwargs):
        raise NotImplementedError("on_notify not implemented")

    def on_error(self, observable, *args):
        return

    def on_info(self, observable, *args):
        return
