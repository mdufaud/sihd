#!/usr/bin/python
# coding: utf-8

class IObserver(object):

    def on_notify(self, observable):
        return False
