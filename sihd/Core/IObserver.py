#!/usr/bin/python
#coding: utf-8

""" System """
from .ANamedObject import ANamedObject

class IObserver(ANamedObject):

    def __init__(self, name="IObserver", **kwargs):
        super(IObserver, self).__init__(name, **kwargs)

    def on_notify(self, observable):
        raise NotImplementedError("on_notify not implemented")
