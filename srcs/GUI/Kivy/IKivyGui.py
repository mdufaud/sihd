#!/usr/bin/python
#coding: utf-8

""" System """


from ..IGui import IGui

class IKivyGui(IGui):

    def __init__(self, app=None, name="IKivyGui"):
        super(IKivyGui, self).__init__(app=app, name=name)

    def _start_impl(self):
        return True

    def _stop_impl(self):
        return True

    def _resume_impl(self):
        return True

    def _pause_impl(self):
        return True
