#!/usr/bin/python
#coding: utf-8

""" System """
from .INamedObject import INamedObject

class IObserver(INamedObject):

    def __init__(self, name="IObserver"):
        super(IObserver, self).__init__(name)

    def on_notify(self, observable):
        raise NotImplementedError("on_notify not implemented")

    def get(self):
        return None
