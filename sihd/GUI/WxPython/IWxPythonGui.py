#!/usr/bin/python
#coding: utf-8

""" System """

import os
import os.path
import sys
import logging

from sihd.GUI.IGui import IGui
from sihd import Core

wx = None

class IWxPythonGui(IGui):

    def __init__(self, app=None, name="IWxPythonGui"):
        global wx
        if wx is None:
            import wx
        super(IWxPythonGui, self).__init__(app=app, name=name)

    def loop(self, **kwargs):
        app = wx.App(False)
        self._app = app
        self._make_frames(app)
        app.MainLoop()

    def _make_frames(self, app):
        raise NotImplemented
