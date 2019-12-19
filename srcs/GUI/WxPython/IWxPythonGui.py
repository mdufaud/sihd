#!/usr/bin/python
#coding: utf-8

""" System """
from __future__ import print_function

import os
import os.path
import sys
import logging

from ..IGui import IGui
from ... import Utilities

import wx

class IWxPythonGui(IGui):

    def __init__(self, app=None, name="IWxPythonGui"):
        super(IWxPythonGui, self).__init__(app=app, name=name)
        if app:
            self.get_app().set_loop(self.start_gui)

    def start_gui(self):
        app = wx.App(False)
        self._app = app
        self._make_frames(app)
        app.MainLoop()

    def _make_frames(self, app):
        raise NotImplemented

    # Services

    def _pause_impl(self):
        return True

    def _resume_impl(self):
        return True

    def _start_impl(self):
        if self.get_app() is None:
            self.start_gui()
        return True

    def _stop_impl(self):
        return True
