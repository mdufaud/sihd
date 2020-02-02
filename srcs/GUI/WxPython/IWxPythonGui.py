#!/usr/bin/python
#coding: utf-8

""" System """

import os
import os.path
import sys
import logging

from sihd.srcs.GUI.IGui import IGui
from sihd.srcs import Core

wx = None

class IWxPythonGui(IGui):

    def __init__(self, app=None, name="IWxPythonGui"):
        global wx
        if wx is None:
            import wx
        super(IWxPythonGui, self).__init__(app=app, name=name)

    def gui_loop(self, **kwargs):
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
            self.gui_loop()
        return True

    def _stop_impl(self):
        return True
